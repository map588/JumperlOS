"""
ViperIDE Autocomplete Test Script
==================================

Run this in ViperIDE after connecting to test autocomplete discovery.

Instructions:
1. Connect to Jumperless in ViperIDE
2. Copy and paste this entire script into the ViperIDE console
3. After running, try typing 'connect(' and see if autocomplete appears
"""

print("=" * 60)
print("ViperIDE Autocomplete Discovery Test")
print("=" * 60)

# Import the module to populate ViperIDE's autocomplete cache
print("\n1. Importing jumperless module...")
import jumperless

# Import all names globally
print("2. Importing all names globally...")
from jumperless import *

# Trigger introspection to help ViperIDE discover functions
print("3. Running introspection...")
jl_items = [item for item in dir(jumperless) if not item.startswith('_')]
print(f"   Found {len(jl_items)} public items in jumperless module")

# Print a sample of available functions
print("\n4. Sample of available functions:")
sample_funcs = [item for item in jl_items if not item.isupper()][:15]
for func in sample_funcs:
    print(f"   - {func}")

print("\n5. Sample of available constants:")
sample_consts = [item for item in jl_items if item.isupper()][:15]
for const in sample_consts:
    print(f"   - {const}")

print("\n" + "=" * 60)
print("✓ Discovery complete!")
print("=" * 60)
print("\nNow try typing the following in the console:")
print("  connect(")
print("  dac_set(")
print("  gpio_set(")
print("\nViperIDE should show autocomplete suggestions.")
print("\nIf not, ViperIDE's autocomplete may be limited and you'll")
print("need to use help() for reference instead.")
print("=" * 60)

