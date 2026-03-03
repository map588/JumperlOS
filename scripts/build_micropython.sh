#!/bin/bash

# Build script for MicroPython embed port with built-in modules and Jumperless integration
set -e

# Configuration
MICROPYTHON_VERSION="v1.25.0"
MICROPYTHON_REPO_PATH=$(realpath "$(dirname "$0")/../micropython_repo")
MICROPYTHON_LOCAL_PATH=$(realpath "$(dirname "$0")/../lib/micropython")
PROJECT_ROOT=$(realpath "$(dirname "$0")/../")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building MicroPython embed port with built-in modules...${NC}"

# Check if we already have a working micropython_embed with Jumperless integration
cd "$MICROPYTHON_LOCAL_PATH"
if [ -f "micropython_embed/genhdr/qstrdefs.generated.h" ] && grep -q "jumperless" micropython_embed/genhdr/qstrdefs.generated.h; then
    echo -e "${GREEN}◆ MicroPython embed port with Jumperless integration already exists!${NC}"
    
    # Verify the existing build
    QSTR_COUNT=$(grep -c "^QDEF" micropython_embed/genhdr/qstrdefs.generated.h || true)
    JUMPERLESS_QSTRS=$(grep -c "jumperless\|dac_set\|adc_get\|nodes_connect" micropython_embed/genhdr/qstrdefs.generated.h || true)
    TIME_QSTRS=$(grep -c "time\|sleep\|ticks" micropython_embed/genhdr/qstrdefs.generated.h || true)
    echo -e "${GREEN}   Found $QSTR_COUNT total QSTR definitions${NC}"
    echo -e "${GREEN}   Jumperless module QSTRs found: $JUMPERLESS_QSTRS${NC}"
    echo -e "${GREEN}   Time module QSTRs found: $TIME_QSTRS${NC}"
    echo -e "${GREEN}◆ MicroPython embed port is ready with built-in modules!${NC}"
    # exit 0
fi

echo -e "${YELLOW}Building MicroPython embed port with Jumperless module and built-in modules...${NC}"

# Check if MicroPython repo exists
if [ ! -d "$MICROPYTHON_REPO_PATH" ]; then
    echo -e "${YELLOW}Cloning MicroPython repository (read-only)...${NC}"
    git clone "https://github.com/micropython/micropython.git" "${MICROPYTHON_REPO_PATH}"
    
    # Configure the repository to prevent accidental pushes to GitHub
    pushd "${MICROPYTHON_REPO_PATH}"
    echo -e "${YELLOW}Configuring repository to prevent GitHub commits...${NC}"
    # Remove the origin remote to prevent accidental pushes
    git remote remove origin 2>/dev/null || true
    # Set a dummy remote URL to prevent accidental remote operations
    git remote add origin "file:///dev/null"
    popd
fi

pushd "${MICROPYTHON_REPO_PATH}"
# Ensure no remote operations can happen
if git remote get-url origin 2>/dev/null | grep -q "github.com"; then
    echo -e "${YELLOW}WARNING: Removing GitHub remote to prevent accidental commits...${NC}"
    git remote remove origin
    git remote add origin "file:///dev/null"
fi

git checkout "${MICROPYTHON_VERSION}"
# Initialize submodules needed for the build
echo -e "${YELLOW}Initializing required submodules...${NC}"
git submodule update --init --recursive lib/uzlib lib/libm lib/libm_dbl
popd

# Build mpy-cross first
echo -e "${YELLOW}Building mpy-cross compiler...${NC}"
cd "$MICROPYTHON_REPO_PATH"
make -C mpy-cross V=1

# Clean previous build
echo -e "${YELLOW}Cleaning previous MicroPython embed build...${NC}"
cd "$MICROPYTHON_REPO_PATH/ports/embed"
# Set environment variables for the build
export MICROPYTHON_TOP="$MICROPYTHON_REPO_PATH"
make -f embed.mk clean-micropython-embed-package V=1

cd "$MICROPYTHON_LOCAL_PATH"
if [ -d "micropython_embed" ]; then
    echo -e "${YELLOW}Cleaning previous micropython_embed...${NC}"
    rm -rf micropython_embed
fi

# Copy our custom mpconfigport.h (and include) to the embed port so QSTR scan sees our extras
echo -e "${YELLOW}Setting up custom configuration for embed port...${NC}"
cp "$MICROPYTHON_LOCAL_PATH/port/mpconfigport.h" "$MICROPYTHON_REPO_PATH/ports/embed/"
# Ensure the include referenced by MICROPY_PY_MACHINE_INCLUDEFILE is visible to the embed build
mkdir -p "$MICROPYTHON_REPO_PATH/ports/embed/port"
cp "$MICROPYTHON_LOCAL_PATH/port/modmachine_jl.inc" "$MICROPYTHON_REPO_PATH/ports/embed/port/"



