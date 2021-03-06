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

#ifndef WOLK_H
#define WOLK_H

#include "WolkBuilder.h"
#include "connectivity/ConnectivityService.h"
#include "model/ActuatorStatus.h"
#include "model/Device.h"
#include "utilities/CommandBuffer.h"
#include "utilities/StringUtils.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class ActuationHandler;
class ActuatorStatusProvider;
class ConfigurationHandler;
class ConfigurationProvider;
class ConfigurationSetCommand;
class ConnectivityService;
class DataProtocol;
class DataService;
class FileDownloadService;
class FileRepository;
class FirmwareUpdateService;
class InboundMessageHandler;
class JsonDFUProtocol;
class JsonDownloadProtocol;
class KeepAliveService;
class StatusProtocol;

class Wolk
{
    friend class WolkBuilder;

public:
    virtual ~Wolk();

    /**
     * @brief Initiates wolkabout::WolkBuilder that configures device to connect to WolkAbout IoT Cloud
     * @param device wolkabout::Device
     * @return wolkabout::WolkBuilder instance
     */
    static WolkBuilder newBuilder(Device device);

    /**
     * @brief Publishes sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param value Sensor value<br>
     *              Supported types:<br>
     *               - bool<br>
     *               - float<br>
     *               - double<br>
     *               - signed int<br>
     *               - signed long int<br>
     *               - signed long long int<br>
     *               - unsigned int<br>
     *               - unsigned long int<br>
     *               - unsigned long long int<br>
     *               - string<br>
     *               - char*<br>
     *               - const char*<br>
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    template <typename T> void addSensorReading(const std::string& reference, T value, unsigned long long int rtc = 0);

    /**
     * @brief Publishes sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param value Sensor value
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addSensorReading(const std::string& reference, std::string value, unsigned long long int rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param values Multi-value sensor values<br>
     *              Supported types:<br>
     *               - bool<br>
     *               - float<br>
     *               - double<br>
     *               - signed int<br>
     *               - signed long int<br>
     *               - signed long long int<br>
     *               - unsigned int<br>
     *               - unsigned long int<br>
     *               - unsigned long long int<br>
     *               - string<br>
     *               - char*<br>
     *               - const char*<br>
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    template <typename T>
    void addSensorReading(const std::string& reference, std::initializer_list<T> values,
                          unsigned long long int rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param values Multi-value sensor values<br>
     *              Supported types:<br>
     *               - bool<br>
     *               - float<br>
     *               - double<br>
     *               - signed int<br>
     *               - signed long int<br>
     *               - signed long long int<br>
     *               - unsigned int<br>
     *               - unsigned long int<br>
     *               - unsigned long long int<br>
     *               - string<br>
     *               - char*<br>
     *               - const char*<br>
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    template <typename T>
    void addSensorReading(const std::string& reference, const std::vector<T> values, unsigned long long int rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param values Multi-value sensor values
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addSensorReading(const std::string& reference, const std::vector<std::string> values,
                          unsigned long long int rtc = 0);

    /**
     * @brief Publishes alarm to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Alarm reference
     * @param active Is alarm active or not
     * @param rtc POSIX time at which event occurred - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addAlarm(const std::string& reference, bool active, unsigned long long int rtc = 0);

    /**
     * @brief Invokes ActuatorStatusProvider to obtain actuator status, and the publishes it.<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param Actuator reference
     */
    void publishActuatorStatus(const std::string& reference);

    /**
     * @brief Invokes ConfigurationProvider to obtain device configuration, and the publishes it.<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     */
    void publishConfiguration();

    /**
     * @brief Invokes keepAliveServices method of fetching the last received timestamp in pong.
     */
    long long getLastTimestamp();

    /**
     * @brief Establishes connection with WolkAbout IoT platform
     */
    void connect();

    /**
     * @brief Disconnects from WolkAbout IoT platform
     */
    void disconnect();

    /**
     * @brief Publishes data
     */
    void publish();

private:
    class ConnectivityFacade;

    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;
    static const constexpr std::chrono::seconds KEEP_ALIVE_INTERVAL{60};

    Wolk(Device device);

    void addToCommandBuffer(std::function<void()> command);

    static unsigned long long int currentRtc();

    void flushActuatorStatuses();
    void flushAlarms();
    void flushSensorReadings();
    void flushConfiguration();

    void handleActuatorSetCommand(const std::string& reference, const std::string& value);
    void handleActuatorGetCommand(const std::string& reference);

    void handleConfigurationSetCommand(const ConfigurationSetCommand& command);
    void handleConfigurationGetCommand();

    void publishFirmwareStatus();
    void publishFileList();

    void tryConnect(bool firstTime = false);
    void notifyConnected();
    void notifyDisonnected();

    Device m_device;

    std::unique_ptr<DataProtocol> m_dataProtocol;
    std::unique_ptr<StatusProtocol> m_statusProtocol;
    std::unique_ptr<JsonDownloadProtocol> m_fileDownloadProtocol;
    std::unique_ptr<JsonDFUProtocol> m_firmwareUpdateProtocol;

    std::unique_ptr<ConnectivityService> m_connectivityService;
    std::shared_ptr<Persistence> m_persistence;

    std::unique_ptr<InboundMessageHandler> m_inboundMessageHandler;

    std::shared_ptr<ConnectivityFacade> m_connectivityManager;

    std::shared_ptr<DataService> m_dataService;

    std::shared_ptr<FileRepository> m_fileRepository;

    std::shared_ptr<FileDownloadService> m_fileDownloadService;
    std::shared_ptr<FirmwareUpdateService> m_firmwareUpdateService;

    std::shared_ptr<KeepAliveService> m_keepAliveService;

    std::function<void(std::string, std::string)> m_actuationHandlerLambda;
    std::weak_ptr<ActuationHandler> m_actuationHandler;

    std::function<ActuatorStatus(std::string)> m_actuatorStatusProviderLambda;
    std::weak_ptr<ActuatorStatusProvider> m_actuatorStatusProvider;

    std::function<void(const std::vector<ConfigurationItem>& configuration)> m_configurationHandlerLambda;
    std::weak_ptr<ConfigurationHandler> m_configurationHandler;

    std::function<std::vector<ConfigurationItem>()> m_configurationProviderLambda;
    std::weak_ptr<ConfigurationProvider> m_configurationProvider;

    std::unique_ptr<CommandBuffer> m_commandBuffer;

    class ConnectivityFacade : public ConnectivityServiceListener
    {
    public:
        ConnectivityFacade(InboundMessageHandler& handler, std::function<void()> connectionLostHandler);

        void messageReceived(const std::string& channel, const std::string& message) override;
        void connectionLost() override;
        std::vector<std::string> getChannels() const override;

    private:
        InboundMessageHandler& m_messageHandler;
        std::function<void()> m_connectionLostHandler;
    };
};

template <typename T> void Wolk::addSensorReading(const std::string& reference, T value, unsigned long long rtc)
{
    addSensorReading(reference, StringUtils::toString(value), rtc);
}

template <typename T>
void Wolk::addSensorReading(const std::string& reference, std::initializer_list<T> values, unsigned long long int rtc)
{
    addSensorReading(reference, std::vector<T>(values), rtc);
}

template <typename T>
void Wolk::addSensorReading(const std::string& reference, const std::vector<T> values, unsigned long long int rtc)
{
    std::vector<std::string> stringifiedValues(values.size());
    std::transform(values.cbegin(), values.cend(), stringifiedValues.begin(),
                   [&](const T& value) -> std::string { return StringUtils::toString(value); });

    addSensorReading(reference, stringifiedValues, rtc);
}
}    // namespace wolkabout

#endif
