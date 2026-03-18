/**
 * @file SensorFilter.cpp
 * @brief Implementation of signal filtering for fluid level sensors
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "SensorFilter.h"
#include <ArduinoJson.h>

// =============================================================================
// SECTION 1: MEDIAN FILTER IMPLEMENTATION
// =============================================================================

/**
 * @brief Constructor for MedianFilter
 * @param size Number of samples (odd number recommended)
 */
MedianFilter::MedianFilter(uint8_t size)
    : _size(min(size, (uint8_t)MAX_FILTER_SIZE))
    , _index(0)
    , _sampleCount(0)
    , _lastMedian(0) {
    
    // Step 1: Initialize buffer with zeros
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        _buffer[i] = 0;
    }
}

/**
 * @brief Add sample and calculate median
 * @param value New sensor reading
 * @return Median of collected samples
 */
float MedianFilter::filter(float value) {
    // Step 1: Add new sample to circular buffer
    _buffer[_index] = value;
    _index = (_index + 1) % _size;
    
    // Step 2: Track sample count
    if (_sampleCount < _size) {
        _sampleCount++;
    }
    
    // Step 3: Calculate and return median
    _lastMedian = calculateMedian();
    return _lastMedian;
}

/**
 * @brief Calculate median from buffer
 * @return Median value
 */
float MedianFilter::calculateMedian() {
    // Step 1: Copy buffer to working array
    float sorted[MAX_FILTER_SIZE];
    uint8_t count = min(_sampleCount, _size);
    
    for (uint8_t i = 0; i < count; i++) {
        sorted[i] = _buffer[i];
    }
    
    // Step 2: Sort the array
    insertionSort(sorted, count);
    
    // Step 3: Return middle value
    // For even count, return average of two middle values
    if (count % 2 == 0) {
        return (sorted[count/2 - 1] + sorted[count/2]) / 2.0f;
    } else {
        return sorted[count/2];
    }
}

/**
 * @brief Insertion sort - efficient for small arrays
 * @param arr Array to sort
 * @param n Array size
 */
void MedianFilter::insertionSort(float* arr, uint8_t n) {
    for (uint8_t i = 1; i < n; i++) {
        float key = arr[i];
        int8_t j = i - 1;
        
        // Move elements greater than key one position ahead
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/**
 * @brief Reset median filter
 */
void MedianFilter::reset() {
    _index = 0;
    _sampleCount = 0;
    _lastMedian = 0;
    
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        _buffer[i] = 0;
    }
}

/**
 * @brief Set new filter size
 * @param size New size
 */
void MedianFilter::setSize(uint8_t size) {
    _size = min(max(size, (uint8_t)1), (uint8_t)MAX_FILTER_SIZE);
    reset();
}

// =============================================================================
// SECTION 2: MOVING AVERAGE FILTER IMPLEMENTATION
// =============================================================================

/**
 * @brief Constructor for MovingAverageFilter
 * @param windowSize Size of averaging window
 */
MovingAverageFilter::MovingAverageFilter(uint8_t windowSize)
    : _windowSize(min(windowSize, (uint8_t)MAX_FILTER_SIZE))
    , _index(0)
    , _sampleCount(0)
    , _sum(0)
    , _lastAverage(0) {
    
    // Step 1: Initialize buffer
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        _buffer[i] = 0;
    }
}

/**
 * @brief Add sample and calculate moving average
 * @param value New sensor reading
 * @return Moving average of samples in window
 */
float MovingAverageFilter::filter(float value) {
    // Step 1: Subtract oldest value from sum (if buffer full)
    if (_sampleCount >= _windowSize) {
        _sum -= _buffer[_index];
    }
    
    // Step 2: Add new value to buffer and sum
    _buffer[_index] = value;
    _sum += value;
    
    // Step 3: Update index (circular buffer)
    _index = (_index + 1) % _windowSize;
    
    // Step 4: Track sample count
    if (_sampleCount < _windowSize) {
        _sampleCount++;
    }
    
    // Step 5: Calculate average
    _lastAverage = _sum / _sampleCount;
    return _lastAverage;
}

