/**
 * @file ErrorHandler.cpp
 * @brief Implementation of centralized error handling system
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "ErrorHandler.h"
#include "Log.h"

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
    , _errorsFilePresent(false)
    , _lastPrintedCode(ErrorCode::ERR_NONE)
    , _lastPrintTime(0)
    , _lookupCacheNext(0)
    , _suppressedCount(0) {

    for (size_t i = 0; i < LOOKUP_CACHE_SIZE; i++) {
        _lookupCache[i].valid = false;
        _lookupCache[i].code = ErrorCode::ERR_NONE;
    }
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
    _errorsFilePresent = LittleFS.exists("/errors.json");
    if (_errorsFilePresent) {
        FLM_LOG_INFO("Err", "errors.json present");
    } else {
        FLM_LOG_WARN("Err", "errors.json not found");
    }
    _initialized = true;
    return true;
}

ErrorSeverity ErrorHandler::defaultSeverityFromCode(uint16_t codeNum) {
    if (codeNum >= 500) return ErrorSeverity::ERROR;
    if (codeNum >= 300) return ErrorSeverity::WARNING;
    return ErrorSeverity::INFO;
}

void ErrorHandler::lookupError(ErrorCode code, String& name, String& desc, ErrorSeverity& sev) const {
    for (size_t i = 0; i < LOOKUP_CACHE_SIZE; i++) {
        if (_lookupCache[i].valid && _lookupCache[i].code == code) {
            name = _lookupCache[i].name;
            desc = _lookupCache[i].desc;
            sev = _lookupCache[i].sev;
            return;
        }
    }
    lookupErrorFromFile(code, name, desc, sev);
    size_t slot = _lookupCacheNext % LOOKUP_CACHE_SIZE;
    _lookupCacheNext = (uint8_t)((_lookupCacheNext + 1) % LOOKUP_CACHE_SIZE);
    _lookupCache[slot].valid = true;
    _lookupCache[slot].code = code;
    _lookupCache[slot].name = name;
    _lookupCache[slot].desc = desc;
    _lookupCache[slot].sev = sev;
}

void ErrorHandler::lookupErrorFromFile(ErrorCode code, String& name, String& desc, ErrorSeverity& sev) const {
    uint16_t cn = static_cast<uint16_t>(code);
    name = "E" + String(cn);
    desc = "Error " + String(cn) + " - No description available";
    sev = defaultSeverityFromCode(cn);

    if (!_errorsFilePresent || !LittleFS.exists("/errors.json")) return;

    char key[8];
    snprintf(key, sizeof(key), "%u", (unsigned)cn);

    File f = LittleFS.open("/errors.json", "r");
    if (!f) return;

    JsonDocument filter;
    filter["errors"][key] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
    f.close();
    if (err) return;

    JsonObjectConst entry = doc["errors"][key];
    if (entry.isNull()) return;

    if (!entry["name"].isNull()) {
        const char* n = entry["name"];
        if (n) name = n;
    }
    if (!entry["description"].isNull()) {
        const char* d = entry["description"];
        if (d) desc = d;
    }
    if (!entry["severity"].isNull()) {
        const char* s = entry["severity"];
        if (s) {
            if (strcmp(s, "critical") == 0) sev = ErrorSeverity::CRITICAL;
            else if (strcmp(s, "error") == 0) sev = ErrorSeverity::ERROR;
            else if (strcmp(s, "warning") == 0) sev = ErrorSeverity::WARNING;
            else sev = ErrorSeverity::INFO;
        }
    }
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
    
    String name, description;
    ErrorSeverity severity;
    lookupError(code, name, description, severity);
    
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
    String name, desc;
    ErrorSeverity sev;
    lookupError(code, name, desc, sev);
    return desc;
}

String ErrorHandler::getErrorName(ErrorCode code) const {
    String name, desc;
    ErrorSeverity sev;
    lookupError(code, name, desc, sev);
    return name;
}

ErrorSeverity ErrorHandler::getErrorSeverity(ErrorCode code) const {
    String name, desc;
    ErrorSeverity sev;
    lookupError(code, name, desc, sev);
    return sev;
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

