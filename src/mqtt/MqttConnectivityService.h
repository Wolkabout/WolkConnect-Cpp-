/*
 * Copyright 2017 WolkAbout Technology s.r.o.
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

#ifndef MQTTCONNECTIVITYSERVICE_H
#define MQTTCONNECTIVITYSERVICE_H

#include "ConnectivityService.h"
#include "Device.h"
#include "MqttClient.h"
#include "PahoMqttClient.h"
#include "Reading.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class MqttConnectivityService : public ConnectivityService
{
public:
    MqttConnectivityService(std::shared_ptr<MqttClient> mqttClient, Device device, std::string host);
    virtual ~MqttConnectivityService() = default;

    bool connect() override;
    void disconnect() override;

    bool isConnected() override;

    bool publish(std::shared_ptr<Reading> reading) override;

private:
    class ReadingPublisherVisitor : public ReadingVisitor
    {
    public:
        ReadingPublisherVisitor(MqttClient& mqttClient, Device& device, bool& isPublished)
        : m_mqttClient(mqttClient), m_device(device), m_isPublished(isPublished)
        {
        }

        ~ReadingPublisherVisitor() = default;

        void visit(ActuatorStatus& actuatorStatus) override;
        void visit(Alarm& event) override;
        void visit(SensorReading& sensorReading) override;

    private:
        MqttClient& m_mqttClient;
        Device& m_device;
        bool& m_isPublished;
    };

    Device m_device;
    std::string m_host;

    std::vector<std::string> m_subscriptionList;

    std::atomic_bool m_connected;

    std::shared_ptr<MqttClient> m_mqttClient;

    static const constexpr char* TRUST_STORE = "ca.crt";
};
}

#endif
