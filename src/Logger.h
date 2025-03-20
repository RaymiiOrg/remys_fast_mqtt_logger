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

#include <string>

#ifdef HAVE_SYSLOG
#include <syslog.h>
#else
#define LOG_EMERG   0   /* system is unusable */
#define LOG_ALERT   1   /* action must be taken immediately */
#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERR     3   /* error conditions */
#define LOG_WARNING 4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition */
#define LOG_INFO    6   /* informational */
#define LOG_DEBUG   7   /* debug-level messages */
#endif


class Logger {
public:
    Logger(const char *ident, int facility, bool no_log_to_stderr);
    ~Logger();

    void log(int priority, const char *format, ...) const __attribute__((format(printf, 3, 4)));
    static int getFacilityFromString(const std::string &facility);

};

