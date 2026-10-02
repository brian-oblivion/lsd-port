cmake_minimum_required(VERSION 3.21..3.31)

project(lsd-port C)

set(CMAKE_C_STANDARD 11)

# The platform layer: psyz (brian-oblivion/lsd-psyz, a fork of Xeeynamo/psyz).
# It builds SDL3 from its own submodule and links it statically.
add_subdirectory(psyz/psyz EXCLUDE_FROM_ALL)

add_executable(lsd src/main.c)
target_link_libraries(lsd PRIVATE psyz)
target_compile_definitions(lsd PRIVATE PLATFORM_PC)
psyz_title(lsd "LSD: Dream Emulator")
