/*
* Copyright (c) 2025 Remy van Elst
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Afferro General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once
#include <mqtt/async_client.h>

class ThreadPool;
class Logger;


class MQTT_Success_Failure_Logger : public virtual mqtt::iaction_listener
{
public:
    MQTT_Success_Failure_Logger(const std::string& name, Logger& logger) :
        _name(name), _logger(logger)
    { }

    void on_failure(const mqtt::token &tok) override;
    void on_success(const mqtt::token &tok) override;

private:
    std::string _name;
    Logger& _logger;
};


class MQTT_Callbacks : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
public:
    MQTT_Callbacks(mqtt::async_client& client,
        const std::string& topic,
        Logger& logger,
        ThreadPool& threadpool) :
            _client(client), _topic(topic),
            _logger(logger), _thread_pool(threadpool),
            _subLogger("Subscribe", _logger)
    { }

    void connected(const std::string &cause) override;
    void connection_lost(const std::string &cause) override;
    void on_failure(const mqtt::token &tok) override;
    void on_success(const mqtt::token&) override {};
    void message_arrived(mqtt::const_message_ptr msg) override;

private:
    mqtt::async_client& _client;
    std::string _topic;
    Logger& _logger;
    ThreadPool &_thread_pool;
    MQTT_Success_Failure_Logger _subLogger;
};