# Modify the embed.mk to include extmod modules
echo -e "${YELLOW}Modifying embed port to include extmod modules...${NC}"
cd "$MICROPYTHON_REPO_PATH/ports/embed"

# Create a backup of the original embed.mk
if [ ! -f "embed.mk.backup" ]; then
    cp embed.mk embed.mk.backup
fi

# Modify embed.mk to include extmod modules
cat > embed_with_extmod.mk << 'EOF'
# This file is part of the MicroPython project, http://micropython.org/
# Modified to include specific extmod modules for time, os

# Set the build output directory for the generated files.
BUILD = build-embed

# Include the core environment definitions; this will set $(TOP).
include $(MICROPYTHON_TOP)/py/mkenv.mk

# Include py core make definitions.
include $(TOP)/py/py.mk

# Define extmod source files we specifically want  
# Note: machine_*.c peripheral files use INCLUDEFILE pattern and are NOT compiled here
# Their QSTRs are generated via explicit references in modmachine_jl.inc
# Exception: machine_bitstream.c is compiled (doesn't use INCLUDEFILE)
SRC_EXTMOD_C = \
	extmod/modtime.c \
	extmod/modplatform.c \
	extmod/moductypes.c \
	extmod/modmachine.c \
	extmod/machine_bitstream.c \
	extmod/vfs.c \
	extmod/vfs_reader.c \
	extmod/modos.c \
	extmod/modbinascii.c \
	extmod/modselect.c \
	extmod/modasyncio.c \
	extmod/moddeflate.c \
	extmod/modframebuf.c \
	extmod/modonewire.c \
	extmod/modjson.c \
	extmod/modre.c \
	extmod/modhashlib.c \
	extmod/modrandom.c \
	extmod/modheapq.c \

# Define shared source files we want
SRC_SHARED_C = \
	shared/readline/readline.c \
	shared/runtime/pyexec.c \
	shared/runtime/sys_stdio_mphal.c \

# Define driver files for soft I2C/SPI
SRC_DRIVERS_C = \
	drivers/bus/softspi.c \
	drivers/bus/softqspi.c \

# Process extmod sources like regular sources
PY_O += $(addprefix $(BUILD)/, $(SRC_EXTMOD_C:.c=.o))
PY_O += $(addprefix $(BUILD)/, $(SRC_SHARED_C:.c=.o))
PY_O += $(addprefix $(BUILD)/, $(SRC_DRIVERS_C:.c=.o))
SRC_QSTR += $(SRC_EXTMOD_C)
SRC_QSTR += $(SRC_SHARED_C)
SRC_QSTR += $(SRC_DRIVERS_C)
# Note: Peripheral QSTRs come from jl_qstr_refs() in modmachine_jl.inc

# Set the location of the MicroPython embed port.
MICROPYTHON_EMBED_PORT = $(MICROPYTHON_TOP)/ports/embed

# Set default makefile-level MicroPython feature configurations.
MICROPY_ROM_TEXT_COMPRESSION ?= 0

# Set CFLAGS for the MicroPython build.
CFLAGS += -I. -I$(TOP) -I$(TOP)/extmod -I$(TOP)/drivers -I$(BUILD) -I$(MICROPYTHON_EMBED_PORT) -I$(MICROPYTHON_EMBED_PORT)/port
CFLAGS += -Wall -Werror -std=c99

# Define the required generated header files.
GENHDR_OUTPUT = $(addprefix $(BUILD)/genhdr/, \
	moduledefs.h \
	mpversion.h \
	qstrdefs.generated.h \
	root_pointers.h \
	)

# Define the top-level target, the generated output files.
.PHONY: all
all: micropython-embed-package

clean: clean-micropython-embed-package

.PHONY: clean-micropython-embed-package
clean-micropython-embed-package:
	$(RM) -rf $(PACKAGE_DIR)

PACKAGE_DIR ?= micropython_embed
PACKAGE_DIR_LIST = $(addprefix $(PACKAGE_DIR)/,py extmod shared/runtime shared/timeutils shared/readline genhdr port drivers/bus drivers/spi lib/uzlib lib/crypto-algorithms lib/re1.5)

