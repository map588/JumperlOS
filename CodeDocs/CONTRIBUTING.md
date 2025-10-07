# Contributing to JumperlOS

Thank you for your interest in contributing to JumperlOS! This document will help you get set up for development.

## Development Environment Setup

### Prerequisites

1. **PlatformIO**: Install [PlatformIO](https://platformio.org/install) (either the IDE or CLI)
2. **Git**: For version control
3. **Text Editor/IDE**: VSCode with PlatformIO extension recommended, but any editor that supports clangd works

### Initial Setup

After cloning the repository, follow these steps to set up your development environment:

#### 1. Install Dependencies

PlatformIO will automatically install all required dependencies when you first build the project:

```bash
cd JumperlOS
pio run
```

This will download:
- RP2040 toolchain
- Arduino framework for Pico
- All library dependencies
- Pico SDK

#### 2. Generate Compilation Database for clangd

To get proper IntelliSense, code completion, and linting in your editor, generate the compilation database:

```bash
pio run -t compiledb
```

This creates a `compile_commands.json` file that tells clangd (the C/C++ language server) where to find all the include files, defines, and compiler flags.

**When to regenerate the compilation database:**
- After first cloning the repository
- After adding new libraries or dependencies to `platformio.ini`
- After changing build flags or configurations
- If you're getting "file not found" errors in your editor despite the code compiling fine

#### 3. Editor Configuration

**VSCode:**
- The `.clangd` configuration file is already set up
- Install the "clangd" extension (not Microsoft's C/C++ extension, or disable IntelliSense if using both)
- Reload the window after generating `compile_commands.json`

**Other Editors:**
- Ensure your editor has clangd support
- Point it to the workspace root where `compile_commands.json` is located
- The `.clangd` file contains additional settings that clangd will use automatically

## Building and Uploading

### Build the project
```bash
pio run
```

### Upload to device
```bash
pio run -t upload
```

### Clean build
```bash
pio run -t clean
```

## Project Structure

- `src/` - Main source code
- `include/` - Header files
- `lib/` - Local libraries
- `boards/` - Custom board definitions
- `modules/` - Native modules (MicroPython bindings, etc.)
- `platformio.ini` - PlatformIO project configuration

## Code Style

The project uses the following formatting style (configured in `.clangd`):
- Based on LLVM style
- No column limit
- 4-space indentation
- No tabs

Format your code before committing:
```bash
# If you have clang-format installed
clang-format -i src/your_file.cpp
```

## Debugging

For debugging with hardware debuggers (J-Link, Raspberry Pi Debug Probe, etc.), see the debug configurations in `.vscode/launch.json`.

## Common Issues

### "hardware/pio.h file not found" or similar include errors

This means the compilation database needs to be regenerated:
```bash
pio run -t compiledb
```

Then reload your editor window.

### Library not found during build

Clean and rebuild:
```bash
pio run -t clean
pio run
```

### Linker errors

Make sure you're using the correct platform version specified in `platformio.ini`:
```bash
pio pkg update
```

## Submitting Changes

1. Create a new branch for your feature/fix
2. Make your changes
3. Test thoroughly on hardware if possible
4. Commit with clear, descriptive messages
5. Push to your fork and create a pull request

## Questions?

Join the [Discord](https://discord.gg/bvacV7r3FP) if you have any questions!

