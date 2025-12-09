"""
Jumperless Global Imports
==========================

This file automatically imports all jumperless functions and constants into
the global namespace. It should be executed at REPL startup.

Usage:
    exec(open('/python_scripts/lib/jumperless_globals.py').read())
    
Or better yet, this is automatically executed when entering the REPL.
"""

# Import everything from the jumperless module into global namespace
from jumperless import *

# Inform the user (optional - can be commented out for cleaner startup)
# print("Jumperless module loaded globally - all functions available without prefix")

