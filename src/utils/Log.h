/**
 * @file Log.h
 * @brief Structured Serial logging for FluidLevelMonitor.
 *
 * @note TRACE/DEBUG compiled out unless FLM_LOG_VERBOSE is defined.
 * @note Never pass passwords or secrets as format args.
 */
#ifndef FLM_LOG_H
#define FLM_LOG_H

#include <Arduino.h>

namespace Log {

void rawPrint(const char* level, const char* module, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

void vprint(const char* level, const char* module, const char* fmt, va_list ap);

}  // namespace Log

#define FLM_LOG_ERROR(mod, fmt, ...)   Log::rawPrint("E", mod, fmt, ##__VA_ARGS__)
#define FLM_LOG_WARN(mod, fmt, ...)    Log::rawPrint("W", mod, fmt, ##__VA_ARGS__)
#define FLM_LOG_INFO(mod, fmt, ...)    Log::rawPrint("I", mod, fmt, ##__VA_ARGS__)

#if defined(FLM_LOG_VERBOSE)
#define FLM_LOG_DEBUG(mod, fmt, ...)   Log::rawPrint("D", mod, fmt, ##__VA_ARGS__)
#define FLM_LOG_TRACE(mod, fmt, ...)   Log::rawPrint("T", mod, fmt, ##__VA_ARGS__)
#else
#define FLM_LOG_DEBUG(mod, fmt, ...)   ((void)0)
#define FLM_LOG_TRACE(mod, fmt, ...)   ((void)0)
#endif

#if defined(FLM_DEBUG)
#define FLM_ASSERT(x) \
    do { \
        if (!(x)) { \
            FLM_LOG_ERROR("Assert", "%s:%d: %s", __FILE__, __LINE__, #x); \
        } \
    } while (0)
#else
#define FLM_ASSERT(x) ((void)0)
#endif

#endif
