# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")
cmake_minimum_required(VERSION 3.13)

option(VGA_HDMI "Enable VGA/HDMI driver" OFF)
option(MURM2 "Murmulator 2.0" OFF)
option(PICO_PC "RP2350-PICO-PC" OFF)
option(PICO_DV "RP2350-PICO-DV" OFF)
option(ZERO2 "RP2350-PiZero" OFF)

# Display output (default: VGA_HDMI)
if(NOT VGA_HDMI)
    set(VGA_HDMI ON)
endif()

set(CPU_MHZ 378)
set (PORT_VERSION "1.2.16")
string(LENGTH "${PORT_VERSION}" PORT_VERSION_LEN)

# Target board (default: MURM2; override with -D flags, e.g. cmake -DPICO_PC=ON)
if(NOT MURM2 AND NOT PICO_PC AND NOT PICO_DV AND NOT ZERO2)
    set(MURM2 ON)
endif()

set(KBDUSB ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pull in Raspberry Pi Pico SDK (must be before project)
IF(ZERO2)
    # For Board RP2350-PiZero
    set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/src/boards)
    set(PICO_BOARD waveshare_rp2350_pizero CACHE STRING "Board type" FORCE)
    set(PICO_PLATFORM rp2350-arm-s)
    set(CPU_MHZ 252)
else()
    set(PICO_BOARD pico2 CACHE STRING "Board type" FORCE)
    set(PICO_PLATFORM rp2350-arm-s)
endif()

message(STATUS "PICO_BOARD: ${PICO_BOARD}")

include(pico_sdk_import.cmake)

SET(PROJECT_NAME pico-next)
set(BOARD_CHIP_TAG p2)
set(FLASH_SIZE 4096)

IF(MURM2)
    SET(BUILD_NAME "m2${PROJECT_NAME}")
    set(CONFIG_BOARD_TAG "m2${BOARD_CHIP_TAG}")
    set (BUTTER_PSRAM_GPIO 8)
ELSEIF(PICO_PC)
    SET(BUILD_NAME "PC${PROJECT_NAME}")
    set(CONFIG_BOARD_TAG "PC${BOARD_CHIP_TAG}")
    set (BUTTER_PSRAM_GPIO 8)
ELSEIF(PICO_DV)
    SET(BUILD_NAME "DV${PROJECT_NAME}")
    set(CONFIG_BOARD_TAG "DV${BOARD_CHIP_TAG}")
    set (BUTTER_PSRAM_GPIO 47)
ELSEIF(ZERO2)
    SET(BUILD_NAME "z0${PROJECT_NAME}")
    set(CONFIG_BOARD_TAG "z0${BOARD_CHIP_TAG}")
    set (BUTTER_PSRAM_GPIO 47)
ELSE()
    message(FATAL_ERROR "Unsupported board: enable one of MURM2, PICO_PC, PICO_DV, ZERO2")
ENDIF()

set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

project(${PROJECT_NAME} C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(drivers/audio)
add_subdirectory(drivers/picomp3lib)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/vga-nextgen)
add_subdirectory(drivers/hdmi)
add_subdirectory(drivers/graphics)
add_subdirectory(drivers/psram)

#target_compile_definitions(${PROJECT_NAME} PRIVATE VGA)

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC
    "src/*.cpp"
    "src/*.c"
    "fabgl/*.cpp"
    "src/roms/*.cpp"
    "src/roms/*.c"
    "src/miniz/*.c"
)

message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_executable(${PROJECT_NAME} ${SRC})

# КРИТИЧНАЯ ОПТИМИЗАЦИЯ: принудительно -O3 для горячего кода независимо от CMAKE_BUILD_TYPE
# Это важно для MinSizeRel (где по умолчанию -Os), чтобы не терять FPS
set_source_files_properties(src/AySound.cpp PROPERTIES
    COMPILE_FLAGS "-O3 -ffast-math -funroll-loops")
set_source_files_properties(src/Video.cpp PROPERTIES
    COMPILE_FLAGS "-O3 -funroll-loops")
