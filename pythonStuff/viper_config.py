"""
ViperIDE Configuration for Jumperless
======================================

This file should be uploaded to the device at:
    /python_scripts/lib/viper_config.py

ViperIDE will automatically detect and use this configuration.
"""

# Configure ViperIDE to understand that jumperless is globally imported
# This is a hint file that ViperIDE can parse for autocomplete

# Tell ViperIDE about global imports
__viper_globals__ = [
    "jumperless",  # Module name
]

# Tell ViperIDE to pre-import these
__viper_preload__ = [
    "from jumperless import *",
]

# Provide autocomplete hints for ViperIDE
__viper_hints__ = {
    "modules": ["jumperless", "jfs"],
    "globals": True,
    "stub_path": "/python_scripts/lib/jumperless.pyi"
}

# Alternative: Export the module info
VIPER_CONFIG = {
    "name": "Jumperless MicroPython",
    "version": "1.0.0",
    "global_imports": ["jumperless"],
    "preload": ["from jumperless import *"],
    "stub_files": ["/python_scripts/lib/jumperless.pyi"],
    "autocomplete": {
        "enabled": True,
        "global_namespace": True,
        "modules": ["jumperless", "jfs"]
    }
}

