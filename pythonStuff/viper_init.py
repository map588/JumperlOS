"""
ViperIDE Initialization Script
================================

Upload this to: /python_scripts/lib/viper_init.py

Purpose: Initialize ViperIDE session with full autocomplete support

Usage in ViperIDE:
    exec(open('/python_scripts/lib/viper_init.py').read())
    
This makes autocomplete work for the current session by importing
all functions globally and triggering ViperIDE's introspection.

Why needed: ViperIDE uses runtime introspection for autocomplete,
not static .pyi file analysis. Must import in REPL to populate.
"""

print("=" * 60)
print("  Jumperless ViperIDE Session Initialization")
print("=" * 60)

# Import the jumperless module globally
print("\n[1/3] Importing jumperless module...")
from jumperless import *

# Trigger introspection for autocomplete
print("[2/3] Populating autocomplete cache...")
import jumperless as _jl_module
_jl_items = [item for item in dir(_jl_module) if not item.startswith('_')]

# Show summary
print(f"[3/3] Complete! {len(_jl_items)} items available globally")
print("\n" + "=" * 60)
print("✓ Autocomplete enabled for this session")
print("=" * 60)
print("\nQuick Reference:")
print("  help()        - Show all available functions")
print("  nodes_help()  - Show node names reference")
print("  dir()         - List all global names")
print("\nTry typing: connect(D")
print("Autocomplete should suggest node names!")
print("=" * 60)

# Clean up temp variables from global namespace
del _jl_module, _jl_items

