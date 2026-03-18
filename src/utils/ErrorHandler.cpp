/**
 * @file ErrorHandler.cpp
 * @brief Implementation of centralized error handling system
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "ErrorHandler.h"

// =============================================================================
// SECTION 1: SINGLETON INSTANCE
// =============================================================================

/**
 * @brief Get singleton instance of ErrorHandler
 * @return Reference to the single ErrorHandler instance
 */
ErrorHandler& ErrorHandler::getInstance() {
    static ErrorHandler instance;
    return instance;
}

// =============================================================================
// SECTION 2: CONSTRUCTOR
// =============================================================================

/**
 * @brief Private constructor - initializes member variables
 */
ErrorHandler::ErrorHandler() 
    : _initialized(false)
    , _errorIndex(0)
    , _errorCount(0)
    , _lastError(ErrorCode::ERR_NONE)
    , _errorCallback(nullptr)
    , _lastPrintedCode(ErrorCode::ERR_NONE)
    , _lastPrintTime(0)
    , _suppressedCount(0) {
    
    for (size_t i = 0; i < MAX_ERROR_HISTORY; i++) {
        _errorHistory[i].code = ErrorCode::ERR_NONE;
        _errorHistory[i].severity = ErrorSeverity::INFO;
        _errorHistory[i].timestamp = 0;
        _errorHistory[i].count = 0;
    }
}

// =============================================================================
// SECTION 3: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize error handler and load error descriptions from LittleFS
 * @return true if initialization successful
 */
bool ErrorHandler::begin() {
    // Step 1: Load error descriptions from JSON file
    if (!loadErrorDescriptions()) {
        Serial.println(F("[ErrorHandler] Warning: Could not load error descriptions"));
        // Continue anyway - will use default descriptions
    }
    
    // Step 2: Mark as initialized
    _initialized = true;
    
    Serial.println(F("[ErrorHandler] Initialized successfully"));
    return true;
}

// =============================================================================
// SECTION 4: LOAD ERROR DESCRIPTIONS
// =============================================================================

/**
 * @brief Load error descriptions from JSON file in LittleFS
 * @return true if loaded successfully
 */
bool ErrorHandler::loadErrorDescriptions() {
    // Step 1: Check if LittleFS is mounted
    FSInfo fs_info;
    if (!LittleFS.info(fs_info)) {
        Serial.println(F("[ErrorHandler] LittleFS not mounted!"));
        return false;
    }
    Serial.printf("[ErrorHandler] LittleFS: %u bytes used of %u\n", 
                  (unsigned)fs_info.usedBytes, (unsigned)fs_info.totalBytes);
    
    // Step 2: Check if file exists
    if (!LittleFS.exists("/errors.json")) {
        Serial.println(F("[ErrorHandler] errors.json not found in LittleFS"));
        // List files to debug
        Dir dir = LittleFS.openDir("/");
        Serial.println(F("[ErrorHandler] Files in LittleFS:"));
        while (dir.next()) {
            Serial.printf("  - %s (%u bytes)\n", dir.fileName().c_str(), (unsigned)dir.fileSize());
        }
        return false;
    }
    
    // Step 3: Open the file
    File file = LittleFS.open("/errors.json", "r");
    if (!file) {
        Serial.println(F("[ErrorHandler] Failed to open errors.json"));
        return false;
    }
    
    Serial.printf("[ErrorHandler] errors.json size: %u bytes\n", (unsigned)file.size());
    
    // Step 4: Parse JSON
    DeserializationError error = deserializeJson(_errorDescriptions, file);
    file.close();
    
    if (error) {
        Serial.print(F("[ErrorHandler] JSON parse error: "));
        Serial.println(error.c_str());
        return false;
    }
    
    // Step 5: Verify structure
    if (!_errorDescriptions["errors"].is<JsonObject>()) {
        Serial.println(F("[ErrorHandler] Invalid JSON structure - 'errors' not found"));
        return false;
    }
    
    // Count loaded errors
    JsonObject errors = _errorDescriptions["errors"].as<JsonObject>();
    int count = 0;
    for (JsonPair p : errors) {
        (void)p;
        count++;
    }
    Serial.printf("[ErrorHandler] Loaded %d error definitions\n", count);
    
    // Verify we can read a sample entry
    if (errors["101"].is<JsonObject>()) {
        String desc = errors["101"]["description"].as<String>();
        Serial.printf("[ErrorHandler] Test lookup E101: %s\n", desc.c_str());
    } else {
        Serial.println(F("[ErrorHandler] WARNING: Cannot access error 101!"));
    }
    
    return true;
}

