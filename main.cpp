#include <iostream>
#include <cstring>
#include <cstdarg>
#include <mqtt/async_client.h>
#include <thread>
#include <argp.h>
#include <syslog.h>
#include <csignal>
#include <condition_variable>
#include <mutex>

const char *argp_program_name = "Remys Fast MQTT Logger";
const char *argp_program_version = "1.0";
const char *argp_program_bug_address = "<mqttlog@relst.nl>";

struct cli_arguments {
    std::string broker, username, password;
    std::string topic = "#";
    std::string facility = "LOG_LOCAL6";
    bool no_log_to_stdout = false;
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
    {"no-log-to-stdout", 's', nullptr, 0, "Disabled message logging to stdout. (Default: enabled)"},
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
            arguments->no_log_to_stdout = true;
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


int getFacilityFromString(const std::string& facility) {
    static const std::map<std::string, int> facilityMap = {
        {"LOG_AUTH", LOG_AUTH},
        {"LOG_AUTHPRIV", LOG_AUTHPRIV},
        {"LOG_CRON", LOG_CRON},
        {"LOG_DAEMON", LOG_DAEMON},
        {"LOG_FTP", LOG_FTP},
        {"LOG_KERN", LOG_KERN},
        {"LOG_LPR", LOG_LPR},
        {"LOG_MAIL", LOG_MAIL},
        {"LOG_NEWS", LOG_NEWS},
        {"LOG_SYSLOG", LOG_SYSLOG},
        {"LOG_USER", LOG_USER},
        {"LOG_UUCP", LOG_UUCP},
        {"LOG_LOCAL0", LOG_LOCAL0},
        {"LOG_LOCAL1", LOG_LOCAL1},
        {"LOG_LOCAL2", LOG_LOCAL2},
        {"LOG_LOCAL3", LOG_LOCAL3},
        {"LOG_LOCAL4", LOG_LOCAL4},
        {"LOG_LOCAL5", LOG_LOCAL5},
        {"LOG_LOCAL6", LOG_LOCAL6},
        {"LOG_LOCAL7", LOG_LOCAL7}
    };

    auto it = facilityMap.find(facility);
    return (it != facilityMap.end()) ? it->second : -1;
}

class SysLogger {
    public:
    explicit SysLogger(const char* ident, int facility) {
        openlog(ident, LOG_PID | LOG_CONS, facility);
    }

    ~SysLogger() {
        closelog();
    }

    void log(int priority, const char* format, ...) const __attribute__((format(printf, 3, 4)))
    {
        va_list args;
        va_start(args, format);
        vsyslog(priority, format, args);
        va_end(args);
    }
};

class mqtt_log_msg_callback : public virtual mqtt::callback {
public:
    mqtt_log_msg_callback(mqtt::async_client &client,
        const SysLogger& logger,
        bool no_log_to_stdout) :
            _client(client),
            _logger(logger),
            _no_log_to_stdout(no_log_to_stdout)
    {
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::thread([this, msg] {
            if (msg == nullptr) {
                return;
            }

            _logger.log(LOG_INFO, "topic='%s', qos='%i', retained='%s', msg='%s'\n",
                msg->get_topic().c_str(), msg->get_qos(), msg->is_retained() ? "true" : "false", msg->get_payload_ref().c_str());

            if (!_no_log_to_stdout) {
                printf("topic='%s', qos='%i', retained='%s', msg='%s'\n",
                msg->get_topic().c_str(), msg->get_qos(), msg->is_retained() ? "true" : "false", msg->get_payload_ref().c_str());
            }

        }).detach();
    }
private:
    mqtt::async_client &_client;
    const SysLogger& _logger;
    bool _no_log_to_stdout =  false;
};


std::mutex mtx;
std::condition_variable cv;
bool stop_signal_received = false;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        stop_signal_received = true;
        cv.notify_one();
    }
}

int main(int argc, char *argv[]) {
    cli_arguments arguments;

    if (const char* env_p = std::getenv("BROKER")) {
        arguments.broker = env_p;
    }

    if (const char* env_p = std::getenv("USERNAME")) {
        arguments.username = env_p;
    }

    if (const char* env_p = std::getenv("PASSWORD")) {
        arguments.password = env_p;
    }

    if (const char* env_p = std::getenv("TOPIC")) {
        arguments.topic = env_p;
    }

    if (const char* env_p = std::getenv("FACILITY")) {
        arguments.facility = env_p;
    }

    if (const char* env_p = std::getenv("NO_LOG_TO_STDOUT")) {
        arguments.no_log_to_stdout = std::string(env_p) == "1";
    }
    argp_parse(&argp, argc, argv, 0, nullptr, &arguments);

    auto syslogFacility = getFacilityFromString(arguments.facility);
    SysLogger logger = SysLogger(argp_program_name, syslogFacility);

    logger.log(LOG_INFO, "Started Remys Fast MQTT Logger by Remy van Elst, raymii.org, AGPLv3");

    // large buffer size to combat: MQTT error [-12]: No more messages can be buffered
    mqtt::async_client client(arguments.broker, "", std::numeric_limits<int>::max());
    mqtt::connect_options connOpts;

    if (!arguments.username.empty()) {
        connOpts.set_user_name(arguments.username);
    }
    if (!arguments.password.empty()) {
        connOpts.set_password(arguments.password);
    }

    mqtt_log_msg_callback cb(client, logger, arguments.no_log_to_stdout);
    client.set_callback(cb);
    try
        {
        client.connect(connOpts)->wait();
        client.subscribe(arguments.topic, 1)->wait();
    }
    catch (const std::exception& e)
        {
        fprintf(stderr, "Error connecting: %s\n", e.what());
        logger.log(LOG_INFO, "Error Connecting: %s\n", e.what());
        exit(1);
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::unique_lock lock(mtx);
    cv.wait(lock, [] { return stop_signal_received; });

    client.disconnect()->wait();
    logger.log(LOG_INFO, "Stopping Remys Fast MQTT Logger by Remy van Elst, raymii.org, AGPLv3");
    return 0;
}
