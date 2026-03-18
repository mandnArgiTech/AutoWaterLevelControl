/**
 * @file SensorFilter.h
 * @brief Signal filtering for fluid level sensors
 * 
 * This module provides filtering algorithms to handle:
 * - Moving Average Filter: Smooths ripples and sloshing on fluid surfaces
 * - Median Filter: Eliminates spikes and outliers by picking middle value
 * 
 * For fluid level measurement, both filters are applied in sequence:
 * 1. Median filter first (removes spikes)
 * 2. Moving average second (smooths ripples)
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef SENSOR_FILTER_H
#define SENSOR_FILTER_H

#include <Arduino.h>

// =============================================================================
// SECTION 1: FILTER CONFIGURATION
// =============================================================================

#define MEDIAN_FILTER_SIZE      5       ///< Number of samples for median filter (odd number)
#define MOVING_AVG_WINDOW_SIZE  10      ///< Window size for moving average filter
#define MAX_FILTER_SIZE         20      ///< Maximum buffer size for filters

// Kalman filter defaults
#define KALMAN_PROCESS_NOISE    0.01f   ///< Process noise (Q) - lower = smoother, slower response
#define KALMAN_MEASURE_NOISE    0.1f    ///< Measurement noise (R) - higher = more filtering
#define KALMAN_ESTIMATE_ERROR   1.0f    ///< Initial estimate error covariance (P)

// =============================================================================
// SECTION 2: MEDIAN FILTER CLASS
// =============================================================================

/**
 * @class MedianFilter
 * @brief Eliminates spikes and outliers from sensor readings
 * 
 * The median filter collects N samples, sorts them, and returns the
 * middle value. This completely discards extreme outliers (spikes).
 * 
 * Best for: Removing electrical noise spikes, random measurement errors
 * 
 * Example with 5 samples: [120, 45, 118, 500, 121]
 * Sorted: [45, 118, 120, 121, 500]
 * Median (middle): 120 ← Spike (500) completely ignored
 */
class MedianFilter {
public:
    /**
     * @brief Constructor
     * @param size Number of samples for median calculation (odd number recommended)
     */
    explicit MedianFilter(uint8_t size = MEDIAN_FILTER_SIZE);
    
    /**
     * @brief Add a new sample and get filtered result
     * @param value New sensor reading
     * @return Median filtered value
     */
    float filter(float value);
    
    /**
     * @brief Get last filtered value without adding new sample
     * @return Last median value
     */
    float getLastValue() const { return _lastMedian; }
    
    /**
     * @brief Reset filter (clear all samples)
     */
    void reset();
    
    /**
     * @brief Check if filter has enough samples
     * @return true if buffer is full
     */
    bool isReady() const { return _sampleCount >= _size; }
    
    /**
     * @brief Get current sample count
     * @return Number of samples in buffer
     */
    uint8_t getSampleCount() const { return _sampleCount; }
    
    /**
     * @brief Get filter size
     * @return Configured filter size
     */
    uint8_t getSize() const { return _size; }
    
    /**
     * @brief Set new filter size (resets buffer)
     * @param size New size (1-MAX_FILTER_SIZE)
     */
    void setSize(uint8_t size);

private:
    /**
     * @brief Calculate median from buffer
     * @return Median value
     */
    float calculateMedian();
    
    /**
     * @brief Sort array using insertion sort (efficient for small arrays)
     * @param arr Array to sort
     * @param n Array size
     */
    void insertionSort(float* arr, uint8_t n);
    
    float _buffer[MAX_FILTER_SIZE];     ///< Sample buffer
    uint8_t _size;                      ///< Filter size
    uint8_t _index;                     ///< Current buffer index
    uint8_t _sampleCount;               ///< Number of samples collected
    float _lastMedian;                  ///< Last calculated median
};

// =============================================================================
// SECTION 3: MOVING AVERAGE FILTER CLASS
// =============================================================================

/**
 * @class MovingAverageFilter
 * @brief Smooths fluid surface ripples and sloshing
 * 
 * The moving average filter maintains a sliding window of N samples
 * and outputs their average. This smooths out variations caused by
 * fluid movement in tanks.
 * 
 * Best for: Smoothing oscillations, ripples, sloshing effects
 * 
 * Example with window=5: readings over time create smooth trend
 * rather than jumping between ripple peaks and troughs.
 */
class MovingAverageFilter {
public:
    /**
     * @brief Constructor
     * @param windowSize Size of the moving average window
     */
    explicit MovingAverageFilter(uint8_t windowSize = MOVING_AVG_WINDOW_SIZE);
    
    /**
     * @brief Add a new sample and get filtered result
     * @param value New sensor reading
     * @return Moving average filtered value
     */
    float filter(float value);
    
    /**
     * @brief Get last filtered value without adding new sample
     * @return Last average value
     */
    float getLastValue() const { return _lastAverage; }
    
    /**
     * @brief Reset filter (clear all samples)
     */
    void reset();
    
    /**
     * @brief Check if filter has full window
     * @return true if buffer is full
     */
    bool isReady() const { return _sampleCount >= _windowSize; }
    
