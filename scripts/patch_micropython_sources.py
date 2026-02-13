#!/usr/bin/env python3
"""
Post-build patches for MicroPython embed sources.

Applied automatically by build_micropython.sh after the stock MicroPython
files are copied into micropython_embed/. These patches fix issues specific
to the Jumperless embed port that cannot be addressed through mpconfigport.h
hooks or port-level overrides.

Each patch function documents the bug it fixes, and the script verifies that
patches applied correctly.
"""

import os
import sys

def patch_pyexec_readline_reset(micropython_embed_dir):
    """
    Fix: ViperIDE requires double-press of stop button when input() is active.

    Root cause: When Python's input() builtin calls readline(), it sets the
    global readline state (rl) to point at a stack-allocated vstr. When
    KeyboardInterrupt fires (NLR jump from mp_hal_stdin_rx_chr), the C stack
    unwinds but the global rl struct retains a dangling rl.line pointer.

    In pyexec_raw_repl_process_char's reset: label, vstr_reset() cleans the
    repl vstr but does NOT call readline_init(). When ViperIDE's Ctrl+B byte
    arrives, readline_process_char() checks vstr_len(rl.line)==rl.orig_line_len
    which reads garbage from the dangling pointer, FAILS, and Ctrl+B gets
    swallowed as a cursor movement instead of being returned as a mode-switch
    control character. The friendly REPL never activates.

    Fix: Add readline_init() calls at the reset: label (after every script
    execution) and in the Ctrl+B handler (belt-and-suspenders before mode switch).

    The friendly REPL's input_restart: already calls readline_init(), which is
    why this bug only manifests in raw REPL mode (used by ViperIDE/mpremote).
    """
    pyexec_path = os.path.join(
        micropython_embed_dir, "shared", "runtime", "pyexec.c"
    )

    if not os.path.exists(pyexec_path):
        print(f"  WARNING: {pyexec_path} not found, skipping patch")
        return False

    with open(pyexec_path, "r") as f:
        content = f.read()

    patched = False

    # Patch 1: Add readline_init at the reset: label after vstr_reset
    # Stock code:
    #     vstr_reset(MP_STATE_VM(repl_line));
    #     mp_hal_stdout_tx_str(">");
    old_reset = (
        'vstr_reset(MP_STATE_VM(repl_line));\n'
        '    mp_hal_stdout_tx_str(">");'
    )
    new_reset = (
        'vstr_reset(MP_STATE_VM(repl_line));\n'
        '    // JL patch: reset readline state after script execution to clear\n'
        '    // dangling rl.line pointer left by input() interrupted via NLR\n'
        '    readline_init(MP_STATE_VM(repl_line), "");\n'
        '    mp_hal_stdout_tx_str(">");'
    )

    if old_reset in content:
        content = content.replace(old_reset, new_reset, 1)
        patched = True
        print("  Applied patch 1: readline_init at reset: label")
    elif 'readline_init(MP_STATE_VM(repl_line), "");' in content and 'JL patch' in content:
        print("  Patch 1 already applied (readline_init at reset: label)")
        patched = True
    else:
        print("  WARNING: Could not apply patch 1 (reset: label context not found)")
        print("           Upstream pyexec.c may have changed - manual review needed")

    # Patch 2: Add readline_init before pyexec_friendly_repl_process_char in Ctrl+B handler
    # Stock code:
    #         repl.paste_mode = false;
    #         pyexec_friendly_repl_process_char(CHAR_CTRL_B);
    old_ctrlb = (
        'repl.paste_mode = false;\n'
        '        pyexec_friendly_repl_process_char(CHAR_CTRL_B);'
    )
    new_ctrlb = (
        'repl.paste_mode = false;\n'
        '        // JL patch: ensure clean readline state before switching to friendly REPL\n'
        '        readline_init(MP_STATE_VM(repl_line), "");\n'
        '        pyexec_friendly_repl_process_char(CHAR_CTRL_B);'
    )

    if old_ctrlb in content:
        content = content.replace(old_ctrlb, new_ctrlb, 1)
        patched = True
        print("  Applied patch 2: readline_init in Ctrl+B handler")
    elif 'readline_init' in content and 'clean readline state before switching' in content:
        print("  Patch 2 already applied (readline_init in Ctrl+B handler)")
        patched = True
    else:
        print("  WARNING: Could not apply patch 2 (Ctrl+B handler context not found)")
        print("           Upstream pyexec.c may have changed - manual review needed")

    if patched:
        with open(pyexec_path, "w") as f:
            f.write(content)

    return patched


def main():
    if len(sys.argv) < 2:
        # Default path relative to this script
        script_dir = os.path.dirname(os.path.abspath(__file__))
        micropython_embed_dir = os.path.join(
            script_dir, "..", "lib", "micropython", "micropython_embed"
        )
    else:
        micropython_embed_dir = sys.argv[1]

    micropython_embed_dir = os.path.realpath(micropython_embed_dir)

    print(f"Applying Jumperless patches to MicroPython sources...")
    print(f"  Target: {micropython_embed_dir}")

    all_ok = True

    # Apply all patches
    if not patch_pyexec_readline_reset(micropython_embed_dir):
        all_ok = False

    # Add future patches here as new functions

    if all_ok:
        print("All patches applied successfully.")
    else:
        print("WARNING: Some patches could not be applied.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
