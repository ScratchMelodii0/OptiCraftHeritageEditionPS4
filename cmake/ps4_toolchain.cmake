# ps4_toolchain.cmake — OpenOrbis PS4 Toolchain (clang + lld) for PS4 homebrew.
#
# Same shape as cmake/wii_toolchain.cmake: self-contained, no SDK paths baked
# into flag *strings*, and idempotent across try_compile re-includes.
#
# The PS4 userland is FreeBSD 9-derived x86_64 (Jaguar). OpenOrbis compiles with
# the host clang against its own sysroot and links with ld.lld using its PIE
# linker script and crt1.o; create-fself then wraps the ELF into eboot.bin.
#
# Resolution order for the toolchain root:
#   1. -DOO_PS4_TOOLCHAIN=<path> on the CMake command line
#   2. $ENV{OO_PS4_TOOLCHAIN}
#   3. /opt/OpenOrbis/PS4Toolchain, C:/OpenOrbis/PS4Toolchain

if(PS4_TOOLCHAIN_INCLUDED)
    return()
endif()
set(PS4_TOOLCHAIN_INCLUDED YES)

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_VERSION   1)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Marks the build for CMakeLists.txt, which dispatches to cmake/ps4.cmake.
set(PLAYSTATION4 YES)

# --- SDK root -----------------------------------------------------------------
# Same "first candidate that is actually an install" rule as the Wii toolchain:
# CMAKE_HOST_* is not populated yet when this file runs, so host detection is
# not an option. link.x is the marker -- a bare directory is not an install.
set(_oo_candidates "")
if(DEFINED OO_PS4_TOOLCHAIN)
    list(APPEND _oo_candidates "${OO_PS4_TOOLCHAIN}")
endif()
if(DEFINED ENV{OO_PS4_TOOLCHAIN})
    list(APPEND _oo_candidates "$ENV{OO_PS4_TOOLCHAIN}")
endif()
list(APPEND _oo_candidates "/opt/OpenOrbis/PS4Toolchain" "C:/OpenOrbis/PS4Toolchain")

set(_oo_found "")
foreach(_cand IN LISTS _oo_candidates)
    string(REPLACE "\\" "/" _cand "${_cand}")
    if(EXISTS "${_cand}/link.x" AND IS_DIRECTORY "${_cand}/include/orbis")
        set(_oo_found "${_cand}")
        break()
    endif()
endforeach()

if(NOT _oo_found)
    message(FATAL_ERROR
        "OpenOrbis PS4 Toolchain not found. Tried: ${_oo_candidates}\n"
        "Install a release from https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain "
        "and export OO_PS4_TOOLCHAIN=<path>, or pass -DOO_PS4_TOOLCHAIN=<path>.")
endif()
set(OO_PS4_TOOLCHAIN "${_oo_found}" CACHE PATH "OpenOrbis PS4 Toolchain root" FORCE)

# Host-tool directory (create-fself, PkgTool.Core, create-gp4) per host OS.
# Probed by file rather than by CMAKE_HOST_*, for the reason given above.
if(EXISTS "${OO_PS4_TOOLCHAIN}/bin/windows/create-fself.exe")
    set(OO_PS4_TOOLS_DIR "${OO_PS4_TOOLCHAIN}/bin/windows" CACHE PATH "" FORCE)
    set(_oo_exe ".exe")
elseif(EXISTS "${OO_PS4_TOOLCHAIN}/bin/macos/create-fself")
    set(OO_PS4_TOOLS_DIR "${OO_PS4_TOOLCHAIN}/bin/macos" CACHE PATH "" FORCE)
    set(_oo_exe "")
else()
    set(OO_PS4_TOOLS_DIR "${OO_PS4_TOOLCHAIN}/bin/linux" CACHE PATH "" FORCE)
    set(_oo_exe "")
endif()

message(STATUS "PS4 build: OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN}")

# --- Compilers ----------------------------------------------------------------
# OpenOrbis uses the host LLVM (10+). Honour a preset/command-line override so a
# specific clang can be selected; otherwise take clang/ld.lld from PATH.
if(NOT CMAKE_C_COMPILER)
    find_program(_ps4_clang NAMES clang${_oo_exe} clang REQUIRED)
    set(CMAKE_C_COMPILER "${_ps4_clang}" CACHE FILEPATH "")
endif()
if(NOT CMAKE_CXX_COMPILER)
    find_program(_ps4_clangxx NAMES clang++${_oo_exe} clang++ REQUIRED)
    set(CMAKE_CXX_COMPILER "${_ps4_clangxx}" CACHE FILEPATH "")
endif()
find_program(PS4_LLD NAMES ld.lld${_oo_exe} ld.lld REQUIRED)
set(PS4_LLD "${PS4_LLD}" CACHE FILEPATH "ld.lld used for the final PS4 link")

set(CMAKE_C_COMPILER_TARGET   x86_64-pc-freebsd12-elf)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-freebsd12-elf)

# --- Machine flags ------------------------------------------------------------
# Mirrors samples/*/Makefile in the toolchain:
#   -fPIC               the eboot is a PIE loaded at a randomised base.
#   -funwind-tables     C++ exceptions/unwinding go through .eh_frame.
#   -march=btver2       Jaguar (AMD family 16h); safe on every PS4/PS4 Pro.
#   -D__PS4__/ORBIS     conventional platform macros used by third-party code.
# The sysroot is passed with -isysroot/-isystem through CMAKE_SYSROOT and the
# include directories rather than spliced into these strings.
set(_PS4_MACHDEP "-fPIC -funwind-tables -march=btver2 -D__PS4__ -D__ORBIS__")
set(CMAKE_C_FLAGS_INIT   "${_PS4_MACHDEP}")
set(CMAKE_CXX_FLAGS_INIT "${_PS4_MACHDEP}")

set(CMAKE_SYSROOT "${OO_PS4_TOOLCHAIN}")
include_directories(SYSTEM "${OO_PS4_TOOLCHAIN}/include")
# libc++ headers ship with the release archive under include/c++/v1. Must come
# before the C headers for the C++ wrappers (<cmath>, <cstdlib>) to win.
if(IS_DIRECTORY "${OO_PS4_TOOLCHAIN}/include/c++/v1")
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${OO_PS4_TOOLCHAIN}/include/c++/v1")
endif()

# --- Link ---------------------------------------------------------------------
# The link is driven by ld.lld directly, as the toolchain samples do; clang's
# FreeBSD driver would otherwise add host crt*.o and -lgcc that do not exist in
# the OpenOrbis sysroot. <FLAGS> intentionally carries only linker flags.
set(CMAKE_CXX_LINK_EXECUTABLE
    "\"${PS4_LLD}\" -m elf_x86_64 -pie --script \"${OO_PS4_TOOLCHAIN}/link.x\" --eh-frame-hdr <LINK_FLAGS> -L\"${OO_PS4_TOOLCHAIN}/lib\" <OBJECTS> -o <TARGET> <LINK_LIBRARIES> \"${OO_PS4_TOOLCHAIN}/lib/crt1.o\"")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")

# --- Cross-compile lookup rules -----------------------------------------------
set(CMAKE_FIND_ROOT_PATH "${OO_PS4_TOOLCHAIN}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The compiler check cannot link without crt1.o + the stub libraries on the
# line; probe with a static library, exactly as the PS2 and Wii presets do.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
