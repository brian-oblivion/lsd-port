cmake_minimum_required(VERSION 3.21..3.31)

# 32-bit first (docs/design.md): on Linux the default build is i686, through
# cmake/linux-i686.cmake. LSD_ARCH=x86_64 builds the 64-bit one with the
# host's compiler as-is; CI keeps it compiling. A toolchain file given on the
# command line (Windows, for example) takes precedence.
set(LSD_ARCH i686 CACHE STRING "Linux build: i686 (default) or x86_64")
set_property(CACHE LSD_ARCH PROPERTY STRINGS i686 x86_64)
if(NOT CMAKE_TOOLCHAIN_FILE AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
        AND LSD_ARCH STREQUAL "i686")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux-i686.cmake)
endif()

project(lsd-port C)
message(STATUS "lsd-port: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}, "
               "${CMAKE_SIZEOF_VOID_P}-byte pointers")
# A build directory configured before i686 became the default keeps its
# 64-bit compiler settings, whatever LSD_ARCH says.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(LSD_ARCH STREQUAL "i686" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "LSD_ARCH is i686 but this build directory "
            "compiles 64-bit: configure a new one, or pass -DLSD_ARCH=x86_64")
    elseif(LSD_ARCH STREQUAL "x86_64" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "LSD_ARCH is x86_64 but this build directory "
            "compiles 32-bit: configure a new one")
    endif()
endif()

set(CMAKE_C_STANDARD 11)

# The platform layer: psyz (brian-oblivion/lsd-psyz, a fork of Xeeynamo/psyz).
# It builds SDL3 from its own submodule and links it statically.
add_subdirectory(psyz/psyz EXCLUDE_FROM_ALL)

# The game's C, from lsddecomp (decomp/). src/psyq/ is Sony code carried as C
# for the byte-matching build; psyz replaces it on the host.
# LSD_DECOMP_DIR can point at a full lsddecomp checkout, to try changes there
# before they are pushed and pinned.
set(LSD_DECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/decomp" CACHE PATH
    "lsddecomp source tree")
file(GLOB_RECURSE LSD_GAME_SOURCES CONFIGURE_DEPENDS ${LSD_DECOMP_DIR}/src/*.c)
list(FILTER LSD_GAME_SOURCES EXCLUDE REGEX "/src/psyq/")
add_library(lsd_game OBJECT ${LSD_GAME_SOURCES})
target_include_directories(lsd_game PRIVATE ${LSD_DECOMP_DIR}/include)
target_link_libraries(lsd_game PRIVATE psyz)
# HOST_BUILD: lsddecomp's switch for any build that is not the PS1's.
target_compile_definitions(lsd_game PRIVATE HOST_BUILD)
# psyz's porting guide: <psyz.h> comes before every other header.
# Plain char is unsigned on the PS1 (lsddecomp builds with -funsigned-char).
target_compile_options(lsd_game PRIVATE -include psyz.h -funsigned-char)
# GCC 14 made these errors. In the game's C they are portability debt (an
# address passed as u32 *, an integer as a pointer), counted as warnings
# until the pointer-width work (docs/design.md) removes them.
target_compile_options(lsd_game PRIVATE
    -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)

# The game's main is called by the host's (src/main.c), which needs the name.
set_source_files_properties(${LSD_DECOMP_DIR}/src/main.c PROPERTIES
    COMPILE_DEFINITIONS main=lsd_game_main)

# src/stubs.c: temporary stand-ins for what psyz and lsddecomp lack yet.
add_executable(lsd src/main.c src/stubs.c)
target_link_libraries(lsd PRIVATE lsd_game psyz)
psyz_title(lsd "LSD: Dream Emulator")