/**
 * @brief Reset moving average filter
 */
void MovingAverageFilter::reset() {
    _index = 0;
    _sampleCount = 0;
    _sum = 0;
    _lastAverage = 0;
    
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        _buffer[i] = 0;
    }
}

/**
 * @brief Set new window size
 * @param size New window size
 */
void MovingAverageFilter::setWindowSize(uint8_t size) {
    _windowSize = min(max(size, (uint8_t)1), (uint8_t)MAX_FILTER_SIZE);
    reset();
}

// =============================================================================
// SECTION 3: 1D KALMAN FILTER IMPLEMENTATION
// =============================================================================

/**
 * @brief Constructor for KalmanFilter1D
 * @param processNoise Process noise covariance (Q)
 * @param measurementNoise Measurement noise covariance (R)
 * @param estimateError Initial estimate error (P)
 */
KalmanFilter1D::KalmanFilter1D(float processNoise, float measurementNoise, float estimateError)
    : _processNoise(max(0.0001f, processNoise))
    , _measurementNoise(max(0.0001f, measurementNoise))
    , _estimateError(estimateError)
    , _estimate(0)
    , _kalmanGain(0)
    , _initialized(false) {
}

/**
 * @brief Process measurement through Kalman filter
 * 
 * Kalman filter equations:
 * 1. Predict: P = P + Q
 * 2. Update:  K = P / (P + R)
 * 3. Correct: X = X + K * (measurement - X)
 * 4. Update:  P = (1 - K) * P
 * 
 * @param measurement New sensor reading
 * @return Filtered (estimated) value
 */
float KalmanFilter1D::filter(float measurement) {
    // Step 1: Initialize with first measurement
    if (!_initialized) {
        _estimate = measurement;
        _initialized = true;
        return _estimate;
    }
    
    // Step 2: Prediction step - increase uncertainty
    // P = P + Q (process noise adds to error)
    _estimateError += _processNoise;
    
    // Step 3: Calculate Kalman gain
    // K = P / (P + R)
    // Higher K = trust measurement more
    // Lower K = trust prediction more
    _kalmanGain = _estimateError / (_estimateError + _measurementNoise);
    
    // Step 4: Update estimate with measurement
    // X = X + K * (measurement - X)
    // This is a weighted average between prediction and measurement
    _estimate = _estimate + _kalmanGain * (measurement - _estimate);
    
    // Step 5: Update error covariance
    // P = (1 - K) * P
    _estimateError = (1.0f - _kalmanGain) * _estimateError;
    
    return _estimate;
}

/**
 * @brief Reset Kalman filter to uninitialized state
 */
void KalmanFilter1D::reset() {
    _estimate = 0;
    _estimateError = KALMAN_ESTIMATE_ERROR;
    _kalmanGain = 0;
    _initialized = false;
}

/**
 * @brief Reset Kalman filter with known initial value
 * @param initialValue Starting estimate
 */
void KalmanFilter1D::reset(float initialValue) {
    _estimate = initialValue;
    _estimateError = KALMAN_ESTIMATE_ERROR;
    _kalmanGain = 0;
    _initialized = true;
}

// =============================================================================
// SECTION 4: COMBINED FLUID LEVEL FILTER IMPLEMENTATION
// =============================================================================

/**
 * @brief Constructor for combined filter
 * @param medianSize Median filter size
 * @param avgWindow Moving average window
 * @param kalmanQ Kalman process noise
 * @param kalmanR Kalman measurement noise
 */
FluidLevelFilter::FluidLevelFilter(uint8_t medianSize, uint8_t avgWindow, 
                                   float kalmanQ, float kalmanR)
    : _medianFilter(medianSize)
    , _movingAvgFilter(avgWindow)
    , _kalmanFilter(kalmanQ, kalmanR)
    , _lastFiltered(0)
    , _lastRaw(0)
    , _medianEnabled(true)
    , _movingAvgEnabled(true)
    , _kalmanEnabled(true) {
}

