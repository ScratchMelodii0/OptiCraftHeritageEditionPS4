# ps4.cmake — PlayStation 4 homebrew (OpenOrbis) build branch for OptiCraft.
#
# Included from the top of CMakeLists.txt when cmake/ps4_toolchain.cmake is
# active (it sets PLAYSTATION4), which then return()s so none of the desktop
# SDL2/glad/OpenGL configuration runs. Structured to mirror cmake/wii.cmake.
#
# Two build shapes:
#
#   PS4_BRINGUP=ON
#       Only the toolchain/hardware smoke test (src/ps4/tools/Ps4Bringup.cpp):
#       Piglet/EGL context, DualShock 4 polling, clear-colour cycling. Proves
#       the OpenOrbis install links, signs and boots before any game code.
#
#   PS4_BRINGUP=OFF
#       The full game, with every platform backend selected as *_PS4 /
#       *_GLES_PS4. Requires the complete src/ps4 backend set.
#
# Output (bin/ps4/):
#   OptiCraft.elf        linked PIE, for symbol lookups / debugging
#   pkg/eboot.bin        fake-signed SELF (create-fself)
#   pkg/sce_sys/...      param.sfo + icon0.png
#   <CONTENT_ID>.pkg     installable package (target: ps4-pkg)

cmake_minimum_required(VERSION 3.21)

include(${CMAKE_SOURCE_DIR}/cmake/SourceSelection.cmake)

# --- PS4 feature options ------------------------------------------------------
option(PS4_BRINGUP "Build only the PS4 toolchain smoke test instead of the game" OFF)
option(PS4_ENABLE_SOUND "Enable the libSceAudioOut sound backend" ON)
option(PS4_ENABLE_NETWORK "Enable TCP multiplayer through libSceNet sockets" ON)
set(MC_LOG_LEVEL "0" CACHE STRING "Unified diagnostic verbosity: 0=off, 1=info, 2=debug, 3=trace")
set_property(CACHE MC_LOG_LEVEL PROPERTY STRINGS 0 1 2 3)

# Package identity. The title id must be 4 letters + 5 digits; BREW* is the
# homebrew convention. CONTENT_ID layout: XXYYYY-TITLEID_00-<16 chars>.
set(PS4_TITLE      "OptiCraft Heritage" CACHE STRING "PS4 package title")
set(PS4_TITLE_ID   "OPTC00173"          CACHE STRING "PS4 title id (4 letters + 5 digits)")
set(PS4_CONTENT_ID "IV0000-OPTC00173_00-OPTICRAFTHERITAG" CACHE STRING "PS4 content id")
set(PS4_APP_VER    "01.00"              CACHE STRING "PS4 package version (NN.NN)")

string(LENGTH "${PS4_TITLE_ID}" _ps4_tid_len)
if(NOT _ps4_tid_len EQUAL 9)
    message(FATAL_ERROR "PS4_TITLE_ID must be exactly 9 characters (e.g. OPTC00173), got '${PS4_TITLE_ID}'")
endif()
string(LENGTH "${PS4_CONTENT_ID}" _ps4_cid_len)
if(NOT _ps4_cid_len EQUAL 36)
    message(FATAL_ERROR "PS4_CONTENT_ID must be exactly 36 characters, got '${PS4_CONTENT_ID}'")
endif()

# --- Source selection ---------------------------------------------------------
if(PS4_BRINGUP)
    set(PS4_SOURCES
        "${CMAKE_SOURCE_DIR}/src/ps4/tools/Ps4Bringup.cpp"
        "${CMAKE_SOURCE_DIR}/src/ps4/system/Ps4Modules.cpp"
        "${CMAKE_SOURCE_DIR}/src/ps4/system/Ps4Piglet.cpp"
        "${CMAKE_SOURCE_DIR}/src/ps4/system/Ps4DebugLog.cpp"
    )
    message(STATUS "PS4 build: BRINGUP (toolchain smoke test only)")
