cmake_minimum_required(VERSION 3.21..3.31)

project(lsd-port C)

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
target_compile_definitions(lsd_game PRIVATE PLATFORM_PC)
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
target_compile_definitions(lsd PRIVATE PLATFORM_PC)
psyz_title(lsd "LSD: Dream Emulator")