/**
 * @brief Process reading through all enabled filters
 * 
 * Processing chain:
 * Raw → [Median] → [Moving Avg] → [Kalman] → Output
 *          ↓            ↓            ↓
 *    Remove spikes  Smooth ripples  Optimal estimate
 * 
 * @param rawValue Raw sensor reading
 * @return Stable filtered value
 */
float FluidLevelFilter::filter(float rawValue) {
    _lastRaw = rawValue;
    float value = rawValue;
    
    // Step 1: Apply Median Filter (removes spikes)
    // This MUST come first to eliminate outliers before averaging
    // Sorts readings and picks middle value, completely discarding spikes
    if (_medianEnabled) {
        value = _medianFilter.filter(value);
    }
    
    // Step 2: Apply Moving Average (smooths ripples)
    // Handles fluid surface oscillation from sloshing
    // Creates smooth trend from ripple peaks and troughs
    if (_movingAvgEnabled) {
        value = _movingAvgFilter.filter(value);
    }
    
    // Step 3: Apply Kalman Filter (optimal estimation)
    // Balances prediction with measurement for best estimate
    // Adapts to actual changes while filtering noise
    if (_kalmanEnabled) {
        value = _kalmanFilter.filter(value);
    }
    
    _lastFiltered = value;
    return value;
}

/**
 * @brief Reset all filters
 */
void FluidLevelFilter::reset() {
    _medianFilter.reset();
    _movingAvgFilter.reset();
    _kalmanFilter.reset();
    _lastFiltered = 0;
    _lastRaw = 0;
}

/**
 * @brief Check if filters are warmed up
 * @return true if all enabled filters are ready
 */
bool FluidLevelFilter::isReady() const {
    bool medianReady = !_medianEnabled || _medianFilter.isReady();
    bool avgReady = !_movingAvgEnabled || _movingAvgFilter.isReady();
    bool kalmanReady = !_kalmanEnabled || _kalmanFilter.isInitialized();
    return medianReady && avgReady && kalmanReady;
}

/**
 * @brief Get filter status as JSON
 * @return JSON string with filter configuration and status
 */
String FluidLevelFilter::getStatusJson() const {
    JsonDocument doc;
    
    doc["lastRaw"] = _lastRaw;
    doc["lastFiltered"] = _lastFiltered;
    doc["ready"] = isReady();
    
    // Median filter status (spike removal)
    JsonObject median = doc["medianFilter"].to<JsonObject>();
    median["enabled"] = _medianEnabled;
    median["size"] = _medianFilter.getSize();
    median["samples"] = _medianFilter.getSampleCount();
    median["lastValue"] = _medianFilter.getLastValue();
    median["ready"] = _medianFilter.isReady();
    
    // Moving average status (ripple smoothing)
    JsonObject movingAvg = doc["movingAvgFilter"].to<JsonObject>();
    movingAvg["enabled"] = _movingAvgEnabled;
    movingAvg["windowSize"] = _movingAvgFilter.getWindowSize();
    movingAvg["samples"] = _movingAvgFilter.getSampleCount();
    movingAvg["lastValue"] = _movingAvgFilter.getLastValue();
    movingAvg["ready"] = _movingAvgFilter.isReady();
    
    // Kalman filter status (optimal estimation)
    JsonObject kalman = doc["kalmanFilter"].to<JsonObject>();
    kalman["enabled"] = _kalmanEnabled;
    kalman["processNoise"] = _kalmanFilter.getProcessNoise();
    kalman["measurementNoise"] = _kalmanFilter.getMeasurementNoise();
    kalman["kalmanGain"] = _kalmanFilter.getKalmanGain();
    kalman["estimate"] = _kalmanFilter.getEstimate();
    kalman["estimateError"] = _kalmanFilter.getEstimateError();
    kalman["initialized"] = _kalmanFilter.isInitialized();
    
    String output;
    serializeJson(doc, output);
    return output;
}

