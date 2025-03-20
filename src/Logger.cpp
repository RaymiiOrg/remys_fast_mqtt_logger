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

#include "Logger.h"
#include <map>
#include <cstdarg>

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif


Logger::Logger(const char *ident, int facility, bool no_log_to_stderr)
{
#ifdef HAVE_SYSLOG
    int log_options = LOG_PID | LOG_CONS;
    if (!no_log_to_stderr) {
        log_options |= LOG_PERROR;
    }
    openlog(ident, log_options, facility);
#else
    (void)ident;
    (void)facility;
    (void)log_to_stdout;
#endif
}

Logger::~Logger() {
#ifdef HAVE_SYSLOG
    closelog();
#endif
}

void Logger::log(int priority, const char *format, ...) const
{
#ifdef HAVE_SYSLOG
    va_list args;
    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
#else
    printf("topic='%s', qos='%i', retained='%s', msg='%s'\n",
        msg->get_topic().c_str(), msg->get_qos(), msg->is_retained() ? "true" : "false", msg->get_payload_ref().c_str());
#endif
}


int Logger::getFacilityFromString(const std::string &facility) {
    static const std::map<std::string, int> facilityMap = {
#ifdef HAVE_SYSLOG
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
#endif
    };

    auto it = facilityMap.find(facility);
    return (it != facilityMap.end()) ? it->second : -1;
}