cmake_minimum_required(VERSION 3.21..3.31)

# On Linux the default build is x86_64, with the host's compiler as-is (since
# task 10 it plays as i686 does, docs/design.md). LSD_ARCH=i686 builds the
# 32-bit one through cmake/linux-i686.cmake, which keeps the PS1-sized
# layouts checked; CI builds both and keeps pointer-width warnings at 0. A
# toolchain file given on the command line (Windows, for example) takes
# precedence.
set(LSD_ARCH x86_64 CACHE STRING "Linux build: x86_64 (default) or i686")
set_property(CACHE LSD_ARCH PROPERTY STRINGS i686 x86_64)
if(NOT CMAKE_TOOLCHAIN_FILE AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
        AND LSD_ARCH STREQUAL "i686")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux-i686.cmake)
endif()

project(lsd-port C)
message(STATUS "lsd-port: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}, "
               "${CMAKE_SIZEOF_VOID_P}-byte pointers")
# A build directory keeps the compiler it was configured with, whatever
# LSD_ARCH says later.
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
# src/widescreen.c, src/pacing.c and src/draw_distance.c wrap the game's
# methods, so they build with the game's C.
add_library(lsd_game OBJECT ${LSD_GAME_SOURCES} src/widescreen.c src/pacing.c
    src/draw_distance.c)
target_include_directories(lsd_game PRIVATE ${LSD_DECOMP_DIR}/include)
target_link_libraries(lsd_game PRIVATE psyz)
# HOST_BUILD: lsddecomp's switch for any build that is not the PS1's.
target_compile_definitions(lsd_game PRIVATE HOST_BUILD)
# psyz's porting guide: <psyz.h> comes before every other header.
# Plain char is unsigned on the PS1 (lsddecomp builds with -funsigned-char).
target_compile_options(lsd_game PRIVATE -include psyz.h -funsigned-char)
# The game's main is called by the host's (src/main.c), which needs the name.
set_source_files_properties(${LSD_DECOMP_DIR}/src/main.c PROPERTIES
    COMPILE_DEFINITIONS main=lsd_game_main)

add_executable(lsd src/main.c src/controls.c src/ini.c src/settings.c)
target_link_libraries(lsd PRIVATE lsd_game psyz)

# Clang (macOS's compiler, and LSD_CFI's): without noundef it doesn't delete
# code that passes an uninitialized variable. TmdModel__RaycastFaces passes
# `p` before it is set (ignored on that first call, and retail never sets
# it, so lsddecomp can't without changing the PS1's bytes); with noundef,
# Clang compiled the function to nothing and walking in a dream crashed.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(lsd_game PRIVATE "SHELL:-Xclang -no-enable-noundef-analysis")
endif()

# LSD_SANITIZE: GCC sanitizers for the game's C, for example "address,bounds".
# ASan's global redzones catch code that reaches one static through another's
# address, which works on the PS1 only because of retail's symbol order
# (docs/tasks/08-neighbouring-statics.md). psyz's SDK files are instrumented
# too, since the game hands its vectors to libgte, which then does the
# overreading; its renderer, movie decoder and SPU core are left out, so the
# game keeps its speed. Errors do not stop the run with
# ASAN_OPTIONS=halt_on_error=0.
set(LSD_SANITIZE "" CACHE STRING "GCC -fsanitize= list for the game's C")
if(LSD_SANITIZE)
    set(LSD_SANITIZE_FLAGS -fsanitize=${LSD_SANITIZE} -fsanitize-recover=all
        -fno-omit-frame-pointer)
    target_compile_options(lsd_game PRIVATE ${LSD_SANITIZE_FLAGS})
    get_target_property(LSD_PSYZ_SOURCES psyz SOURCES)
    list(FILTER LSD_PSYZ_SOURCES INCLUDE REGEX "^src/psyz/")
    list(FILTER LSD_PSYZ_SOURCES EXCLUDE REGEX "/(libpress|psyz_spu)\\.c$")
    list(TRANSFORM LSD_PSYZ_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/psyz/psyz/)
    set_source_files_properties(${LSD_PSYZ_SOURCES} DIRECTORY psyz/psyz
        PROPERTIES COMPILE_OPTIONS "${LSD_SANITIZE_FLAGS}")
    target_link_options(lsd PRIVATE -fsanitize=${LSD_SANITIZE})
endif()

# LSD_CFI: Clang's check of every indirect call against the callee's type,
# for calls through a mistyped function pointer (wrong arity, an integer
# for a pointer, a lost return value), which compilers don't warn about.
# Pointer types count as one, so the game's C inheritance (a vtable slot
# typed with the derived class's self, filled with the base's method) passes;
# -fsanitize=function can't be told that, and its reports were all such.
# Needs Clang and lld: CC=clang, a build directory of its own. Reports
# don't stop the run; each call site reports once.
option(LSD_CFI "Clang CFI indirect-call check of the game's C and psyz" OFF)
if(LSD_CFI)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "LSD_CFI needs Clang: configure with CC=clang")
    endif()
    set(LSD_CFI_FLAGS -flto=thin -fvisibility=hidden -fsanitize=cfi-icall
        -fsanitize-cfi-icall-generalize-pointers
        -fno-sanitize-trap=cfi-icall -fsanitize-recover=cfi-icall)
    target_compile_options(lsd_game PRIVATE ${LSD_CFI_FLAGS})
    target_compile_options(psyz PRIVATE ${LSD_CFI_FLAGS})
    target_compile_options(lsd PRIVATE ${LSD_CFI_FLAGS})
    target_link_options(lsd PRIVATE -fuse-ld=lld ${LSD_CFI_FLAGS})
endif()

# MinGW: link its runtime (libgcc, winpthreads) in, so lsd.exe runs without
# the toolchain's DLLs beside it.
if(MINGW)
    target_link_options(lsd PRIVATE -static)
endif()
psyz_title(lsd "LSD: Dream Emulator")