else()
    mcbeta_collect_platform_sources(PS4_SOURCES ps4)
    set(PS4_MINIZIP_SOURCES
        "${CMAKE_SOURCE_DIR}/external/zlib/contrib/minizip/ioapi.c"
        "${CMAKE_SOURCE_DIR}/external/zlib/contrib/minizip/unzip.c"
    )
    list(APPEND PS4_SOURCES ${PS4_MINIZIP_SOURCES})
    # The OpenOrbis libc has no fopen64/ftello64 family; the archives are far
    # below 2 GB, so keep minizip on the 32-bit stdio API (same as the Wii).
    set_source_files_properties(${PS4_MINIZIP_SOURCES}
        PROPERTIES COMPILE_DEFINITIONS USE_FILE32API
    )

    # PS4 stores stats locally; keep the desktop synchronizer out of the target.
    mcbeta_exclude_remote_stats_sources(PS4_SOURCES)

    # The PS4 renders terrain through the desktop display-list path
    # (PLATFORM_DISPLAY_LISTS); its RenderList replay is backend-neutral and
    # lives with the desktop sources, so it is taken from there by name.
    list(APPEND PS4_SOURCES "${CMAKE_SOURCE_DIR}/src/pc/minecraft/RenderList.cpp")

    # stb_vorbis decodes the .ogg assets for the libSceAudioOut mixer.
    list(APPEND PS4_SOURCES "${CMAKE_SOURCE_DIR}/src/pc/external/stb_vorbis.cpp")

    # Without sound the libSceAudioOut mixer is not linked at all.
    if(NOT PS4_ENABLE_SOUND)
        mcbeta_exclude_sources(PS4_SOURCES "[/\\]ps4[/\\]audio[/\\]")
    endif()

    # JavaNetwork.cpp is the SDL_net desktop backend.
    if(PS4_ENABLE_NETWORK)
        mcbeta_exclude_sources(PS4_SOURCES "[/\\]java[/\\]JavaNetwork\\.cpp$")
    else()
        mcbeta_exclude_sources(PS4_SOURCES "[/\\]ps4[/\\](JavaNetwork_ps4|network[/\\].*)\\.cpp$")
    endif()

    mcbeta_exclude_sources(PS4_SOURCES "[/\\]ps4[/\\]tools[/\\]")
    mcbeta_select_platform_backends(PS4_SOURCES PS4 GLES_PS4 PS4)
    message(STATUS "PS4 build: FULL game sources")
endif()

# --- Target -------------------------------------------------------------------
add_executable(OptiCraft ${PS4_SOURCES})
set_target_properties(OptiCraft PROPERTIES
    SUFFIX ".elf"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/ps4"
)

target_compile_options(OptiCraft PRIVATE
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<COMPILE_LANGUAGE:CXX>:-frtti>
    -fno-math-errno
    -fno-trapping-math
    # The OpenOrbis headers declare many prototypes without parameters.
    -Wno-deprecated-non-prototype
    -Wno-unknown-warning-option
)

target_compile_definitions(OptiCraft PRIVATE
    "PS4_PLATFORM"
    "MC_LOG_LEVEL=${MC_LOG_LEVEL}"
    $<$<NOT:$<BOOL:${PS4_ENABLE_NETWORK}>>:NO_NETWORK>
    $<$<BOOL:${PS4_ENABLE_NETWORK}>:PS4_ENABLE_NETWORK=1>
    $<$<NOT:$<BOOL:${PS4_ENABLE_SOUND}>>:NO_SOUND>
)

target_include_directories(OptiCraft PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/pc"
    "${CMAKE_SOURCE_DIR}/src/ps4"
    "${CMAKE_SOURCE_DIR}/external/stb"
    "${CMAKE_SOURCE_DIR}/external/zlib"
    "${CMAKE_SOURCE_DIR}/external/zlib/contrib/minizip"
)

# System libraries resolve to the stub .so files OpenOrbis generates under
# $OO_PS4_TOOLCHAIN/lib; the real .sprx modules come from the console firmware.
set(PS4_SYSTEM_LIBS
    c kernel c++
    SceSysmodule SceSystemService SceUserService
    SceVideoOut ScePad
    ScePigletv2VSH
)
if(PS4_ENABLE_SOUND)
    list(APPEND PS4_SYSTEM_LIBS SceAudioOut)
endif()
if(PS4_ENABLE_NETWORK)
    list(APPEND PS4_SYSTEM_LIBS SceNet SceNetCtl)
endif()
if(NOT PS4_BRINGUP)
    list(APPEND PS4_SYSTEM_LIBS z)
endif()
foreach(_lib IN LISTS PS4_SYSTEM_LIBS)
    target_link_libraries(OptiCraft PRIVATE "-l${_lib}")
endforeach()

# --- Packaging ----------------------------------------------------------------
set(PS4_PKG_ROOT "${CMAKE_SOURCE_DIR}/bin/ps4/pkg")
file(MAKE_DIRECTORY "${PS4_PKG_ROOT}/sce_sys/about")

set(PS4_CREATE_FSELF "${OO_PS4_TOOLS_DIR}/create-fself${CMAKE_EXECUTABLE_SUFFIX}")
set(PS4_PKGTOOL      "${OO_PS4_TOOLS_DIR}/PkgTool.Core${CMAKE_EXECUTABLE_SUFFIX}")
set(PS4_CREATE_GP4   "${OO_PS4_TOOLS_DIR}/create-gp4${CMAKE_EXECUTABLE_SUFFIX}")
if(EXISTS "${OO_PS4_TOOLS_DIR}/create-fself.exe")
    set(PS4_CREATE_FSELF "${OO_PS4_TOOLS_DIR}/create-fself.exe")
    set(PS4_PKGTOOL      "${OO_PS4_TOOLS_DIR}/PkgTool.Core.exe")
    set(PS4_CREATE_GP4   "${OO_PS4_TOOLS_DIR}/create-gp4.exe")
