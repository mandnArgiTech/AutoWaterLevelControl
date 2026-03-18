/**
 * @file FlmTime.h
 * @brief millis()-safe interval helpers (49-day wrap tolerant).
 */
#ifndef FLM_TIME_H
#define FLM_TIME_H

#include <Arduino.h>

/** @return true once per periodMs; updates lastMs on fire. */
static inline bool flmElapsedMs(unsigned long& lastMs, uint32_t periodMs) {
    unsigned long now = millis();
    if ((unsigned long)(now - lastMs) >= periodMs) {
        lastMs = now;
        return true;
    }
    return false;
}

#endif
