set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(pico_cam_shape_capture C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(lib/Config)
add_subdirectory(lib/CAM)
add_subdirectory(lib/LCD)
add_subdirectory(lib/GUI)
include_directories(./lib/Config)
include_directories(./lib/CAM)
include_directories(./lib/LCD)
include_directories(./lib/GUI)
include_directories(./src)

add_executable(${PROJECT_NAME} 
src/main.c
src/ImageData.c
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/CAM/image.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/CAM/)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(
    ${PROJECT_NAME}
    Config
	CAM
	LCD
	GUI
    pico_stdlib
	hardware_dma
	hardware_i2c
	hardware_pio
	hardware_pwm
	hardware_spi
	pico_multicore
    )
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})