// =============================================================================
// SECTION 5: ERROR LOGGING
// =============================================================================

/**
 * @brief Log an error with optional additional information
 * @param code Error code to log
 * @param additionalInfo Optional context information
 * @return The error code for chaining
 */
ErrorCode ErrorHandler::logError(ErrorCode code, const String& additionalInfo) {
    // Step 1: Update last error
    _lastError = code;
    
    // Step 2: Skip if no error
    if (code == ErrorCode::ERR_NONE) {
        return code;
    }
    
    // Step 3: Get error details
    ErrorSeverity severity = getErrorSeverity(code);
    String name = getErrorName(code);
    String description = getErrorDescription(code);
    
    // Step 4: Create error entry
    ErrorEntry& entry = _errorHistory[_errorIndex];
    entry.code = code;
    entry.severity = severity;
    entry.name = name;
    entry.description = description;
    entry.timestamp = millis();
    entry.count++;
    
    // Step 5: Update indices
    _errorIndex = (_errorIndex + 1) % MAX_ERROR_HISTORY;
    if (_errorCount < MAX_ERROR_HISTORY) {
        _errorCount++;
    }
    
    // Step 6: Rate-limited serial output
    unsigned long now = millis();
    bool shouldPrint = true;
    if (code == _lastPrintedCode && (now - _lastPrintTime < PRINT_INTERVAL_MS)) {
        _suppressedCount++;
        shouldPrint = false;
    } else {
        if (_suppressedCount > 0) {
            Serial.printf("[...] %u similar messages suppressed\n", (unsigned)_suppressedCount);
        }
        _lastPrintedCode = code;
        _lastPrintTime = now;
        _suppressedCount = 0;
    }
    if (shouldPrint) {
        printError(code, additionalInfo);
    }
    
    // Step 7: Call error callback if set
    if (_errorCallback) {
        String message = description;
        if (additionalInfo.length() > 0) {
            message += " - " + additionalInfo;
        }
        _errorCallback(code, severity, message);
    }
    
    return code;
}

// =============================================================================
// SECTION 6: ERROR ACCESSORS
// =============================================================================

/**
 * @brief Get the last error code
 * @return Last logged error code
 */
ErrorCode ErrorHandler::getLastError() const {
    return _lastError;
}

/**
 * @brief Get last error entry with full details
 * @return Pointer to last error entry or nullptr if no errors
 */
const ErrorEntry* ErrorHandler::getLastErrorEntry() const {
    if (_errorCount == 0) {
        return nullptr;
    }
    size_t lastIndex = (_errorIndex == 0) ? MAX_ERROR_HISTORY - 1 : _errorIndex - 1;
    return &_errorHistory[lastIndex];
}

/**
 * @brief Clear all errors from history
 */
void ErrorHandler::clearErrors() {
    _errorIndex = 0;
    _errorCount = 0;
    _lastError = ErrorCode::ERR_NONE;
    
    for (size_t i = 0; i < MAX_ERROR_HISTORY; i++) {
        _errorHistory[i].code = ErrorCode::ERR_NONE;
        _errorHistory[i].count = 0;
    }
}

/**
 * @brief Check if any errors are in history
 * @return true if there are errors
 */
bool ErrorHandler::hasErrors() const {
    return _errorCount > 0;
}

/**
 * @brief Get error count
 * @return Number of errors in history
 */
size_t ErrorHandler::getErrorCount() const {
    return _errorCount;
}

// =============================================================================
// SECTION 7: ERROR DESCRIPTION LOOKUPS
// =============================================================================

/**
 * @brief Get error description from code
 * @param code Error code
 * @return Error description string
 */
String ErrorHandler::getErrorDescription(ErrorCode code) const {
    // Step 1: Try to get from loaded JSON
    String codeStr = String(static_cast<uint16_t>(code));
    
    // Direct path access for ArduinoJson v7
    JsonVariantConst entry = _errorDescriptions["errors"][codeStr];
    if (!entry.isNull() && entry.is<JsonObjectConst>()) {
        JsonVariantConst desc = entry["description"];
        if (!desc.isNull()) {
            return desc.as<String>();
        }
    }
    
    // Step 2: Return default if not found
    return "Error " + codeStr + " - No description available";
}

/**
 * @brief Get error name from code
 * @param code Error code
 * @return Error name string
 */
