#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <limits>
#include <cstdlib>
#include "MQTT_Callbacks.h"
#include "ThreadPool.h"

using namespace std::chrono_literals;

const char* argp_program_name = "Remys Fast MQTT Logger";
const char* argp_program_version = "1.1";
const char* argp_program_bug_address = "<mqttlog@relst.nl>";

struct cli_arguments
{
    std::string broker, username, password;
    std::string topic = "#";
    std::string facility = "LOG_LOCAL6";
    bool no_log_to_stderr = false;
    std::string ca_file, client_cert, client_key, client_key_password;
};

static char doc[] = "MQTT client that connects to a broker, "
    "subscribes to $TOPIC and logs messages "
    "to syslog $FACILITY, and if asked, to stdout. "
    "Assumes strings, not binary payload. "
    "Options can also be set as ENV vars ";

static argp_option options[] = {
    {"broker", 'b', "BROKER", 0, "MQTT broker URL:port"},
    {"topic", 't', "TOPIC", 0, "Topic to subscribe (default: #)"},
    {"username", 'u', "USERNAME", OPTION_ARG_OPTIONAL, "Username for MQTT broker (optional)"},
    {"password", 'p', "PASSWORD", OPTION_ARG_OPTIONAL, "Password for MQTT broker (optional)"},
    {"facility", 'f', "FACILITY", 0, "Syslog facility to log to (default: LOG_LOCAL6). Must prefix with LOG_"},
    {"no-log-to-stderr", 's', nullptr, 0, "Disable message logging to STDERR. (Default: enabled)"},
    {"ca-file", 'A', "CA_FILE", OPTION_ARG_OPTIONAL, "CA certificate file (optional)"},
    {"client-cert", 'C', "CLIENT_CERT", OPTION_ARG_OPTIONAL, "Client certificate file (optional)"},
    {"client-key", 'K', "CLIENT_KEY", OPTION_ARG_OPTIONAL, "Client private key file (optional)"},
    {"client-key-password", 'P', "CLIENT_KEY_PASSWORD", OPTION_ARG_OPTIONAL, "Client private key password (optional)"},
    {nullptr}
};

