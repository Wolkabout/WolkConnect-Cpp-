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

#include "service/connectivity/mqtt/dto/ActuatorStatusDto.h"

#include <string>
#include <utility>

namespace wolkabout
{
ActuatorStatusDto::ActuatorStatusDto(ActuatorStatus actuatorStatus)
: m_state(actuatorStatus.getState()), m_value(actuatorStatus.getValue())
{
}

ActuatorStatusDto::ActuatorStatusDto(ActuatorStatus::State state, std::string value)
: m_state(state), m_value(std::move(value))
{
}

ActuatorStatus::State ActuatorStatusDto::getState() const
{
    return m_state;
}

const std::string& ActuatorStatusDto::getValue() const
{
    return m_value;
}
}
