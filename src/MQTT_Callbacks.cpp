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


#include "MQTT_Callbacks.h"
#include "Logger.h"
#include "ThreadPool.h"

void MQTT_Success_Failure_Logger::on_failure(const mqtt::token& tok)
{
    if (tok.get_message_id() != 0)
        _logger.log(LOG_ERR, "%s failure for token: [%i]\n", _name.c_str(), tok.get_message_id());
    else
        _logger.log(LOG_ERR, "%s failure\n", _name.c_str());
}

void MQTT_Success_Failure_Logger::on_success(const mqtt::token& tok)
{
    if (tok.get_message_id() != 0)
        _logger.log(LOG_INFO, "%s success for token: [%i]\n", _name.c_str(), tok.get_message_id());


    auto top = tok.get_topics();
    if (top && !top->empty()) {
        _logger.log(LOG_INFO, "%s success for topic: [%s]\n", _name.c_str(), (*top)[0].c_str());
    }
}

void MQTT_Callbacks::connected(const std::string& cause)
{
    _logger.log(LOG_INFO, "Connected to MQTT broker '%s'\n", cause.c_str());
    try {
        _logger.log(LOG_INFO, "Subscribing to topic '%s'\n", _topic.c_str());
        _client.subscribe(_topic, 0, nullptr, _subLogger);
    } catch (const mqtt::exception &e) {
        _logger.log(LOG_ERR, "MQTT subscribe failed: %s\n", e.what());
    }
}

void MQTT_Callbacks::connection_lost(const std::string& cause)
{
    _logger.log(LOG_ERR, "MQTT connection lost: %s\n", cause.c_str());
}


void MQTT_Callbacks::on_failure(const mqtt::token& )
{
    _logger.log(LOG_ERR, "MQTT connection attempt failed\n");
}


void MQTT_Callbacks::message_arrived(mqtt::const_message_ptr msg)
{
    if (msg != nullptr) {
        _thread_pool.enqueue([this, msg] {
            _logger.log(LOG_INFO, "topic='%s', qos='%i', retained='%s', msg='%s'",
                        msg->get_topic().c_str(), msg->get_qos(),
                        msg->is_retained() ? "true" : "false",
                        msg->get_payload_ref().c_str());
        });
    }
}