String ErrorHandler::getErrorName(ErrorCode code) const {
    String codeStr = String(static_cast<uint16_t>(code));
    
    // Direct path access for ArduinoJson v7
    JsonVariantConst entry = _errorDescriptions["errors"][codeStr];
    if (!entry.isNull() && entry.is<JsonObjectConst>()) {
        JsonVariantConst name = entry["name"];
        if (!name.isNull()) {
            return name.as<String>();
        }
    }
    
    return "E" + codeStr;
}

/**
 * @brief Get severity level for error code
 * @param code Error code
 * @return Severity level
 */
ErrorSeverity ErrorHandler::getErrorSeverity(ErrorCode code) const {
    String codeStr = String(static_cast<uint16_t>(code));
    
    // Direct path access for ArduinoJson v7
    JsonVariantConst entry = _errorDescriptions["errors"][codeStr];
    if (!entry.isNull() && entry.is<JsonObjectConst>()) {
        JsonVariantConst sev = entry["severity"];
        if (!sev.isNull()) {
            String severity = sev.as<String>();
            
            if (severity == "critical") return ErrorSeverity::CRITICAL;
            if (severity == "error") return ErrorSeverity::ERROR;
            if (severity == "warning") return ErrorSeverity::WARNING;
            return ErrorSeverity::INFO;
        }
    }
    
    // Default based on error code range
    uint16_t codeNum = static_cast<uint16_t>(code);
    if (codeNum >= 500) return ErrorSeverity::ERROR;
    if (codeNum >= 300) return ErrorSeverity::WARNING;
    return ErrorSeverity::INFO;
}

/**
 * @brief Convert severity to string
 * @param severity Severity level
 * @return Severity string
 */
String ErrorHandler::severityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::INFO:     return "INFO";
        case ErrorSeverity::WARNING:  return "WARNING";
        case ErrorSeverity::ERROR:    return "ERROR";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        default:                      return "UNKNOWN";
    }
}

// =============================================================================
// SECTION 8: CALLBACK MANAGEMENT
// =============================================================================

/**
 * @brief Set callback for error notifications
 * @param callback Function to call when errors occur
 */
void ErrorHandler::setErrorCallback(ErrorCallback callback) {
    _errorCallback = callback;
}

// =============================================================================
// SECTION 9: JSON OUTPUT
// =============================================================================

/**
 * @brief Get error history as JSON string
 * @return JSON string containing error history
 */
String ErrorHandler::getErrorHistoryJson() const {
    JsonDocument doc;
    JsonArray errors = doc["errors"].to<JsonArray>();
    
    // Add errors in reverse chronological order
    for (size_t i = 0; i < _errorCount; i++) {
        size_t idx = (_errorIndex - 1 - i + MAX_ERROR_HISTORY) % MAX_ERROR_HISTORY;
        const ErrorEntry& entry = _errorHistory[idx];
        
        JsonObject error = errors.add<JsonObject>();
        error["code"] = static_cast<uint16_t>(entry.code);
        error["name"] = entry.name;
        error["severity"] = severityToString(entry.severity);
        error["description"] = entry.description;
        error["timestamp"] = entry.timestamp;
        error["count"] = entry.count;
    }
    
    doc["count"] = _errorCount;
    doc["hasErrors"] = hasErrors();
    
    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Get specific error info as JSON
 * @param code Error code
 * @return JSON string with error information
 */
String ErrorHandler::getErrorInfoJson(ErrorCode code) const {
    JsonDocument doc;
    
    doc["code"] = static_cast<uint16_t>(code);
    doc["name"] = getErrorName(code);
    doc["severity"] = severityToString(getErrorSeverity(code));
    doc["description"] = getErrorDescription(code);
    
    String output;
    serializeJson(doc, output);
    return output;
}

// =============================================================================
// SECTION 10: SERIAL OUTPUT
// =============================================================================

/**
 * @brief Print error to Serial with formatting
 * @param code Error code
 * @param additionalInfo Optional additional info
 */
void ErrorHandler::printError(ErrorCode code, const String& additionalInfo) const {
    ErrorSeverity severity = getErrorSeverity(code);
    
    // Format: [SEVERITY] E<code>: <description> (<additional info>)
    Serial.print(F("["));
    Serial.print(severityToString(severity));
    Serial.print(F("] "));
    Serial.print(getErrorName(code));
    Serial.print(F(": "));
    Serial.print(getErrorDescription(code));
    
    if (additionalInfo.length() > 0) {
        Serial.print(F(" ("));
        Serial.print(additionalInfo);
        Serial.print(F(")"));
    }
    
    Serial.println();
}