static error_t parse_opt(int key, char* arg, argp_state* state)
{
    auto* arguments = static_cast<cli_arguments*>(state->input);
    switch (key)
    {
    case 'b':
        if (arg) arguments->broker = arg;
        break;
    case 't':
        if (arg) arguments->topic = arg;
        break;
    case 'u':
        if (arg) arguments->username = arg;
        break;
    case 'p':
        if (arg) arguments->password = arg;
        break;
    case 'f':
        if (arg) arguments->facility = arg;
        break;
    case 's':
        arguments->no_log_to_stderr = true;
        break;
    case 'A':
        if (arg) arguments->ca_file = arg;
        break;
    case 'C':
        if (arg) arguments->client_cert = arg;
        break;
    case 'K':
        if (arg) arguments->client_key = arg;
        break;
    case 'P':
        if (arg) arguments->client_key_password = arg;
        break;
    case ARGP_KEY_END:
        if (arguments->broker.empty())
        {
            argp_usage(state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

std::mutex stop_mtx;
std::condition_variable stop_cv;
bool stop_signal_received = false;

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        stop_signal_received = true;
        stop_cv.notify_one();
    }
}

int main(int argc, char* argv[])
{
    cli_arguments arguments;

    // Load environment variables as defaults
    if (const char* env_p = std::getenv("BROKER"))
    {
        arguments.broker = env_p;
    }
    if (const char* env_p = std::getenv("USERNAME"))
    {
        arguments.username = env_p;
    }
    if (const char* env_p = std::getenv("PASSWORD"))
    {
        arguments.password = env_p;
    }
    if (const char* env_p = std::getenv("TOPIC"))
    {
        arguments.topic = env_p;
    }
    if (const char* env_p = std::getenv("FACILITY"))
    {
        arguments.facility = env_p;
    }
    if (const char* env_p = std::getenv("CA_FILE"))
    {
        arguments.ca_file = env_p;
    }
    if (const char* env_p = std::getenv("CLIENT_CERT"))
    {
        arguments.client_cert = env_p;
    }
    if (const char* env_p = std::getenv("CLIENT_KEY"))
    {
        arguments.client_key = env_p;
    }
    if (const char* env_p = std::getenv("CLIENT_KEY_PASSWORD"))
    {
        arguments.client_key_password = env_p;
    }
    if (const char* env_p = std::getenv("NO_LOG_TO_STDERR"))
    {
        arguments.no_log_to_stderr = std::string(env_p) == "1";
    }

    // Always parse CLI args; they override env defaults
    argp_parse(&argp, argc, argv, 0, nullptr, &arguments);

    // Validate required args
    if (arguments.broker.empty())
    {
        std::cerr << "Not enough arguments. Use --help.\n";
        return 1;
    }

    // If user provided any TLS client auth arg, require all three
    bool any_tls = !arguments.ca_file.empty() || !arguments.client_cert.empty() || !arguments.client_key.empty();
    bool all_tls = !arguments.ca_file.empty() && !arguments.client_cert.empty() && !arguments.client_key.empty();
    if (any_tls && !all_tls)
    {
        std::cerr << "If using TLS client cert auth you must provide --ca-file, --client-cert and --client-key\n";
        return 1;
    }

    // If TLS requested, ensure files exist and are readable
    auto file_readable = [](const std::string& path) {
        std::ifstream f(path);
        return f.good();
    };

    if (all_tls)
    {
        if (!file_readable(arguments.ca_file))
        {
            std::cerr << "CA file not found or unreadable: " << arguments.ca_file << "\n";
            return 1;
        }
        if (!file_readable(arguments.client_cert))
        {
            std::cerr << "Client certificate not found or unreadable: " << arguments.client_cert << "\n";
            return 1;
        }
        if (!file_readable(arguments.client_key))
        {
            std::cerr << "Client private key not found or unreadable: " << arguments.client_key << "\n";
            return 1;
        }
    }

    ThreadPool thread_pool(std::thread::hardware_concurrency());

    mqtt::async_client client(arguments.broker, "", std::numeric_limits<int>::max());

    auto connOpts = mqtt::connect_options_builder()
                    .automatic_reconnect(1s, 2s)
                    .connect_timeout(30s)
                    .keep_alive_interval(5s)
                    .clean_session(true).finalize();

    if (!arguments.username.empty())
    {
        connOpts.set_user_name(arguments.username);
    }
    if (!arguments.password.empty())
    {
        connOpts.set_password(arguments.password);
    }

    if (all_tls)
    {
        mqtt::ssl_options sslOpts;

        // Log that TLS client certificate auth is enabled. Avoid logging secrets.
        auto basename = [](const std::string& p) -> std::string {
            auto pos = p.find_last_of("/\\");
            if (pos == std::string::npos) return p;
            return p.substr(pos + 1);
        };

        logger.log(LOG_INFO, "TLS client certificate authentication enabled (cert=%s, ca=%s)", basename(arguments.client_cert).c_str(), basename(arguments.ca_file).c_str());

        sslOpts.set_key_store(arguments.client_cert);
        sslOpts.set_private_key(arguments.client_key);
        sslOpts.set_trust_store(arguments.ca_file);

        if (!arguments.client_key_password.empty())
        {
            // Do not log the password
            sslOpts.set_private_key_password(arguments.client_key_password);
        }

        connOpts.set_ssl(sslOpts);
    }

    MQTT_Callbacks cb(client, arguments.topic, logger, thread_pool);
    client.set_callback(cb);

    try
    {
        if (!client.connect(connOpts)->wait_for(30000))
        {
            logger.log(LOG_ERR, "\nTimeout Connecting\n");
            exit(1);
        }
    }
    catch (const std::exception& e)
    {
        logger.log(LOG_ERR, "Error Connecting: %s\n", e.what());
        exit(1);
    }

    return 0;
}