# Z80 эмулятор — самый горячий код, критично для производительности
set_source_files_properties(src/Z80_JLS.cpp PROPERTIES
    COMPILE_FLAGS "-O3 -funroll-loops")
set_source_files_properties(src/CPU.cpp PROPERTIES
    COMPILE_FLAGS "-O3")
set_source_files_properties(src/Ports.cpp PROPERTIES
    COMPILE_FLAGS "-O3")
set_source_files_properties(src/wd1793.cpp PROPERTIES
    COMPILE_FLAGS "-O3")
set_source_files_properties(src/ESPectrum.cpp PROPERTIES
    COMPILE_FLAGS "-O3 -funroll-loops")
set_source_files_properties(src/pwm_audio.cpp PROPERTIES
    COMPILE_FLAGS "-O3")

# pull in common dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    psram
    audio
    graphics
    picomp3lib

    nespad
    sdcard
    fatfs

    pico_runtime
    pico_stdlib
    pico_multicore
    hardware_dma
    hardware_pio
    hardware_interp
    hardware_timer
    hardware_clocks
    hardware_pwm
    hardware_uart
    hardware_flash
    hardware_exception
    stdc++

    tinyusb_host tinyusb_board
)

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        src/roms
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(${PROJECT_NAME} pico_cyw43_arch_none)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})

# add url via pico_set_program_url

# Custom linker script reserves a fixed 128K region (RAM_PAGES01234567) for
# the 8 Z80 RAM pages backing pages 0-7 (m16c_pages: pages0123/pages46/57).
# All other code/data and the heap (which holds the framebuffer) live in a
# separate RAM region. The two regions sit in different SRAM banks with
# independent AHB ports — HDMI DMA reading the framebuffer cannot stall
# CPU access to Z80 RAM and vice versa.
pico_set_linker_script(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/rp2350-memmap.ld")

target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections)
target_compile_definitions(${PROJECT_NAME} PRIVATE FLASH_SIZE=${FLASH_SIZE})
target_compile_definitions(${PROJECT_NAME} PRIVATE CONFIG_BOARD_TAG="${CONFIG_BOARD_TAG}")

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
target_compile_options(${PROJECT_NAME} PUBLIC
        -Wno-error=suggest-attribute=format
        -Wno-error=cast-qual
        -Wno-error=unused-parameter
        -Wno-error=conversion
        -Wno-error=format=
        -Wno-error=sign-compare
        -Wno-error=missing-field-initializers
        -Wno-error=switch
        -Wno-error=implicit-fallthrough=
        -Wno-error=stringop-truncation
        -Wno-error=restrict
        -w
)
endif ()

IF(KBDUSB)
    add_subdirectory(drivers/ps2kbd)
    target_link_libraries(${PROJECT_NAME} PRIVATE ps2kbd)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KBDUSB)
ELSE()
    add_subdirectory(drivers/ps2)
    target_link_libraries(${PROJECT_NAME} PRIVATE ps2)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KBDPS2)
ENDIF()

if (BUTTER_PSRAM_GPIO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUTTER_PSRAM_GPIO=${BUTTER_PSRAM_GPIO})
endif()