    /**
     * @brief Get current sample count
     * @return Number of samples in buffer
     */
    uint8_t getSampleCount() const { return _sampleCount; }
    
    /**
     * @brief Get window size
     * @return Configured window size
     */
    uint8_t getWindowSize() const { return _windowSize; }
    
    /**
     * @brief Set new window size (resets buffer)
     * @param size New window size (1-MAX_FILTER_SIZE)
     */
    void setWindowSize(uint8_t size);
    
    /**
     * @brief Get current running sum
     * @return Sum of samples in window
     */
    float getSum() const { return _sum; }

private:
    float _buffer[MAX_FILTER_SIZE];     ///< Circular buffer
    uint8_t _windowSize;                ///< Window size
    uint8_t _index;                     ///< Current buffer index
    uint8_t _sampleCount;               ///< Number of samples collected
    float _sum;                         ///< Running sum for efficiency
    float _lastAverage;                 ///< Last calculated average
};

// =============================================================================
// SECTION 4: 1D KALMAN FILTER CLASS
// =============================================================================

/**
 * @class KalmanFilter1D
 * @brief 1D Kalman filter for optimal state estimation
 * 
 * The Kalman filter provides optimal estimation by balancing:
 * - Process noise (Q): How much the actual value can change between measurements
 * - Measurement noise (R): How noisy the sensor readings are
 * 
 * It's excellent for fluid level measurement because:
 * - Adapts to system dynamics (water filling/draining)
 * - Provides smooth output while remaining responsive
 * - Optimal balance between smoothing and tracking
 * 
 * Tuning guide:
 * - Lower Q = Smoother but slower response
 * - Higher Q = Faster response but more jitter
 * - Lower R = Trust measurements more (less filtering)
 * - Higher R = Trust model more (more filtering)
 * 
 * For fluid level: Q=0.01, R=0.1 is a good starting point
 */
class KalmanFilter1D {
public:
    /**
     * @brief Constructor with configurable noise parameters
     * @param processNoise Process noise covariance (Q)
     * @param measurementNoise Measurement noise covariance (R)
     * @param estimateError Initial estimate error covariance (P)
     */
    KalmanFilter1D(float processNoise = KALMAN_PROCESS_NOISE,
                   float measurementNoise = KALMAN_MEASURE_NOISE,
                   float estimateError = KALMAN_ESTIMATE_ERROR);
    
    /**
     * @brief Process a new measurement through the filter
     * @param measurement New sensor reading
     * @return Filtered (estimated) value
     */
    float filter(float measurement);
    
    /**
     * @brief Get current state estimate
     * @return Current filtered value
     */
    float getEstimate() const { return _estimate; }
    
    /**
     * @brief Get current Kalman gain
     * @return Kalman gain (0-1, higher = trust measurements more)
     */
    float getKalmanGain() const { return _kalmanGain; }
    
    /**
     * @brief Get current estimate error
     * @return Estimate error covariance (P)
     */
    float getEstimateError() const { return _estimateError; }
    
    /**
     * @brief Reset filter to initial state
     */
    void reset();
    
    /**
     * @brief Reset filter with a known initial value
     * @param initialValue Starting estimate
     */
    void reset(float initialValue);
    
    /**
     * @brief Check if filter has been initialized with first measurement
     * @return true if filter has processed at least one measurement
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * @brief Set process noise (Q)
     * @param q Process noise covariance (lower = smoother)
     */
    void setProcessNoise(float q) { _processNoise = max(0.0001f, q); }
    
    /**
     * @brief Get process noise (Q)
     * @return Process noise covariance
     */
    float getProcessNoise() const { return _processNoise; }
    
    /**
     * @brief Set measurement noise (R)
     * @param r Measurement noise covariance (higher = more filtering)
     */
    void setMeasurementNoise(float r) { _measurementNoise = max(0.0001f, r); }
    
    /**
     * @brief Get measurement noise (R)
     * @return Measurement noise covariance
     */
    float getMeasurementNoise() const { return _measurementNoise; }

private:
    float _processNoise;        ///< Process noise covariance (Q)
    float _measurementNoise;    ///< Measurement noise covariance (R)
    float _estimateError;       ///< Estimate error covariance (P)
    float _estimate;            ///< Current state estimate
    float _kalmanGain;          ///< Current Kalman gain
    bool _initialized;          ///< Filter initialized flag
};

// =============================================================================
// SECTION 5: COMBINED FLUID LEVEL FILTER CLASS
// =============================================================================

/**
 * @class FluidLevelFilter
 * @brief Combined filter optimized for fluid level measurement
 * 
 * This class combines Median, Moving Average, and Kalman filters in the
 * optimal sequence for fluid level sensing:
 * 
 * Raw Reading → [Median] → [Moving Avg] → [Kalman] → Stable Output
 *                  ↓            ↓             ↓
 *            Remove spikes  Smooth ripples  Optimal estimate
 * 
 * The three-stage approach provides:
 * - Spike rejection (median first removes outliers)
 * - Ripple smoothing (moving average smooths fluid movement)
 * - Optimal estimation (Kalman balances responsiveness and stability)
 * 
 * Each filter can be independently enabled/disabled for tuning.
 */
