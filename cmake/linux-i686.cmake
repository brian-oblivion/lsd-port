# 32-bit Linux (i686) on an x86_64 host: the port's default build
# (docs/design.md, "Pointer width"). Needs the multilib C library and the
# 32-bit development libraries SDL3 configures against.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_FLAGS_INIT -m32)
set(CMAKE_CXX_FLAGS_INIT -m32)
set(CMAKE_EXE_LINKER_FLAGS_INIT -m32)
set(CMAKE_SHARED_LINKER_FLAGS_INIT -m32)
set(CMAKE_MODULE_LINKER_FLAGS_INIT -m32)

# pkg-config must find the 32-bit .pc files: /usr/lib32 on Arch,
# /usr/lib/i386-linux-gnu on Debian and Ubuntu.
foreach(dir /usr/lib/i386-linux-gnu/pkgconfig /usr/lib32/pkgconfig)
    if(IS_DIRECTORY ${dir})
        list(APPEND LSD_PKG_CONFIG_LIBDIR ${dir})
    endif()
endforeach()
list(APPEND LSD_PKG_CONFIG_LIBDIR /usr/share/pkgconfig)
list(JOIN LSD_PKG_CONFIG_LIBDIR ":" LSD_PKG_CONFIG_LIBDIR)
set(ENV{PKG_CONFIG_LIBDIR} "${LSD_PKG_CONFIG_LIBDIR}")
if(EXISTS /usr/lib/i386-linux-gnu)
    set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
endif()