IF(MURM2)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        PORT_VERSION="${PORT_VERSION}" PORT_VERSION_LEN=${PORT_VERSION_LEN}
        MURM2=1
        CPU_MHZ=${CPU_MHZ}

        KBD_CLOCK_PIN=2
        KBD_DATA_PIN=3

        LOAD_WAV_PIO=22

        CLK_AY_PIN1=21 # not in use
        LATCH_595_PIN=9
        CLK_595_PIN=10
        DATA_595_PIN=11
        CLK_AY_PIN2=29

        I2S_DATA_PIO=9 #
        I2S_BCK_PIO=10 #
        I2S_LCK_PIO=11

        BEEPER_PIN=9 #
        PWM_PIN0=10 #
        PWM_PIN1=11

        # SDCARD
        SDCARD_PIN_SPI0_CS=5
        SDCARD_PIN_SPI0_SCK=6
        SDCARD_PIN_SPI0_MOSI=7
        SDCARD_PIN_SPI0_MISO=4

        #PSRAM
        # PSRAM_MUTEX=1
        PSRAM_SPINLOCK=1
        PSRAM_ASYNC=1

        PSRAM_PIN_CS=18
        PSRAM_PIN_SCK=19
        PSRAM_PIN_MOSI=20
        PSRAM_PIN_MISO=21

        # NES Gamepad
        USE_NESPAD
        NES_GPIO_CLK=20
        NES_GPIO_LAT=21
        NES_GPIO_DATA=26

        # VGA 8 pins starts from pin:
        VGA_BASE_PIN=12

        # HDMI 8 pins starts from pin:
        HDMI_BASE_PIN=12

        DEFAULT_THROTTLING=0
        PICO_DEFAULT_LED_PIN=25
    )
ELSEIF(PICO_PC)
    # Optional UART for Debug Probe. Set PICO_PC_DBG_UART=ON to enable UART0 on the
    # on-board DBG1 header (GP0=TX, GP1=RX) of Olimex RP2040-PICO-PC.
    # DBG1 shares pins with KBD_CLOCK/KBD_DATA → keyboard is auto-remapped to FREE
    # pins GP10/GP11 when DBG_UART is enabled.
    set(PICO_PC_DBG_UART OFF CACHE BOOL "Enable Debug Probe UART0 on DBG1 header GP0/GP1 (PICO_PC)")
    if(PICO_PC_DBG_UART)
        pico_enable_stdio_uart(${PROJECT_NAME} 1)
        set(PICO_PC_KBD_CLOCK 10) # GP0 taken by UART TX → move KBD to FREE pin
        set(PICO_PC_KBD_DATA 11)  # GP1 taken by UART RX → move KBD to FREE pin
    else()
        set(PICO_PC_KBD_CLOCK 0)
        set(PICO_PC_KBD_DATA 1)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        DEBUG=0

        $<$<BOOL:${PICO_PC_DBG_UART}>:PICO_DEFAULT_UART=0>
        $<$<BOOL:${PICO_PC_DBG_UART}>:PICO_DEFAULT_UART_TX_PIN=0>
        $<$<BOOL:${PICO_PC_DBG_UART}>:PICO_DEFAULT_UART_RX_PIN=1>

        PORT_VERSION="${PORT_VERSION}" PORT_VERSION_LEN=${PORT_VERSION_LEN}
        CPU_MHZ=${CPU_MHZ}

        PICO_PC=1

        # GP-5  UXT1-10
        # GP-8  UXT1-6
        # GP-9  UXT1-5
        # GP-21 UXT1-4
        # GP-20 UXT1-3

        # QWST1 3/4 - GP2/3

        # DBG1 1/2  (GP0=UART0_TX, GP1=UART0_RX) — used by PICO_PC_DBG_UART
        KBD_CLOCK_PIN=${PICO_PC_KBD_CLOCK}
        KBD_DATA_PIN=${PICO_PC_KBD_DATA}

        LOAD_WAV_PIO=5 # UXT1-10

        CLK_AY_PIN1=21 # not in use
        LATCH_595_PIN=26
        CLK_595_PIN=27
        DATA_595_PIN=28
        CLK_AY_PIN2=29

        I2S_DATA_PIO=26 # not implemented
        I2S_BCK_PIO=26  # I2S_BCK_PIO == I2S_LCK_PIO == I2S_DATA_PIO - unsupported mark
        I2S_LCK_PIO=26

        BEEPER_PIN=26 # not implemented
        PWM_PIN0=27 # R
        PWM_PIN1=28 # L

        # SDCARD
        SDCARD_PIN_SPI0_MISO=4
        SDCARD_PIN_SPI0_SCK=6
        SDCARD_PIN_SPI0_MOSI=7
        SDCARD_PIN_SPI0_CS=22

        # PSRAM: onboard butter PSRAM at GP8 (RP2350A QMI CS1, set above).
        # No external SPI PSRAM — dummy PSRAM_PIN_* values are kept only so
        # drivers/psram/psram_spi.c compiles; init_psram() short-circuits.
        PSRAM_SPINLOCK=1
        PSRAM_ASYNC=1
        PSRAM_PIN_CS=3
        PSRAM_PIN_SCK=3
        PSRAM_PIN_MOSI=3
        PSRAM_PIN_MISO=3

        # NES Gamepad
        USE_NESPAD
        NES_GPIO_CLK=5    # UXT1-10
        NES_GPIO_LAT=9    # UXT1-5
        NES_GPIO_DATA=20  # UXT1-3
        NES_GPIO_DATA2=21 # UXT1-4

        # VGA 8 pins starts from pin:
        VGA_BASE_PIN=12

        # HDMI 8 pins starts from pin:
        HDMI_BASE_PIN=12

        DEFAULT_THROTTLING=0
        PICO_DEFAULT_LED_PIN=25
    )
