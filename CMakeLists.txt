cmake_minimum_required(VERSION 3.20.5)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcuboot_reset)


# Linker config for retained RAM
zephyr_linker_sources(SECTIONS src/reset/retained_ram.ld)

# Linker flag to wrap main(), allowing custom behavior
target_link_libraries(app PRIVATE "-Wl,--wrap=main")


FILE(GLOB app_sources src/*.c src/reset/reset_handler.c)
target_sources(app PRIVATE ${app_sources})