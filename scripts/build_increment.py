#!/usr/bin/env python3
"""
Build Number Auto-Increment Script for PlatformIO
FluidLevelMonitor Project

This script automatically increments the build number on each compilation.
The build number is stored in a JSON file and injected as a build flag.
"""

import json
import os
from datetime import datetime

Import("env")  # type: ignore # PlatformIO build environment

# Path to build number file
BUILD_FILE = os.path.join(env.subst("$PROJECT_DIR"), "scripts", "build_number.json")

def get_build_info():
    """
    Read or initialize build information from JSON file.
    
    Returns:
        dict: Build information containing build_number, last_build_date, and total_builds
    """
    default_info = {
        "build_number": 0,
        "last_build_date": "",
        "total_builds": 0
    }
    
    try:
        if os.path.exists(BUILD_FILE):
            with open(BUILD_FILE, 'r') as f:
                return json.load(f)
    except (json.JSONDecodeError, IOError) as e:
        print(f"Warning: Could not read build file: {e}")
    
    return default_info

def save_build_info(info):
    """
    Save build information to JSON file.
    
    Args:
        info (dict): Build information to save
    """
    try:
        with open(BUILD_FILE, 'w') as f:
            json.dump(info, f, indent=2)
    except IOError as e:
        print(f"Warning: Could not save build file: {e}")

def increment_build():
    """
    Increment build number and update build information.
    
    Returns:
        int: New build number
    """
    info = get_build_info()
    
    # Increment build number
    info["build_number"] = info.get("build_number", 0) + 1
    info["total_builds"] = info.get("total_builds", 0) + 1
    info["last_build_date"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    save_build_info(info)
    
    return info["build_number"]

# Get current build number and increment
build_number = increment_build()
build_date = datetime.now().strftime("%Y-%m-%d")
build_time = datetime.now().strftime("%H:%M:%S")

# Add build flags with version information
env.Append(CPPDEFINES=[
    ("BUILD_NUMBER", build_number),
    ("BUILD_DATE", f'\\"{build_date}\\"'),
    ("BUILD_TIME", f'\\"{build_time}\\"')
])

print(f">>> Build #{build_number} - {build_date} {build_time}")