ELSEIF(PICO_DV)
    # Optional UART for Debug Probe. Set DBG_UART_PINS=ON to enable UART1 on GP20/GP21.
    # Note: GP20 is also LOAD_WAV_PIO default — auto-remapped to GP3 when UART enabled.
    set(PICO_DV_DBG_UART OFF CACHE BOOL "Enable Debug Probe UART1 on GP20/GP21 (PICO_DV)")
    if(PICO_DV_DBG_UART)
        pico_enable_stdio_uart(${PROJECT_NAME} 1)
        set(PICO_DV_LOAD_WAV_PIO 3)  # GP20 taken by UART → use FREE pin
    else()
        set(PICO_DV_LOAD_WAV_PIO 20)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        DEBUG=0

        $<$<BOOL:${PICO_DV_DBG_UART}>:PICO_DEFAULT_UART=1>
        $<$<BOOL:${PICO_DV_DBG_UART}>:PICO_DEFAULT_UART_TX_PIN=20>
        $<$<BOOL:${PICO_DV_DBG_UART}>:PICO_DEFAULT_UART_RX_PIN=21>

        PORT_VERSION="${PORT_VERSION}" PORT_VERSION_LEN=${PORT_VERSION_LEN}
        CPU_MHZ=${CPU_MHZ}

        PICO_DV=1

        # DBG1 1/2
        KBD_CLOCK_PIN=0
        KBD_DATA_PIN=1

        LOAD_WAV_PIO=${PICO_DV_LOAD_WAV_PIO}

        CLK_AY_PIN1=21 # not in use
        LATCH_595_PIN=26
        CLK_595_PIN=27
        DATA_595_PIN=28
        CLK_AY_PIN2=29

        I2S_DATA_PIO=26 #
        I2S_BCK_PIO=27 #
        I2S_LCK_PIO=28

        PWM_PIN0=26 # R
        PWM_PIN1=27 # L
        BEEPER_PIN=28 # not implemented

        # SDCARD
        SDCARD_SPI_BUS=spi1
        SDCARD_PIN_SPI0_MISO=19
        SDCARD_PIN_SPI0_SCK=5
        SDCARD_PIN_SPI0_MOSI=18
        SDCARD_PIN_SPI0_CS=22

        SDCARD_PIO=pio1
        SDCARD_PIO_SM=0

        # PSRAM: onboard butter PSRAM at GP47 (Pimoroni Pico Plus 2 / RP2350B QMI CS1).
        # No external SPI PSRAM on this target — the dummy PSRAM_PIN_* values
        # below only exist so drivers/psram/psram_spi.c compiles; init_psram()
        # short-circuits to size=0 because PSRAM is not defined.
        PSRAM_SPINLOCK=1
        PSRAM_ASYNC=1
        PSRAM_PIN_CS=3
        PSRAM_PIN_SCK=3
        PSRAM_PIN_MOSI=3
        PSRAM_PIN_MISO=3

        # NES Gamepad
        NES_GPIO_CLK=8    # UXT1-6
        NES_GPIO_LAT=9    # UXT1-5
        NES_GPIO_DATA=20  # UXT1-3
        NES_GPIO_DATA2=21 # UXT1-4

        # VGA 8 pins starts from pin:
        VGA_BASE_PIN=6

        # HDMI 8 pins starts from pin:
        HDMI_BASE_PIN=6

        DEFAULT_THROTTLING=0
        PICO_DEFAULT_LED_PIN=25
)
ELSEIF(ZERO2)
    set(ZERO2_DBG_UART OFF CACHE BOOL "Enable Debug Probe UART1 on GP20/GP21 (ZERO2)")
    if(ZERO2_DBG_UART)
        pico_enable_stdio_uart(${PROJECT_NAME} 1)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        DEBUG=0
        $<$<BOOL:${ZERO2_DBG_UART}>:PICO_DEFAULT_UART=1>
        $<$<BOOL:${ZERO2_DBG_UART}>:PICO_DEFAULT_UART_TX_PIN=20>
        $<$<BOOL:${ZERO2_DBG_UART}>:PICO_DEFAULT_UART_RX_PIN=21>

        PORT_VERSION="${PORT_VERSION}" PORT_VERSION_LEN=${PORT_VERSION_LEN}
        CPU_MHZ=${CPU_MHZ}
        ZERO2=1

        # PS2 keyboard
        KBD_CLOCK_PIN=14
        KBD_DATA_PIN=15

        LOAD_WAV_PIO=17

        CLK_AY_PIN1=8 # not in use
        LATCH_595_PIN=10
        CLK_595_PIN=11
        DATA_595_PIN=12
        CLK_AY_PIN2=7

        I2S_DATA_PIO=10 # DIN
        I2S_BCK_PIO=11 # BCK
        I2S_LCK_PIO=12 # LRCK

        PWM_PIN0=10
        PWM_PIN1=11
        BEEPER_PIN=12

        # SDCARD
        SDCARD_SPI_BUS=spi1
        SDCARD_PIN_SPI0_SCK=30
        SDCARD_PIN_SPI0_MOSI=31
        SDCARD_PIN_SPI0_MISO=40
        SDCARD_PIN_SPI0_CS=43

        # PSRAM
        # PSRAM_MUTEX=1
        PSRAM_SPINLOCK=0
        PSRAM_ASYNC=1

        PSRAM_PIN_CS=0
        PSRAM_PIN_SCK=0
        PSRAM_PIN_MOSI=0
        PSRAM_PIN_MISO=0

        # NES Gamepad
        USE_NESPAD
        NES_GPIO_CLK=4
        NES_GPIO_LAT=5
        NES_GPIO_DATA=7
        NES_GPIO_DATA2=8

        # VGA 8 pins starts from pin:
        VGA_BASE_PIN=32

        # HDMI 8 pins starts from pin:
        HDMI_BASE_PIN=32

        PICO_DEFAULT_LED_PIN=25
        DEFAULT_THROTTLING=0
    )
ELSE()
    message(FATAL_ERROR "Unsupported board: enable one of MURM2, PICO_PC, PICO_DV, ZERO2")
ENDIF()

target_link_libraries(${PROJECT_NAME} PRIVATE hdmi)
target_link_libraries(${PROJECT_NAME} PRIVATE vga-nextgen)
target_compile_definitions(${PROJECT_NAME} PRIVATE VGA_HDMI)
SET(BUILD_NAME "${BUILD_NAME}-VGA-HDMI")

SET(BUILD_NAME "${BUILD_NAME}-${CPU_MHZ}MHz-${PORT_VERSION}")

message(STATUS "BUILD_NAME: ${BUILD_NAME}")

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PICO_BUILD_NAME="${BUILD_NAME}"
    PICO_GIT_BRANCH="${GIT_BRANCH}"
    PICO_GIT_COMMIT="${GIT_COMMIT_HASH}"
)
