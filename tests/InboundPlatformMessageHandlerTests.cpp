/*
 * Copyright 2020 WolkAbout Technology s.r.o.
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
#ifndef WOLKABOUTCONNECTOR_INBOUNDPLATFORMMESSAGEHANDLERTESTS_CPP
#define WOLKABOUTCONNECTOR_INBOUNDPLATFORMMESSAGEHANDLERTESTS_CPP

#define private public
#define protected public
#include "InboundPlatformMessageHandler.h"
#undef private
#undef protected

#include "mocks/MessageListenerMock.h"

#include <gtest/gtest.h>

#include <iostream>

class InboundPlatformMessageHandlerTests : public ::testing::Test
{
public:
    void SetUp()
    {
        protocolMock.reset(new ::testing::NiceMock<ProtocolMock>());
        messageListenerMock.reset(new ::testing::NiceMock<MessageListenerMock>(*protocolMock));
    }

    void TearDown()
    {
        protocolMock.reset();
        messageListenerMock.reset();
    }

    std::unique_ptr<ProtocolMock> protocolMock;
    std::unique_ptr<MessageListenerMock> messageListenerMock;
};

TEST_F(InboundPlatformMessageHandlerTests, CoveringTest)
{
    const auto& key = "TEST_DEVICE";
    const auto& channels = std::vector<std::string>{"CHANNEL1", "CHANNEL2", "CHANNEL3"};
    const auto& invalidChannel = "NOT_EXISTENT_CHANNEL";
    const auto& testContent = R"({"message":"Hello!"})";

    const auto& messageHandler = std::make_shared<wolkabout::InboundPlatformMessageHandler>(key);
    std::shared_ptr<MessageListenerMock> pointer(messageListenerMock.release());

    EXPECT_CALL(*protocolMock, getInboundChannelsForDevice(key)).WillOnce(testing::Return(channels));

    ASSERT_NO_THROW(messageHandler->addListener(pointer));

    // Won't enter in if, as there is no listeners for this channel.
    // Sadly, there's no mechanism to see if the message I wanted to sent, has a listener that can handle it or not.
    EXPECT_NO_THROW(messageHandler->messageReceived(invalidChannel, testContent));

    // Will enter if
    EXPECT_CALL(*pointer, messageReceived(testing::A<std::shared_ptr<wolkabout::Message>>()))
      .WillOnce(testing::Return());
    EXPECT_NO_THROW(messageHandler->messageReceived(channels[1], testContent));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(channels.size(), messageHandler->getChannels().size());
}

#endif    // WOLKABOUTCONNECTOR_INBOUNDPLATFORMMESSAGEHANDLERTESTS_CPP
