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

#include <iostream>
#include <cstring>
#include <argp.h>
#include <csignal>
#include <condition_variable>
#include <mutex>
#include <mqtt/async_client.h>
#include <mqtt/connect_options.h>
#include <chrono>
using namespace std::chrono_literals;


#include "Logger.h"
#include "MQTT_Callbacks.h"
#include "ThreadPool.h"

const char *argp_program_name = "Remys Fast MQTT Logger";
const char *argp_program_version = "1.0";
const char *argp_program_bug_address = "<mqttlog@relst.nl>";

struct cli_arguments {
    std::string broker, username, password;
    std::string topic = "#";
    std::string facility = "LOG_LOCAL6";
    bool no_log_to_stderr = false;
};

static char doc[] = "MQTT client that connects to a broker, "
        "subscribes to $TOPIC and logs messages "
        "to syslog $FACILITY, and if asked, to stdout."
        "Assumes strings, not binary payload.";

static argp_option options[] = {
    {"broker", 'b', "BROKER", 0, "MQTT broker URL:port"},
    {"topic", 't', "TOPIC", 0, "Topic to subscribe (default: #)"},
    {"username", 'u', "USERNAME", 0, "Username for MQTT broker (optional)"},
    {"password", 'p', "PASSWORD", 0, "Password for MQTT broker (optional)"},
    {"facility", 'f', "FACILITY", 0, "Syslog facility to log to (default: LOG_LOCAL6). Must prefix with LOG_"},
    {"no-log-to-stderr", 's', nullptr, 0, "Disabled message logging to STDERR. (Default: enabled)"},
    {nullptr}
};

static error_t parse_opt(int key, char *arg, argp_state *state) {
    auto *arguments = static_cast<cli_arguments *>(state->input);
    switch (key) {
        case 'b':
            arguments->broker = arg;
            break;
        case 't':
            arguments->topic = arg;
            break;
        case 'u':
            arguments->username = arg;
            break;
        case 'p':
            arguments->password = arg;
            break;
        case 'f':
            arguments->facility = arg;
            break;
        case 's':
            arguments->no_log_to_stderr = true;
            break;
        case ARGP_KEY_END:
            if (arguments->broker.empty()) {
                argp_usage(state);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static argp argp = {options, parse_opt, nullptr, doc};

std::mutex stop_mtx;
std::condition_variable stop_cv;
bool stop_signal_received = false;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        stop_signal_received = true;
        stop_cv.notify_one();
    }
}

int main(int argc, char *argv[]) {
    cli_arguments arguments;

    if (const char *env_p = std::getenv("BROKER")) {
        arguments.broker = env_p;
    }

    if (const char *env_p = std::getenv("USERNAME")) {
        arguments.username = env_p;
    }

    if (const char *env_p = std::getenv("PASSWORD")) {
        arguments.password = env_p;
    }

    if (const char *env_p = std::getenv("TOPIC")) {
        arguments.topic = env_p;
    }

    if (const char *env_p = std::getenv("FACILITY")) {
        arguments.facility = env_p;
    }

    if (const char *env_p = std::getenv("NO_LOG_TO_STDERR")) {
        arguments.no_log_to_stderr = std::string(env_p) == "1";
    }
    argp_parse(&argp, argc, argv, 0, nullptr, &arguments);

    ThreadPool thread_pool(std::thread::hardware_concurrency());

    auto syslogFacility = Logger::getFacilityFromString(arguments.facility);
    Logger logger = Logger(argp_program_name, syslogFacility, arguments.no_log_to_stderr);

    logger.log(LOG_INFO, "Started Remys Fast MQTT Logger by Remy van Elst, raymii.org, AGPLv3");

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#ifndef HAVE_SYSLOG
    fprintf(stderr, "<syslog.h> not found. Only printing to STDOUT!");
#endif

    // large buffer size to combat: MQTT error [-12]: No more messages can be buffered
    mqtt::async_client client(arguments.broker, "", std::numeric_limits<int>::max());

    auto connOpts = mqtt::connect_options_builder()
            .automatic_reconnect(1s, 2s)
            .connect_timeout(30s)
            .keep_alive_interval(5s)
            .clean_session(true).finalize();

    if (!arguments.username.empty()) {
        connOpts.set_user_name(arguments.username);
    }
    if (!arguments.password.empty()) {
        connOpts.set_password(arguments.password);
    }

    MQTT_Callbacks cb(client, arguments.topic, logger, thread_pool);
    client.set_callback(cb);

    try {
        if (!client.connect(connOpts)->wait_for(30000)) {
            logger.log(LOG_ERR, "Timeout Connecting\n");
            exit(1);
        }
    } catch (const std::exception &e) {
        logger.log(LOG_ERR, "Error Connecting: %s\n", e.what());
        exit(1);
    }


    std::unique_lock lock(stop_mtx);
    stop_cv.wait(lock, [] { return stop_signal_received; });

    client.disconnect()->wait();
    logger.log(LOG_INFO, "Stopping Remys Fast MQTT Logger by Remy van Elst, raymii.org, AGPLv3");
    return 0;
}
