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

#include "service/connectivity/mqtt/dto/SensorReadingDto.h"
#include "model/SensorReading.h"

#include <string>
#include <utility>

namespace wolkabout
{
SensorReadingDto::SensorReadingDto(const SensorReading& sensorReading)
: m_rtc(sensorReading.getRtc()), m_value(sensorReading.getValue())
{
}

SensorReadingDto::SensorReadingDto(unsigned long long rtc, std::string value) : m_rtc(rtc), m_value(std::move(value)) {}

unsigned long long SensorReadingDto::getRtc() const
{
    return m_rtc;
}

const std::string& SensorReadingDto::getValue() const
{
    return m_value;
}
}