.PHONY: micropython-embed-package
micropython-embed-package: $(GENHDR_OUTPUT)
	$(ECHO) "Generate micropython_embed output:"
	$(Q)$(RM) -rf $(PACKAGE_DIR_LIST)
	$(Q)$(MKDIR) -p $(PACKAGE_DIR_LIST)
	$(ECHO) "- py"
	$(Q)$(CP) $(TOP)/py/*.[ch] $(PACKAGE_DIR)/py
	$(ECHO) "- extmod (specific modules only)"
	$(Q)$(CP) $(TOP)/extmod/modtime.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/modtime.h $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/modplatform.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/modplatform.h $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/moductypes.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/misc.h $(PACKAGE_DIR)/extmod
	# Provide machine glue header for ports and its dependencies
	$(Q)$(CP) $(TOP)/extmod/modmachine.h $(PACKAGE_DIR)/extmod
	# Machine module implementation
	$(Q)$(CP) $(TOP)/extmod/modmachine.c $(PACKAGE_DIR)/extmod
	# VFS support for standard os/open
	$(Q)$(CP) $(TOP)/extmod/vfs.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/vfs_reader.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/vfs.h $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/modos.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/modbinascii.c $(PACKAGE_DIR)/extmod
	# Select module for poll-based I/O and asyncio
	$(Q)$(CP) $(TOP)/extmod/modselect.c $(PACKAGE_DIR)/extmod
	# Asyncio C acceleration module (_asyncio)
	$(Q)$(CP) $(TOP)/extmod/modasyncio.c $(PACKAGE_DIR)/extmod
	# Deflate/zlib decompression module
	$(Q)$(CP) $(TOP)/extmod/moddeflate.c $(PACKAGE_DIR)/extmod
	# Framebuffer module + font data
	$(Q)$(CP) $(TOP)/extmod/modframebuf.c $(PACKAGE_DIR)/extmod
	$(Q)$(CP) $(TOP)/extmod/font_petme128_8x8.h $(PACKAGE_DIR)/extmod
	# Machine bitstream support (compiled)
	$(Q)$(CP) $(TOP)/extmod/machine_bitstream.c $(PACKAGE_DIR)/extmod || true
	# Onewire module
	$(Q)$(CP) $(TOP)/extmod/modonewire.c $(PACKAGE_DIR)/extmod || true
	# JSON, RE, Hashlib, Random, Heapq
	$(Q)$(CP) $(TOP)/extmod/modjson.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/modre.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/modhashlib.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/modrandom.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/modheapq.c $(PACKAGE_DIR)/extmod || true
	# Machine peripheral support files (glue layer from extmod)
	# These are generic glue that connects to port-specific implementations via INCLUDEFILE
	$(Q)$(CP) $(TOP)/extmod/machine_adc.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_pwm.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_wdt.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_i2c.c $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_spi.c $(PACKAGE_DIR)/extmod || true
	# Machine peripheral headers
	$(Q)$(CP) $(TOP)/extmod/machine_mem.h $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_pinbase.h $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_pulse.h $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_signal.h $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_i2c.h $(PACKAGE_DIR)/extmod || true
	$(Q)$(CP) $(TOP)/extmod/machine_spi.h $(PACKAGE_DIR)/extmod || true
	# Drivers headers and implementations for soft I2C/SPI
	$(ECHO) "- drivers/bus"
	$(Q)$(MKDIR) -p $(PACKAGE_DIR)/drivers/bus || true
	$(Q)$(CP) $(TOP)/drivers/bus/*.h $(PACKAGE_DIR)/drivers/bus || true
	$(Q)$(CP) $(TOP)/drivers/bus/softspi.c $(PACKAGE_DIR)/drivers/bus || true
	$(Q)$(CP) $(TOP)/drivers/bus/softqspi.c $(PACKAGE_DIR)/drivers/bus || true
	$(Q)$(CP) $(TOP)/drivers/bus/qspi.h $(PACKAGE_DIR)/drivers/bus || true
	# Skip machine_uart sources for embed build; not needed unless enabling UART

	$(ECHO) "- lib/uzlib (for binascii/crc32)"
	$(Q)$(MKDIR) -p $(PACKAGE_DIR)/lib/uzlib || true
	$(Q)$(CP) $(TOP)/lib/uzlib/*.[ch] $(PACKAGE_DIR)/lib/uzlib
	# Keep all uzlib files — lz77.c includes defl_static.c, and moddeflate.c includes lz77.c

	$(ECHO) "- lib/crypto-algorithms"
	$(Q)$(MKDIR) -p $(PACKAGE_DIR)/lib/crypto-algorithms || true
	$(Q)$(CP) $(TOP)/lib/crypto-algorithms/*.[ch] $(PACKAGE_DIR)/lib/crypto-algorithms || true

	$(ECHO) "- lib/re1.5"
	$(Q)$(MKDIR) -p $(PACKAGE_DIR)/lib/re1.5 || true
	$(Q)$(CP) $(TOP)/lib/re1.5/*.[ch] $(PACKAGE_DIR)/lib/re1.5 || true

	$(ECHO) "- shared"
	$(Q)$(CP) $(TOP)/shared/runtime/gchelper.h $(PACKAGE_DIR)/shared/runtime
	$(Q)$(CP) $(TOP)/shared/runtime/gchelper_generic.c $(PACKAGE_DIR)/shared/runtime
	$(Q)$(CP) $(TOP)/shared/runtime/pyexec.h $(PACKAGE_DIR)/shared/runtime
	$(Q)$(CP) $(TOP)/shared/runtime/pyexec.c $(PACKAGE_DIR)/shared/runtime
	$(Q)$(CP) $(TOP)/shared/runtime/mpirq.h $(PACKAGE_DIR)/shared/runtime
	$(Q)$(CP) $(TOP)/shared/runtime/sys_stdio_mphal.c $(PACKAGE_DIR)/shared/runtime
	$(Q)$(MKDIR) -p $(PACKAGE_DIR)/shared/timeutils || true
	$(Q)$(CP) $(TOP)/shared/timeutils/*.h $(PACKAGE_DIR)/shared/timeutils || true
	$(Q)$(CP) $(TOP)/shared/timeutils/*.c $(PACKAGE_DIR)/shared/timeutils || true
	$(ECHO) "- shared/readline"
	$(Q)$(MKDIR) -p $(PACKAGE_DIR)/shared/readline || true
	$(Q)$(CP) $(TOP)/shared/readline/*.h $(PACKAGE_DIR)/shared/readline || true
	$(Q)$(CP) $(TOP)/shared/readline/*.c $(PACKAGE_DIR)/shared/readline || true
	$(ECHO) "- genhdr"
	$(Q)$(CP) $(GENHDR_OUTPUT) $(PACKAGE_DIR)/genhdr
	$(ECHO) "- port (embed port base files)"
	$(Q)$(CP) $(MICROPYTHON_EMBED_PORT)/port/*.[ch] $(PACKAGE_DIR)/port
	$(ECHO) "- port (Jumperless-specific files from lib/micropython/port/)"
	# Copy Jumperless-specific machine implementations
	$(Q)$(CP) ../../../lib/micropython/port/machine_pin_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_uart_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_timer_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_rtc_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_wdt_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_pwm_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_adc_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_i2c_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_spi_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/machine_bitstream_jl.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/modmachine_jl.inc $(PACKAGE_DIR)/port/ || true
	# Copy Jumperless mpconfigport.h and mphalport files
	$(Q)$(CP) ../../../lib/micropython/port/mpconfigport.h $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/mphalport.h $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/mphalport.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/micropython_embed.c $(PACKAGE_DIR)/port/ || true
	$(Q)$(CP) ../../../lib/micropython/port/micropython_embed.h $(PACKAGE_DIR)/port/ || true

# Include remaining core make rules.
include $(TOP)/py/mkrules.mk
EOF

# Build the embed port with Jumperless module
echo -e "${YELLOW}Building MicroPython embed port with Jumperless module...${NC}"
echo -e "${YELLOW}This includes built-in modules: time, machine, os, math, etc.${NC}"
cd "$MICROPYTHON_REPO_PATH/ports/embed"

# Set environment variables for the build
export MICROPYTHON_TOP="$MICROPYTHON_REPO_PATH"
export USER_C_MODULES="$PROJECT_ROOT/modules"

# Build the embed port using the modified makefile that includes extmod
make -f embed_with_extmod.mk PACKAGE_DIR="$MICROPYTHON_LOCAL_PATH/micropython_embed" V=1

# Apply Jumperless-specific patches to stock MicroPython sources
# These fix bugs that can't be addressed through mpconfigport.h or port-level overrides
echo -e "${YELLOW}Applying Jumperless patches to MicroPython sources...${NC}"
python3 "$PROJECT_ROOT/scripts/patch_micropython_sources.py" "$MICROPYTHON_LOCAL_PATH/micropython_embed"

# Copy mpconfigport.h to the micropython_embed directory so it can be found during PlatformIO compilation
echo -e "${YELLOW}Copying mpconfigport.h to micropython_embed directory...${NC}"

cp "$MICROPYTHON_LOCAL_PATH/port/mpconfigport.h" "$MICROPYTHON_LOCAL_PATH/micropython_embed/"

# Do not remove machine module; we include it properly now

# echo -e "${RED}$MICROPYTHON_LOCAL_PATH/micropython_embed/port/mphalport.h${NC}"
if [  -f "$MICROPYTHON_LOCAL_PATH/micropython_embed/port/mphalport.h" ]; then
    echo -e "${YELLOW}Removing embed port's mphalport files...${NC}"
    rm "$MICROPYTHON_LOCAL_PATH/micropython_embed/port/mphalport.h"
    rm "$MICROPYTHON_LOCAL_PATH/micropython_embed/port/mphalport.c"
fi

# Verify the build
if [ -f "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" ]; then
    QSTR_COUNT=$(grep -c "^QDEF" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    JUMPERLESS_QSTRS=$(grep -c "jumperless\|dac_set\|adc_get\|nodes_connect" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    TIME_QSTRS=$(grep -c "time\|sleep\|ticks" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    MACHINE_QSTRS=$(grep -c "machine\|Pin\|ADC\|PWM\|Timer\|RTC\|WDT\|I2C\|SPI" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    PERIPHERAL_QSTRS=$(grep -c "duty_u16\|duty_ns\|freq\|baudrate\|read_u16\|datetime\|feed\|scan\|writeto\|readfrom" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    SELECT_QSTRS=$(grep -c "select\|poll\|ipoll" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    ASYNCIO_QSTRS=$(grep -c "asyncio\|TaskQueue\|CancelledError" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    ONEWIRE_QSTRS=$(grep -c "_onewire\|crc8" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    JSON_QSTRS=$(grep -c "json\|dumps\|loads" "$MICROPYTHON_LOCAL_PATH/micropython_embed/genhdr/qstrdefs.generated.h" || true)
    
    echo -e "${GREEN}◆ MicroPython embed build successful!${NC}"
    echo -e "${GREEN}   Generated $QSTR_COUNT total QSTR definitions${NC}"
    if [ "$JUMPERLESS_QSTRS" -gt 0 ]; then
        echo -e "${GREEN}   Jumperless module QSTRs found: $JUMPERLESS_QSTRS${NC}"
    else
        echo -e "${YELLOW}   Warning: No Jumperless module QSTRs detected${NC}"
    fi
    echo -e "${GREEN}   Time module QSTRs found: $TIME_QSTRS${NC}"
    echo -e "${GREEN}   Machine module QSTRs found: $MACHINE_QSTRS${NC}"
    echo -e "${GREEN}   Peripheral method QSTRs found: $PERIPHERAL_QSTRS${NC}"
    echo -e "${GREEN}   Select/poll QSTRs found: $SELECT_QSTRS${NC}"
    echo -e "${GREEN}   Asyncio QSTRs found: $ASYNCIO_QSTRS${NC}"
    echo -e "${GREEN}   Onewire QSTRs found: $ONEWIRE_QSTRS${NC}"
    echo -e "${GREEN}   JSON QSTRs found: $JSON_QSTRS${NC}"
    echo -e "${GREEN}   Files ready for PlatformIO integration with embed API${NC}"
    echo -e "${GREEN}   Available modules: time, machine, os, math, gc, array, select, asyncio, deflate, framebuf, etc.${NC}"
else
    echo -e "${RED}◇ MicroPython embed build failed!${NC}"
    echo -e "${RED}   qstrdefs.generated.h not found${NC}"
    exit 1
fi

# Verify Jumperless module files are present
echo -e "${YELLOW}Verifying Jumperless module integration...${NC}"
if [ -f "$PROJECT_ROOT/modules/jumperless/modjumperless.c" ]; then
    echo -e "${GREEN}   ◆ Jumperless MicroPython module found${NC}"
else
    echo -e "${RED}   ◇ Jumperless MicroPython module missing${NC}"
fi

if [ -f "$PROJECT_ROOT/src/JumperlessMicroPythonAPI.cpp" ]; then
    echo -e "${GREEN}   ◆ Jumperless API wrapper found${NC}"
else
    echo -e "${RED}   ◇ Jumperless API wrapper missing${NC}"
fi

echo -e "${GREEN}◆ MicroPython embed port is ready with built-in modules!${NC}"
echo -e "${GREEN}   You can now use: mp_embed_init(), mp_embed_exec_str(), mp_embed_deinit()${NC}"
echo -e "${GREEN}   Available modules: time, machine, os, math, gc, array, binascii, select, deflate, framebuf, etc.${NC}"
echo -e "${GREEN}   C acceleration: _asyncio (full asyncio needs Python files on filesystem)${NC}"
echo -e "${GREEN}   Machine peripherals: Pin, PWM, ADC, Timer, RTC, WDT, I2C, SPI, UART${NC}"
echo -e "${GREEN}   Machine functions: reset(), unique_id(), reset_cause(), freq(), bootloader()${NC}" 