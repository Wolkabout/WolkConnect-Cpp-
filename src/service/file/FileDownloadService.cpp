/*
 * Copyright 2018 WolkAbout Technology s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FileDownloadService.h"

#include "FileDownloader.h"
#include "FileHandler.h"
#include "api/UrlFileDownloader.h"
#include "connectivity/ConnectivityService.h"
#include "model/BinaryData.h"
#include "model/FileDelete.h"
#include "model/FileList.h"
#include "model/FilePacketRequest.h"
#include "model/FileTransferStatus.h"
#include "model/FileUploadAbort.h"
#include "model/FileUploadInitiate.h"
#include "model/FileUploadStatus.h"
#include "model/FileUrlDownloadAbort.h"
#include "model/FileUrlDownloadInitiate.h"
#include "model/FileUrlDownloadStatus.h"
#include "model/Message.h"
#include "protocol/json/JsonDownloadProtocol.h"
#include "repository/FileRepository.h"
#include "utilities/ByteUtils.h"
#include "utilities/FileSystemUtils.h"
#include "utilities/Logger.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utilities/StringUtils.h>

namespace
{
static const size_t FILE_HASH_INDEX = 0;
static const size_t FILE_DOWNLOADER_INDEX = 1;
static const size_t FLAG_INDEX = 2;
}    // namespace

namespace wolkabout
{
FileDownloadService::FileDownloadService(std::string deviceKey, JsonDownloadProtocol& protocol,
                                         std::string fileDownloadDirectory, std::uint64_t maxPacketSize,
                                         ConnectivityService& connectivityService, FileRepository& fileRepository,
                                         std::shared_ptr<UrlFileDownloader> urlFileDownloader)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_fileDownloadDirectory{std::move(fileDownloadDirectory)}
, m_maxPacketSize{maxPacketSize}
, m_connectivityService{connectivityService}
, m_fileRepository{fileRepository}
, m_urlFileDownloader{std::move(urlFileDownloader)}
, m_activeDownload{""}
, m_run{true}
, m_garbageCollector(&FileDownloadService::clearDownloads, this)
{
    if (!FileSystemUtils::isDirectoryPresent(m_fileDownloadDirectory))
    {
        FileSystemUtils::createDirectory(m_fileDownloadDirectory);
    }
}

FileDownloadService::~FileDownloadService()
{
    m_run = false;
    notifyCleanup();

    if (m_garbageCollector.joinable())
    {
        m_garbageCollector.join();
    }
}

void FileDownloadService::messageReceived(std::shared_ptr<wolkabout::Message> message)
{
    assert(message);

    auto binary = m_protocol.makeBinaryData(*message);
    if (binary)
    {
        auto binaryData = *binary;
        addToCommandBuffer([=] { handle(binaryData); });

        return;
    }

    auto uploadInit = m_protocol.makeFileUploadInitiate(*message);
    if (uploadInit)
    {
        auto initiateRequest = *uploadInit;
        addToCommandBuffer([=] { handle(initiateRequest); });

        return;
    }

    auto uploadAbort = m_protocol.makeFileUploadAbort(*message);
    if (uploadAbort)
    {
        auto abortRequest = *uploadAbort;
        addToCommandBuffer([=] { handle(abortRequest); });

        return;
    }

    auto fileDelete = m_protocol.makeFileDelete(*message);
    if (fileDelete)
    {
        auto deleteRequest = *fileDelete;
        addToCommandBuffer([=] { handle(deleteRequest); });

        return;
    }

    if (m_protocol.isFilePurge(*message))
    {
        addToCommandBuffer([=] { purgeFiles(); });

        return;
    }

    auto listConfirmResult = m_protocol.makeFileListConfirm(*message);
    if (listConfirmResult)
    {
        LOG(DEBUG) << "Received file list confirm: " << listConfirmResult->getMessage();
        return;
    }

    auto urlDownloadInit = m_protocol.makeFileUrlDownloadInitiate(*message);
    if (urlDownloadInit)
    {
        auto initiateRequest = *urlDownloadInit;
        addToCommandBuffer([=] { handle(initiateRequest); });

        return;
    }

    auto urlDownloadAbort = m_protocol.makeFileUrlDownloadAbort(*message);
    if (urlDownloadAbort)
    {
        auto abortRequest = *urlDownloadAbort;
        addToCommandBuffer([=] { handle(abortRequest); });

        return;
    }

    LOG(WARN) << "Unable to parse message; channel: " << message->getChannel()
              << ", content: " << message->getContent();
}

const Protocol& FileDownloadService::getProtocol()
{
    return m_protocol;
}

void FileDownloadService::handle(const BinaryData& binaryData)
{
    std::lock_guard<decltype(m_mutex)> lg{m_mutex};

    auto it = m_activeDownloads.find(m_activeDownload);
    if (it == m_activeDownloads.end())
    {
        LOG(WARN) << "Unexpected binary data";
        return;
    }

    std::get<FILE_DOWNLOADER_INDEX>(it->second)->handleData(binaryData);
}

void FileDownloadService::handle(const FileUploadInitiate& request)
{
    if (m_maxPacketSize == 0)
    {
        LOG(WARN) << "File transfer protocol disabled";

        sendStatus(FileUploadStatus{request.getName(), FileTransferError::TRANSFER_PROTOCOL_DISABLED});
        return;
    }

    if (request.getName().empty())
    {
        LOG(WARN) << "Missing file name from file upload initiate";

        sendStatus(FileUploadStatus{request.getName(), FileTransferError::UNSPECIFIED_ERROR});
        return;
    }

    if (request.getSize() == 0)
    {
        LOG(WARN) << "Missing file size from file upload initiate";
        sendStatus(FileUploadStatus{request.getName(), FileTransferError::UNSPECIFIED_ERROR});
        return;
    }

    if (request.getHash().empty())
    {
        LOG(WARN) << "Missing file hash from file upload initiate";
        sendStatus(FileUploadStatus{request.getName(), FileTransferError::UNSPECIFIED_ERROR});
        return;
    }

    auto fileInfo = m_fileRepository.getFileInfo(request.getName());

    if (!fileInfo)
    {
        download(request.getName(), request.getSize(), request.getHash());
    }
    else if (fileInfo->hash != request.getHash())
    {
        sendStatus(FileUploadStatus{request.getName(), FileTransferError::FILE_HASH_MISMATCH});
    }
    else
    {
        sendStatus(FileUploadStatus{request.getName(), FileTransferStatus::FILE_READY});
    }
}

void FileDownloadService::handle(const FileUploadAbort& request)
{
    if (request.getName().empty())
    {
        LOG(WARN) << "Missing file name from file upload abort";

        sendStatus(FileUploadStatus{request.getName(), FileTransferError::UNSPECIFIED_ERROR});
        return;
    }

    abortDownload(request.getName());
}

void FileDownloadService::handle(const FileDelete& request)
{
    if (request.getName().empty())
    {
        LOG(WARN) << "Missing file name from file delete";

        sendFileList();
        return;
    }

    deleteFile(request.getName());
}

void FileDownloadService::handle(const FileUrlDownloadInitiate& request)
{
    if (!m_urlFileDownloader)
    {
        LOG(WARN) << "Url downloader not available";

        sendStatus(FileUrlDownloadStatus{request.getUrl(), FileTransferError::TRANSFER_PROTOCOL_DISABLED});
        return;
    }

    if (request.getUrl().empty())
    {
        LOG(WARN) << "Missing file url from file url download initiate";

        sendStatus(FileUrlDownloadStatus{request.getUrl(), FileTransferError::UNSPECIFIED_ERROR});
        return;
    }

    urlDownload(request.getUrl());
}

void FileDownloadService::handle(const FileUrlDownloadAbort& request)
{
    if (!m_urlFileDownloader)
    {
        LOG(WARN) << "Url downloader not available";

        sendStatus(FileUrlDownloadStatus{request.getUrl(), FileTransferError::TRANSFER_PROTOCOL_DISABLED});
        return;
    }

    if (request.getUrl().empty())
    {
        LOG(WARN) << "Missing file url from file url download abort";

        sendStatus(FileUrlDownloadStatus{request.getUrl(), FileTransferError::UNSPECIFIED_ERROR});
        return;
    }

    abortUrlDownload(request.getUrl());
}

void FileDownloadService::download(const std::string& fileName, uint64_t fileSize, const std::string& fileHash)
{
    std::lock_guard<decltype(m_mutex)> lg{m_mutex};

    auto it = m_activeDownloads.find(fileName);
    if (it != m_activeDownloads.end())
    {
        auto activeHash = std::get<FILE_HASH_INDEX>(m_activeDownloads[fileName]);
        if (activeHash != fileHash)
        {
            LOG(WARN) << "Download already active for file: " << fileName << ", but with different hash";
            // TODO another error
            sendStatus(FileUploadStatus{fileName, FileTransferError::UNSPECIFIED_ERROR});
            return;
        }

        LOG(INFO) << "Download already active for file: " << fileName;
        sendStatus(FileUploadStatus{fileName, FileTransferStatus::FILE_TRANSFER});
        return;
    }

    LOG(INFO) << "Downloading file: " << fileName;
    sendStatus(FileUploadStatus{fileName, FileTransferStatus::FILE_TRANSFER});

    const auto byteHash = ByteUtils::toByteArray(StringUtils::base64Decode(fileHash));

    auto downloader = std::unique_ptr<FileDownloader>(new FileDownloader(m_maxPacketSize));
    m_activeDownloads[fileName] = std::make_tuple(fileHash, std::move(downloader), false);
    m_activeDownload = fileName;

    std::get<FILE_DOWNLOADER_INDEX>(m_activeDownloads[fileName])
      ->download(
        fileName, fileSize, byteHash, m_fileDownloadDirectory,
        [=](const FilePacketRequest& request) { requestPacket(request); },
        [=](const std::string& filePath) { downloadCompleted(fileName, filePath, fileHash); },
        [=](FileTransferError code) { downloadFailed(fileName, code); });
}

void FileDownloadService::urlDownload(const std::string& fileUrl)
{
    LOG(DEBUG) << "FileDownloadService::urlDownload " << fileUrl;

    m_urlFileDownloader->download(
      fileUrl, m_fileDownloadDirectory,
      [=](const std::string& url, const std::string& fileName, const std::string& filePath) {
          urlDownloadCompleted(url, fileName, filePath);
      },
      [=](const std::string& url, FileTransferError errorCode) { urlDownloadFailed(url, errorCode); });
}

void FileDownloadService::abortDownload(const std::string& fileName)
{
    LOG(DEBUG) << "FileDownloadService::abort " << fileName;

    std::lock_guard<decltype(m_mutex)> lg{m_mutex};

    auto it = m_activeDownloads.find(fileName);
    if (it != m_activeDownloads.end())
    {
        LOG(INFO) << "Aborting download for file: " << fileName;
        std::get<FILE_DOWNLOADER_INDEX>(it->second)->abort();
        flagCompletedDownload(fileName);
        // TODO race with completed
        sendStatus(FileUploadStatus{fileName, FileTransferStatus::ABORTED});

        m_activeDownload = "";
    }
    else
    {
        LOG(DEBUG) << "FileDownloadService::abort download not active";
    }
}

void FileDownloadService::abortUrlDownload(const std::string& fileUrl)
{
    LOG(DEBUG) << "FileDownloadService::abortUrlDownload " << fileUrl;

    LOG(INFO) << "Aborting download for file: " << fileUrl;
    m_urlFileDownloader->abort(fileUrl);

    sendStatus(FileUrlDownloadStatus{fileUrl, FileTransferStatus::ABORTED});
}

void FileDownloadService::deleteFile(const std::string& fileName)
{
    LOG(DEBUG) << "FileDownloadService::delete " << fileName;

    auto info = m_fileRepository.getFileInfo(fileName);
    if (!info)
    {
        LOG(WARN) << "File info missing for file: " << fileName << ",  can't delete";
    }

    LOG(INFO) << "Deleting file: " << info->path;
    if (!FileSystemUtils::deleteFile(info->path))
    {
        LOG(ERROR) << "Failed to delete file: " << info->path;
        sendFileList();
        return;
    }

    m_fileRepository.remove(fileName);

    sendFileList();
}

void FileDownloadService::purgeFiles()
{
    LOG(DEBUG) << "FileDownloadService::purge";

    auto fileNames = m_fileRepository.getAllFileNames();
    if (!fileNames)
    {
        LOG(ERROR) << "Failed to fetch file names";
        sendFileList();
        return;
    }

    for (const auto& name : *fileNames)
    {
        auto info = m_fileRepository.getFileInfo(name);
        if (!info)
        {
            LOG(ERROR) << "File info missing for file: " << name << ",  can't delete";
            continue;
        }

        LOG(INFO) << "Deleting file: " << info->path;
        if (!FileSystemUtils::deleteFile(info->path))
        {
            LOG(ERROR) << "Failed to delete file: " << info->path;
            continue;
        }

        m_fileRepository.remove(name);
    }

    sendFileList();
}

void FileDownloadService::sendFileList()
{
    LOG(DEBUG) << "FileDownloadService::sendFileList";

    addToCommandBuffer([=] { sendFileListUpdate(); });
}

void FileDownloadService::sendStatus(const FileUploadStatus& response)
{
    std::shared_ptr<Message> message = m_protocol.makeMessage(m_deviceKey, response);

    if (!message)
    {
        LOG(ERROR) << "Failed to create file upload status";
        return;
    }

    m_connectivityService.publish(message);
}

void FileDownloadService::sendStatus(const FileUrlDownloadStatus& response)
{
    std::shared_ptr<Message> message = m_protocol.makeMessage(m_deviceKey, response);

    if (!message)
    {
        LOG(ERROR) << "Failed to create file url download status";
        return;
    }

    m_connectivityService.publish(message);
}

void FileDownloadService::sendFileListUpdate()
{
    LOG(DEBUG) << "FileDownloadService::sendFileListUpdate";

    auto fileNames = updateFileList();

    std::shared_ptr<Message> message = m_protocol.makeFileListUpdateMessage(m_deviceKey, FileList{fileNames});

    if (!message)
    {
        LOG(ERROR) << "Failed to create file list update";
        return;
    }

    m_connectivityService.publish(message);
}

void FileDownloadService::requestPacket(const FilePacketRequest& request)
{
    std::shared_ptr<Message> message = m_protocol.makeMessage(m_deviceKey, request);

    if (!message)
    {
        LOG(WARN) << "Failed to create file packet request";
        return;
    }

    m_connectivityService.publish(message);
}

void FileDownloadService::downloadCompleted(const std::string& fileName, const std::string& filePath,
                                            const std::string& fileHash)
{
    flagCompletedDownload(fileName);

    addToCommandBuffer([=] {
        m_fileRepository.store(FileInfo{fileName, fileHash, filePath});
        sendStatus(FileUploadStatus{fileName, FileTransferStatus::FILE_READY});
    });

    sendFileList();
}

void FileDownloadService::downloadFailed(const std::string& fileName, FileTransferError errorCode)
{
    flagCompletedDownload(fileName);

    sendStatus(FileUploadStatus{fileName, errorCode});

    sendFileList();
}

void FileDownloadService::urlDownloadCompleted(const std::string& fileUrl, const std::string& fileName,
                                               const std::string& filePath)
{
    addToCommandBuffer([=] {
        ByteArray fileContent;
        if (!FileSystemUtils::readBinaryFileContent(filePath, fileContent))
        {
            LOG(ERROR) << "Failed to open downloaded file: " << filePath;
            FileSystemUtils::deleteFile(filePath);
            sendStatus(FileUrlDownloadStatus{fileUrl, FileTransferError::FILE_SYSTEM_ERROR});
            return;
        }

        auto byteHash = ByteUtils::hashSHA256(fileContent);
        auto hashStr = StringUtils::base64Encode(byteHash);

        m_fileRepository.store(FileInfo{fileName, hashStr, filePath});
        sendStatus(FileUrlDownloadStatus{fileUrl, fileName});
    });

    sendFileList();
}

void FileDownloadService::urlDownloadFailed(const std::string& fileUrl, FileTransferError errorCode)
{
    sendStatus(FileUrlDownloadStatus{fileUrl, errorCode});

    sendFileList();
}

void FileDownloadService::addToCommandBuffer(std::function<void()> command)
{
    m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(command));
}

void FileDownloadService::flagCompletedDownload(const std::string& key)
{
    std::lock_guard<decltype(m_mutex)> lg{m_mutex};

    auto it = m_activeDownloads.find(key);
    if (it != m_activeDownloads.end())
    {
        std::get<FLAG_INDEX>(it->second) = true;
    }

    notifyCleanup();
}

void FileDownloadService::notifyCleanup()
{
    m_condition.notify_one();
}

std::vector<std::string> FileDownloadService::updateFileList()
{
    std::vector<std::tuple<std::string, std::unique_ptr<ByteArray>>> newFilesOnDisk;
    std::vector<std::string> filesMissingOnDisk;
    std::vector<std::string> allValidFiles;

    auto filesOnDisk = FileSystemUtils::listFiles(m_fileDownloadDirectory);
    auto filesInRepo = m_fileRepository.getAllFileNames();

    if (!filesInRepo)
    {
        LOG(ERROR) << "Failed to fetch file names";
        // Just return whatever is on the disk
        return filesOnDisk;
    }

    for (const auto& repoFile : *filesInRepo)
    {
        auto it = std::find(filesOnDisk.begin(), filesOnDisk.end(), repoFile);
        if (it == filesOnDisk.end())
        {
            LOG(WARN) << "File missing on disk: " << repoFile;
            filesMissingOnDisk.push_back(repoFile);
        }
        else
        {
            allValidFiles.push_back(repoFile);
        }
    }

    for (const auto& diskFile : filesOnDisk)
    {
        auto it = std::find(filesInRepo->begin(), filesInRepo->end(), diskFile);
        if (it == filesInRepo->end())
        {
            std::unique_ptr<ByteArray> fileContent(new ByteArray());
            bool result = FileSystemUtils::readBinaryFileContent(
              FileSystemUtils::composePath(diskFile, m_fileDownloadDirectory), *fileContent);

            if (result)
            {
                LOG(INFO) << "Found new file on disk: " << diskFile;
                newFilesOnDisk.push_back({diskFile, std::move(fileContent)});
                allValidFiles.push_back(diskFile);
            }
            else
            {
                LOG(WARN) << "Found new file on disk: " << diskFile << ", but unable to open";
            }
        }
        else
        {
            allValidFiles.push_back(diskFile);
        }
    }

    // remove missing files from repo
    for (const auto& missingFile : filesMissingOnDisk)
    {
        m_fileRepository.remove(missingFile);
    }

    // add new files to repo
    for (auto& newFile : newFilesOnDisk)
    {
        auto byteHash = ByteUtils::hashSHA256(*(std::get<1>(newFile)));
        auto hashStr = StringUtils::base64Encode(byteHash);

        auto fileName = std::get<0>(newFile);

        auto path = FileSystemUtils::composePath(fileName, m_fileDownloadDirectory);

        FileInfo info{fileName, hashStr, FileSystemUtils::absolutePath(path)};

        m_fileRepository.store(info);
    }

    // Sort and remove all duplicates
    std::sort(allValidFiles.begin(), allValidFiles.end());
    allValidFiles.erase(std::unique(allValidFiles.begin(), allValidFiles.end()), allValidFiles.end());
    return allValidFiles;
}

void FileDownloadService::clearDownloads()
{
    while (m_run)
    {
        std::unique_lock<decltype(m_mutex)> lg{m_mutex};

        for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end();)
        {
            auto& tuple = it->second;
            auto& downloadCompleted = std::get<FLAG_INDEX>(tuple);

            if (downloadCompleted)
            {
                LOG(DEBUG) << "Removing completed download on channel: " << it->first;
                // removed flagged messages
                it = m_activeDownloads.erase(it);
            }
            else
            {
                ++it;
            }
        }

        lg.unlock();

        static std::mutex cvMutex;
        std::unique_lock<std::mutex> lock{cvMutex};
        m_condition.wait(lock);
    }
}
}    // namespace wolkabout
