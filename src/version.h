/**
 * @file version.h
 * @brief Version information for FluidLevelMonitor project
 * 
 * This file contains version macros and functions for the water level
 * monitoring system. Build number is auto-incremented by the build script.
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef VERSION_H
#define VERSION_H

#include <Arduino.h>

// =============================================================================
// SECTION 1: VERSION DEFINITIONS
// =============================================================================

/**
 * Version numbers are defined in platformio.ini build_flags:
 * - VERSION_MAJOR: Major version (breaking changes)
 * - VERSION_MINOR: Minor version (new features)
 * - VERSION_PATCH: Patch version (bug fixes)
 * - BUILD_NUMBER: Auto-incremented build number
 * - BUILD_DATE: Build date string
 * - BUILD_TIME: Build time string
 */

// Provide defaults if not defined (for IDE intellisense)
#ifndef VERSION_MAJOR
    #define VERSION_MAJOR 1
#endif

#ifndef VERSION_MINOR
    #define VERSION_MINOR 0
#endif

#ifndef VERSION_PATCH
    #define VERSION_PATCH 0
#endif

#ifndef BUILD_NUMBER
    #define BUILD_NUMBER 0
#endif

#ifndef BUILD_DATE
    #define BUILD_DATE "Unknown"
#endif

#ifndef BUILD_TIME
    #define BUILD_TIME "Unknown"
#endif

// =============================================================================
// SECTION 2: PROJECT INFORMATION
// =============================================================================

#define PROJECT_NAME        "FluidLevelMonitor"
#define PROJECT_DESCRIPTION "Sintex Tank Water Level Monitoring System"
#define DEVICE_TYPE         "NodeMCU-V2"
#define FIRMWARE_PREFIX     "FLM"

// =============================================================================
// SECTION 3: VERSION STRING HELPERS
// =============================================================================

/**
 * @brief Helper macro to stringify a value
 */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/**
 * @brief Full version string (e.g., "1.0.0")
 */
#define VERSION_STRING TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) "." TOSTRING(VERSION_PATCH)

/**
 * @brief Full version with build number (e.g., "1.0.0.42")
 */
#define VERSION_FULL VERSION_STRING "." TOSTRING(BUILD_NUMBER)

/**
 * @brief Firmware identifier (e.g., "FLM-1.0.0.42")
 */
#define FIRMWARE_VERSION FIRMWARE_PREFIX "-" VERSION_FULL

// =============================================================================
// SECTION 4: VERSION CLASS
// =============================================================================

/**
 * @class Version
 * @brief Static class providing version information accessors
 * 
 * This class provides convenient methods to access version information
 * throughout the application.
 */
class Version {
public:
    /**
     * @brief Get major version number
     * @return Major version as integer
     */
    static int getMajor() { return VERSION_MAJOR; }
    
    /**
     * @brief Get minor version number
     * @return Minor version as integer
     */
    static int getMinor() { return VERSION_MINOR; }
    
    /**
     * @brief Get patch version number
     * @return Patch version as integer
     */
    static int getPatch() { return VERSION_PATCH; }
    
    /**
     * @brief Get build number
     * @return Build number as integer
     */
    static int getBuild() { return BUILD_NUMBER; }
    
    /**
     * @brief Get version string (e.g., "1.0.0")
     * @return Version string
     */
    static String getVersion() { return VERSION_STRING; }
    
    /**
     * @brief Get full version with build (e.g., "1.0.0.42")
     * @return Full version string
     */
    static String getFullVersion() { return VERSION_FULL; }
    
    /**
     * @brief Get firmware identifier (e.g., "FLM-1.0.0.42")
     * @return Firmware version string
     */
    static String getFirmware() { return FIRMWARE_VERSION; }
    
    /**
     * @brief Get build date
     * @return Build date string
     */
    static String getBuildDate() { return BUILD_DATE; }
    
    /**
     * @brief Get build time
     * @return Build time string
     */
    static String getBuildTime() { return BUILD_TIME; }
    
    /**
     * @brief Get project name
     * @return Project name string
     */
    static String getProjectName() { return PROJECT_NAME; }
    
    /**
     * @brief Get device type
     * @return Device type string
     */
    static String getDeviceType() { return DEVICE_TYPE; }
    
    /**
     * @brief Print version information to Serial
     */
    static void printInfo() {
        Serial.println();
        Serial.println(F("╔══════════════════════════════════════════════════╗"));
        Serial.println(F("║       FluidLevelMonitor - Version Info           ║"));
        Serial.println(F("╠══════════════════════════════════════════════════╣"));
        Serial.print(F("║ Project    : ")); Serial.println(PROJECT_NAME);
        Serial.print(F("║ Description: ")); Serial.println(PROJECT_DESCRIPTION);
        Serial.print(F("║ Device     : ")); Serial.println(DEVICE_TYPE);
        Serial.print(F("║ Firmware   : ")); Serial.println(FIRMWARE_VERSION);
        Serial.print(F("║ Build Date : ")); Serial.print(BUILD_DATE); 
        Serial.print(F(" ")); Serial.println(BUILD_TIME);
        Serial.println(F("╚══════════════════════════════════════════════════╝"));
        Serial.println();
    }
};

#endif // VERSION_H