class FluidLevelFilter {
public:
    /**
     * @brief Constructor with configurable parameters
     * @param medianSize Median filter size (default: 5)
     * @param avgWindow Moving average window (default: 10)
     * @param kalmanQ Kalman process noise (default: 0.01)
     * @param kalmanR Kalman measurement noise (default: 0.1)
     */
    FluidLevelFilter(uint8_t medianSize = MEDIAN_FILTER_SIZE, 
                     uint8_t avgWindow = MOVING_AVG_WINDOW_SIZE,
                     float kalmanQ = KALMAN_PROCESS_NOISE,
                     float kalmanR = KALMAN_MEASURE_NOISE);
    
    /**
     * @brief Process a raw sensor reading through both filters
     * @param rawValue Raw sensor reading
     * @return Filtered (stable) value
     */
    float filter(float rawValue);
    
    /**
     * @brief Get last filtered value
     * @return Last output value
     */
    float getLastValue() const { return _lastFiltered; }
    
    /**
     * @brief Get value after median filter only (before moving avg)
     * @return Value after spike removal
     */
    float getMedianValue() const { return _medianFilter.getLastValue(); }
    
    /**
     * @brief Reset both filters
     */
    void reset();
    
    /**
     * @brief Check if filters are warmed up
     * @return true if both filters have full buffers
     */
    bool isReady() const;
    
    /**
     * @brief Enable/disable median filter
     * @param enable true to enable
     */
    void enableMedianFilter(bool enable) { _medianEnabled = enable; }
    
    /**
     * @brief Enable/disable moving average filter
     * @param enable true to enable
     */
    void enableMovingAverage(bool enable) { _movingAvgEnabled = enable; }
    
    /**
     * @brief Enable/disable Kalman filter
     * @param enable true to enable
     */
    void enableKalmanFilter(bool enable) { _kalmanEnabled = enable; }
    
    /**
     * @brief Check if median filter is enabled
     * @return true if enabled
     */
    bool isMedianEnabled() const { return _medianEnabled; }
    
    /**
     * @brief Check if moving average is enabled
     * @return true if enabled
     */
    bool isMovingAvgEnabled() const { return _movingAvgEnabled; }
    
    /**
     * @brief Check if Kalman filter is enabled
     * @return true if enabled
     */
    bool isKalmanEnabled() const { return _kalmanEnabled; }
    
    /**
     * @brief Configure median filter size
     * @param size New size
     */
    void setMedianSize(uint8_t size) { _medianFilter.setSize(size); }
    
    /**
     * @brief Configure moving average window
     * @param size New window size
     */
    void setMovingAvgWindow(uint8_t size) { _movingAvgFilter.setWindowSize(size); }
    
    /**
     * @brief Configure Kalman process noise (Q)
     * @param q Process noise (lower = smoother, slower)
     */
    void setKalmanProcessNoise(float q) { _kalmanFilter.setProcessNoise(q); }
    
    /**
     * @brief Configure Kalman measurement noise (R)
     * @param r Measurement noise (higher = more filtering)
     */
    void setKalmanMeasurementNoise(float r) { _kalmanFilter.setMeasurementNoise(r); }
    
    /**
     * @brief Get median filter reference
     * @return Reference to median filter
     */
    MedianFilter& getMedianFilter() { return _medianFilter; }
    
    /**
     * @brief Get median filter const reference
     * @return Const reference to median filter
     */
    const MedianFilter& getMedianFilter() const { return _medianFilter; }
    
    /**
     * @brief Get moving average filter reference
     * @return Reference to moving average filter
     */
    MovingAverageFilter& getMovingAvgFilter() { return _movingAvgFilter; }
    
    /**
     * @brief Get moving average filter const reference
     * @return Const reference to moving average filter
     */
    const MovingAverageFilter& getMovingAvgFilter() const { return _movingAvgFilter; }
    
    /**
     * @brief Get Kalman filter reference
     * @return Reference to Kalman filter
     */
    KalmanFilter1D& getKalmanFilter() { return _kalmanFilter; }
    
    /**
     * @brief Get Kalman filter const reference
     * @return Const reference to Kalman filter
     */
    const KalmanFilter1D& getKalmanFilter() const { return _kalmanFilter; }
    
    /**
     * @brief Get filter status as JSON
     * @return JSON string with filter status
     */
    String getStatusJson() const;

private:
    MedianFilter _medianFilter;             ///< Spike removal filter
    MovingAverageFilter _movingAvgFilter;   ///< Ripple smoothing filter
    KalmanFilter1D _kalmanFilter;           ///< Optimal estimation filter
    float _lastFiltered;                    ///< Last output value
    float _lastRaw;                         ///< Last raw input
    bool _medianEnabled;                    ///< Median filter enabled
    bool _movingAvgEnabled;                 ///< Moving average enabled
    bool _kalmanEnabled;                    ///< Kalman filter enabled
};

#endif // SENSOR_FILTER_H