endif()

if(EXISTS "${PS4_CREATE_FSELF}")
    # --paid 0x3800000000000011 is the homebrew program authority id every
    # OpenOrbis sample uses; it grants the SceShellCore privileges Piglet needs.
    add_custom_command(TARGET OptiCraft POST_BUILD
        COMMAND "${PS4_CREATE_FSELF}"
                "-in=$<TARGET_FILE:OptiCraft>"
                "-out=${CMAKE_BINARY_DIR}/OptiCraft.oelf"
                --eboot "${PS4_PKG_ROOT}/eboot.bin"
                --paid 0x3800000000000011
        COMMENT "create-fself: ${PS4_PKG_ROOT}/eboot.bin"
        VERBATIM
    )
else()
    message(WARNING "PS4 build: create-fself not found at ${PS4_CREATE_FSELF}; no eboot.bin will be produced.")
endif()

configure_file("${CMAKE_SOURCE_DIR}/src/ps4/sce_sys/icon0.png"
               "${PS4_PKG_ROOT}/sce_sys/icon0.png" COPYONLY)
# right.sprx is the module-rights blob every fake PKG carries; the toolchain
# ships it with its samples.
set(_ps4_right "${OO_PS4_TOOLCHAIN}/samples/hello_world/sce_sys/about/right.sprx")
if(EXISTS "${_ps4_right}")
    configure_file("${_ps4_right}" "${PS4_PKG_ROOT}/sce_sys/about/right.sprx" COPYONLY)
endif()

if(EXISTS "${PS4_PKGTOOL}")
    set(_sfo "${PS4_PKG_ROOT}/sce_sys/param.sfo")
    add_custom_command(OUTPUT "${_sfo}"
        COMMAND "${PS4_PKGTOOL}" sfo_new "${_sfo}"
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" APP_TYPE --type Integer --maxsize 4 --value 1
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" APP_VER --type Utf8 --maxsize 8 --value "${PS4_APP_VER}"
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" ATTRIBUTE --type Integer --maxsize 4 --value 0
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" CATEGORY --type Utf8 --maxsize 4 --value gd
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" CONTENT_ID --type Utf8 --maxsize 48 --value "${PS4_CONTENT_ID}"
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" SYSTEM_VER --type Integer --maxsize 4 --value 0
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" TITLE --type Utf8 --maxsize 128 --value "${PS4_TITLE}"
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" TITLE_ID --type Utf8 --maxsize 12 --value "${PS4_TITLE_ID}"
        COMMAND "${PS4_PKGTOOL}" sfo_setentry "${_sfo}" VERSION --type Utf8 --maxsize 8 --value "${PS4_APP_VER}"
        COMMENT "PkgTool: param.sfo (${PS4_TITLE_ID})"
        VERBATIM
    )
    add_custom_target(ps4-sfo ALL DEPENDS "${_sfo}")

    # Stage the game data next to eboot.bin (/app0/data on the console) and
    # build the installable package. Separate from ALL for the same reason the
    # Wii keeps wii-data separate: thousands of copies dwarf the link.
    add_custom_target(ps4-pkg
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PS4_PKG_ROOT}/data"
        COMMAND ${CMAKE_COMMAND}
                -DSRC="${CMAKE_SOURCE_DIR}/data" -DDST="${PS4_PKG_ROOT}/data"
                -P "${CMAKE_CURRENT_LIST_DIR}/ps4_stage_data.cmake"
        COMMAND ${CMAKE_COMMAND}
                -DCREATE_GP4="${PS4_CREATE_GP4}" -DPKGTOOL="${PS4_PKGTOOL}"
                -DPKG_ROOT="${PS4_PKG_ROOT}" -DCONTENT_ID="${PS4_CONTENT_ID}"
                -DOUT_DIR="${CMAKE_SOURCE_DIR}/bin/ps4"
                -P "${CMAKE_CURRENT_LIST_DIR}/ps4_make_pkg.cmake"
        DEPENDS OptiCraft ps4-sfo
        COMMENT "Building ${PS4_CONTENT_ID}.pkg"
        VERBATIM
    )
else()
    message(WARNING "PS4 build: PkgTool.Core not found at ${PS4_PKGTOOL}; param.sfo/.pkg will not be produced.")
endif()
