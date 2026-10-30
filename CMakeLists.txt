# 007: Everything or Nothing (original Xbox), statically recompiled

cmake_minimum_required(VERSION 3.25)
project(game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# the SDK: sdk/ here, two folders up inside the SDK's templates, a checkout beside this repository, else downloaded
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/sdk/cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/../..)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../XboxXBERecomp/cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/../XboxXBERecomp)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../XboxXBE-Recomp/cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/../XboxXBE-Recomp)
else()
    include(FetchContent)
    FetchContent_Declare(xbe_sdk GIT_REPOSITORY https://github.com/hk21x/XboxXBERecomp.git GIT_TAG main)
    FetchContent_MakeAvailable(xbe_sdk)
    set(SDK ${xbe_sdk_SOURCE_DIR})
endif()
include(${SDK}/cmake/XbeRecomp.cmake)

file(GLOB XBE_HOST_SOURCES CONFIGURE_DEPENDS src/*/*.cpp)
list(FILTER XBE_HOST_SOURCES EXCLUDE REGEX "/src/(game|driving)/")

# the release, by assets/default.xbe: the European discs share one code (config/*.toml), the USA disc is another
# compile (config/usa_*.toml, every address ported; EON_USA picks the hooks' USA addresses, src/common/release.h)
set(EON_PREFIX "")
set(EON_USA 0)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/default.xbe)
    file(SHA1 ${CMAKE_CURRENT_SOURCE_DIR}/assets/default.xbe EON_XBE_SHA1)
    if(EON_XBE_SHA1 STREQUAL "1f4db3b7d3f5102ec7a2351e521f545ef4972be6")
        set(EON_PREFIX "usa_")
        set(EON_USA 1)
    endif()
endif()
message(STATUS "EoN release: ${EON_PREFIX}")

# partitions as xbe_symbols wrote them into config/game.toml
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS config/${EON_PREFIX}game.toml)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config/${EON_PREFIX}game.toml)
    file(STRINGS config/${EON_PREFIX}game.toml GAME_PARTITIONS REGEX "^partitions *=")
    string(REGEX MATCH "[0-9]+" GAME_PARTITIONS "${GAME_PARTITIONS}")
endif()
# no config yet: build.bat writes it with xbe_symbols, the change reconfigures
if(NOT GAME_PARTITIONS)
    set(GAME_PARTITIONS 8)
endif()

# target name = GAME_EXE in build.bat, xbe = GAME_XBE
add_xbe_image(EoN default.xbe ${EON_PREFIX}eon.toml ${GAME_PARTITIONS} game)
target_compile_definitions(EoN PRIVATE EON_USA=${EON_USA})
# the driving levels are a second xbe the game launches (XLaunchNewImage): an exe of their own, src/driving its hooks
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS config/${EON_PREFIX}driving_game.toml)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config/${EON_PREFIX}driving_game.toml)
    file(STRINGS config/${EON_PREFIX}driving_game.toml DRIVING_PARTITIONS REGEX "^partitions *=")
    string(REGEX MATCH "[0-9]+" DRIVING_PARTITIONS "${DRIVING_PARTITIONS}")
endif()
if(NOT DRIVING_PARTITIONS)
    set(DRIVING_PARTITIONS 8)
endif()
add_xbe_image(EoNDriving driving.xbe ${EON_PREFIX}driving.toml ${DRIVING_PARTITIONS} driving)
target_compile_definitions(EoNDriving PRIVATE EON_USA=${EON_USA})

# launcher/icon.ico, when there is one: the game's icon too (resource 1, which the window class loads)
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/launcher/icon.ico)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/game_icon.rc "1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/launcher/icon.ico\"\n")
    target_sources(EoN PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/game_icon.rc)
    target_sources(EoNDriving PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/game_icon.rc)
endif()
