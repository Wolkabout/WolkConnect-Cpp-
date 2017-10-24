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

#ifndef WOLK_H
#define WOLK_H

#include "ActuationHandler.h"
#include "ActuatorCommand.h"
#include "ActuatorStatusProvider.h"
#include "ConnectivityService.h"
#include "Device.h"
#include "PublishingService.h"
#include "ReadingsBuffer.h"
#include "WolkBuilder.h"

#include <functional>
#include <memory>
#include <string>

namespace wolkabout
{
class Wolk
{
    friend class WolkBuilder;

public:
    virtual ~Wolk() = default;

    /**
     * @brief Initiates wolkabout::WolkBuilder that configures device to connect to WolkAbout IoT Cloud
     * @return wolkabout::WolkBuilder instance
     */
    static WolkBuilder newBuilder();

    /**
     * @brief Publishes sensor reading to WolkAbout IoT Cloud
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
     * @brief Publishes alarm to WolkAbout IoT Cloud
     * @param reference Alarm reference
     * @param value Alarm value
     * @param rtc POSIX time at which event occurred - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addAlarm(const std::string& reference, const std::string& value, unsigned long long int rtc = 0);

    /**
     * @brief Invokes ActuatorStatusProvider callback to obtain actuator status
     * @param Actuator reference
     */
    void publishActuatorStatus(const std::string& reference);

    /**
     * @brief connect Establishes connection with WolkAbout IoT platform
     */
    void connect();

    /**
     * @brief disconnect Disconnects from WolkAbout IoT platform
     */
    void disconnect();

private:
    class ConnectivityServiceListenerImpl : public ConnectivityServiceListener
    {
    public:
        ConnectivityServiceListenerImpl(Wolk& wolk);
        virtual ~ConnectivityServiceListenerImpl() = default;

        void actuatorCommandReceived(const ActuatorCommand& actuatorCommand, const std::string& reference) override;

    private:
        Wolk& m_wolk;
    };

    Wolk(std::shared_ptr<ConnectivityService> publisher, Device device);

    void addActuatorStatus(const std::string& reference, const ActuatorStatus& actuatorStatus);

    void handleActuatorCommand(const ActuatorCommand& actuatorCommand, const std::string& reference);

    void handleSetActuator(const ActuatorCommand& actuatorCommand, const std::string& reference);

    static unsigned long long int currentRtc();

    Device m_device;

    std::function<void(std::string, std::string)> m_actuationHandlerLambda;
    std::weak_ptr<ActuationHandler> m_actuationHandler;

    std::function<ActuatorStatus(std::string)> m_actuatorStatusProviderLambda;
    std::weak_ptr<ActuatorStatusProvider> m_actuatorStatusProvider;

    std::shared_ptr<ConnectivityService> m_connectivityService;
    std::shared_ptr<ConnectivityServiceListenerImpl> m_ConnectivityServiceListener;

    std::unique_ptr<PublishingService> m_publishingService;

    std::shared_ptr<ReadingBuffer> m_readingsBuffer;
};
}

#endif
