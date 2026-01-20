"""
PlatformIO Pre-Build Script: Windows C++ Compilation Fix

Adds -include Arduino.h flag ONLY to C++ file compilation on Windows.
This ensures Arduino.h (with Stream/Print classes) is available before
any library headers that depend on it.

"""

Import("env")
import platform

def is_windows():
    """Check if running on Windows"""
    return platform.system() == "Windows"

if is_windows():
    print("🔧 Applying Windows build fix: forcing Arduino.h include for C++ files")
    
    # Add -include Arduino.h ONLY to C++ compilation flags (CXXFLAGS)
    # This won't affect C files (.c) or assembly files (.S)
    env.Append(CXXFLAGS=["-include", "Arduino.h"])
    
    print("✅ Windows build fix applied")
