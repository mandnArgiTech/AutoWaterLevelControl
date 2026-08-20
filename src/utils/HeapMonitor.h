/**
 * @file HeapMonitor.h
 * @brief Free-heap sampling + low-water mark for ESP8266 RAM safety
 *
 * Tracks min free heap and exposes max contiguous free block (fragmentation).
 * Used to shed load (skip MQTT publish) when heap falls below FLM_HEAP_FLOOR_BYTES.
 */
#ifndef FLM_HEAP_MONITOR_H
#define FLM_HEAP_MONITOR_H

#include <Arduino.h>

#ifndef FLM_HEAP_FLOOR_BYTES
#define FLM_HEAP_FLOOR_BYTES 8192u
#endif

class HeapMonitor {
public:
    static void sample() {
        uint32_t free = ESP.getFreeHeap();
        if (free < s_minFree) {
            s_minFree = free;
        }
    }

    static uint32_t freeHeap() { return ESP.getFreeHeap(); }

    static uint32_t minFreeHeap() {
        sample();
        return s_minFree;
    }

    static uint32_t maxFreeBlock() { return ESP.getMaxFreeBlockSize(); }

    static bool belowFloor() {
        sample();
        return ESP.getFreeHeap() < FLM_HEAP_FLOOR_BYTES;
    }

    static void resetMin() { s_minFree = UINT32_MAX; }

private:
    static uint32_t s_minFree;
};

inline uint32_t HeapMonitor::s_minFree = UINT32_MAX;

#endif // FLM_HEAP_MONITOR_H
