/**
 * @file Log.cpp
 * @brief Log implementation — single Serial line prefix [FLM][LEVEL][Module]
 */
#include "Log.h"
#include <cstdarg>
#include <cstdio>

static char s_buf[192];

void Log::vprint(const char* level, const char* module, const char* fmt, va_list ap) {
    int n = snprintf(s_buf, sizeof(s_buf), "[FLM][%s][%s] ", level, module);
    if (n < 0 || (size_t)n >= sizeof(s_buf)) {
        Serial.print(F("[FLM][?][?] "));
    } else {
        Serial.print(s_buf);
    }
    vsnprintf(s_buf, sizeof(s_buf), fmt, ap);
    s_buf[sizeof(s_buf) - 1] = '\0';
    Serial.println(s_buf);
}

void Log::rawPrint(const char* level, const char* module, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprint(level, module, fmt, ap);
    va_end(ap);
}
