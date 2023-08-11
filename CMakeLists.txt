cmake_minimum_required(VERSION 3.24)

# Get the Pico SDK from https://github.com/raspberrypi/pico-sdk.git tested on TAG 1.4.0
# Get FreeRTOS from https://github.com/FreeRTOS/FreeRTOS-Kernel.git tested on TAG V10.5.0
set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel)
set(LIBSMB2_PATH ${CMAKE_CURRENT_LIST_DIR}/libsmb2)

# Name the project and prepare it to be a pico_w project, using FreeRTOS and lwip
set(PROJECT_NAME x68kzrmthds)
set(PICO_BOARD pico_w)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
project(${PROJECT_NAME} C CXX ASM)
pico_sdk_init()

# Add a couple of definitions that all projects/libraries can/should use
add_definitions(-DPICO_PLATFORM=${PICO_PLATFORM})
add_definitions(-DHAVE_CONFIG_H)

# Build libsmb2 as a library
add_subdirectory(${LIBSMB2_PATH} libsmb2)

# Application, including in the FreeRTOS-Kernel
add_executable(
        ${PROJECT_NAME}
        src/x68kzrmthds.c
        src/msc_disk.c
        src/virtual_disk.c
        src/config_file.c
        src/usb_descriptors.c
)

# Build the app
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${FREERTOS_KERNEL_PATH}/include
        ${LIBSMB2_PATH}/include
        ${LIBSMB2_PATH}/include/smb2
        ${LIBSMB2_PATH}/include/picow
)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_semihosting(${PROJECT_NAME} 0)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        PICO_HEAP_SIZE=0x10000
        NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
)

target_link_libraries(${PROJECT_NAME}
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel
        pico_stdlib
        libsmb2
        FreeRTOS-Kernel-Heap3
        tinyusb_device
        tinyusb_board
)

pico_add_extra_outputs(${PROJECT_NAME})