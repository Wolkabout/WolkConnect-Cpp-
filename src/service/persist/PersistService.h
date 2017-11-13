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

#ifndef PERSISTSERVICE_H
#define PERSISTSERVICE_H

#include <memory>
#include <string>

namespace wolkabout
{
class Reading;
/**
 * @brief The PersistService class defines interface which custom persistence mechanism should implement
 */
class PersistService
{
public:
    /**
     * @brief Constructor
     * @param persistPath Relative, or absolute, path to directory to be used with persistence implementation
     * @param maximumNumberOfPersistedItems This many items will be kept in persistence.<br>
     *                                      Passing N will limit persistence to keep N sensor readings, and N alarms,
     *                                      Actuator statuses are not limited by this parameter, since ActuatorStatus-es
     *                                      do not have history latest actuator statuses are persisted always
     * @param isCircular True if persistence should act as circular buffer, eg. overwrite oldest sensor reaading or
     * alarm when maximumNumberOfPersistedItems is reached
     */
    PersistService(std::string persistPath = "", unsigned long long int maximumNumberOfPersistedItems = 0,
                   bool isCircular = false);

    /**
     * @brief Destructor
     */
    virtual ~PersistService() = default;

    /**
     * @brief Check whether persisted reading(s) exist
     * @return true if there are persisted readings
     */
    virtual bool hasPersistedReadings() = 0;

    /**
     * @brief Persists reading
     * @param reading to be persisted
     */
    virtual void persist(std::shared_ptr<Reading> reading) = 0;

    /**
     * @brief Unpersists reading
     * @return If reading is unpersisted successfully std::shared_ptr<Reading>, otherwise nullptr
     */
    virtual std::shared_ptr<Reading> unpersistFirst() = 0;

    /**
     * @brief Removes first reading from persistence
     */
    virtual void dropFirst() = 0;

    /**
     * @brief getMaximumNumberOfPersistedReadings Returns maximum number of persisted readings
     * @return Maximum number of reading items
     */
    unsigned long long int getMaximumNumberOfPersistedReadings() const;

    /**
     * @brief isCircular Returns whether persistence acts as circular
     * @return true if persistence acts as circular buffer, eg. overwrites oldest when maximumNumberOfPersistedItems is
     * reached
     */
    bool isCircular() const;

    /**
     * @brief Returns path to directory used by persistence
     * @return çonst std::string& containing path to directory used by persistence
     */
    const std::string& getPersistPath() const;

private:
    unsigned long long int m_maximumNumberOfPersistedReadings;
    bool m_isCircular;
    std::string m_persistPath;
};
}

#endif