cmake_minimum_required(VERSION 3.10)
project(MechManiaIV VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options
add_compile_options(-Wall -Wextra -g)

# Option to build with graphics (now enabled by default)
option(BUILD_WITH_GRAPHICS "Build with SDL2 graphics support" ON)
option(USE_SDL2 "Use SDL2 for graphics instead of X11" ON)

# Source directory
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/team/src)

# Common source files (physics and game logic)
set(COMMON_SOURCES
    ${SRC_DIR}/Sendable.C
    ${SRC_DIR}/Coord.C
    ${SRC_DIR}/Traj.C
    ${SRC_DIR}/Thing.C
    ${SRC_DIR}/Asteroid.C
    ${SRC_DIR}/Station.C
    ${SRC_DIR}/Ship.C
    ${SRC_DIR}/World.C
    ${SRC_DIR}/Team.C
    ${SRC_DIR}/ArgumentParser.C
    ${SRC_DIR}/ParserModern.C
    ${SRC_DIR}/GameConstants.C
)

# Network sources
set(NETWORK_SOURCES
    ${SRC_DIR}/Network.C
)

# Server sources
set(SERVER_SOURCES
    ${SRC_DIR}/ServerNet.C
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerTeam.C
)

# Client sources
set(CLIENT_SOURCES
    ${SRC_DIR}/ClientNet.C
    ${SRC_DIR}/Client.C
)

# Team AI sources
set(TEAM_SOURCES
    ${SRC_DIR}/ChromeFunk.C
)

# Graphics sources (only if enabled)
if(BUILD_WITH_GRAPHICS)
    if(USE_SDL2)
        # Find SDL2 packages
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED sdl2)
        pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
        pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

        set(GRAPHICS_SOURCES
            ${SRC_DIR}/SDL2Graphics.C
            ${SRC_DIR}/ObserverSDL.C
            ${SRC_DIR}/XPMLoader.C
            ${SRC_DIR}/SpriteManager.C
        )

        add_compile_definitions(BUILD_WITH_GRAPHICS USE_SDL2)
        message(STATUS "Building with SDL2 graphics support")
    else()
        # Original X11 graphics (kept for reference)
        find_package(X11 REQUIRED)

        set(GRAPHICS_SOURCES
            ${SRC_DIR}/Observer.C
            ${SRC_DIR}/ObserverImage.C
            ${SRC_DIR}/SpaceViewer.C
        )

        add_compile_definitions(BUILD_WITH_GRAPHICS)
        message(STATUS "Building with X11 graphics support")
    endif()
else()
    set(GRAPHICS_SOURCES)
    message(STATUS "Building without graphics support (headless mode)")
endif()

# Include directories
include_directories(${SRC_DIR})

# Create static library with common code
add_library(mm4_common STATIC
    ${COMMON_SOURCES}
    ${NETWORK_SOURCES}
)

# Server executable
add_executable(mm4serv
    ${SRC_DIR}/mm4serv.C
    ${SERVER_SOURCES}
)
target_link_libraries(mm4serv mm4_common pthread)

# Team client executable
add_executable(mm4team
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${TEAM_SOURCES}
)
target_link_libraries(mm4team mm4_common pthread)

# Groogroo team executable
add_executable(mm4team_groogroo
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/groogroo/Groogroo.C
    teams/groogroo/GetVinyl.C
    teams/groogroo/MagicBag.C
    teams/groogroo/DumbThing.C
    teams/groogroo/ReturnToBase.C
    teams/groogroo/Entry.C
)
target_link_libraries(mm4team_groogroo mm4_common pthread)
target_include_directories(mm4team_groogroo PRIVATE teams/groogroo)

# Groonew team executable
add_executable(mm4team_groonew
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/groonew/Groonew.C
    teams/groonew/GetVinyl.C
    teams/groonew/MagicBag.C
    teams/groonew/DumbThing.C
    teams/groonew/ReturnToBase.C
    teams/groonew/PathInfo.C
    teams/groonew/Pathfinding.C
)
target_link_libraries(mm4team_groonew mm4_common pthread)
target_include_directories(mm4team_groonew PRIVATE teams/groonew)

# Evo team executable
add_executable(mm4team_evo
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/evo/EvoAI.C
)
target_link_libraries(mm4team_evo mm4_common pthread)
target_include_directories(mm4team_evo PRIVATE teams/evo)

# Vortex team executable
add_executable(mm4team_vortex
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/vortex/VortexTeam.C
)
target_link_libraries(mm4team_vortex mm4_common pthread)
target_include_directories(mm4team_vortex PRIVATE teams/vortex)

# NoOp team executable (does nothing - for testing)
add_executable(mm4team_noop
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/noop/NoOp.C
)
target_link_libraries(mm4team_noop mm4_common pthread)
target_include_directories(mm4team_noop PRIVATE teams/noop)

# James Kirk team executable (demonstrates engine exploits)
add_executable(mm4team_jameskirk
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/jameskirk/JamesKirk.C
    teams/jameskirk/KobayashiMaru.C
)
target_link_libraries(mm4team_jameskirk mm4_common pthread)
target_include_directories(mm4team_jameskirk PRIVATE teams/jameskirk)

# Low Thrust test team executable (demonstrates launch re-docking bug)
add_executable(mm4team_lowthrust
    ${SRC_DIR}/mm4team.C
    ${CLIENT_SOURCES}
    ${SRC_DIR}/Server.C
    ${SRC_DIR}/ServerNet.C
    teams/test_lowthrust/LowThrust.C
)
target_link_libraries(mm4team_lowthrust mm4_common pthread)
target_include_directories(mm4team_lowthrust PRIVATE teams/test_lowthrust)

# Observer executable (only if graphics enabled)
if(BUILD_WITH_GRAPHICS)
    if(USE_SDL2)
        set(OBS_MAIN ${SRC_DIR}/mm4obs_sdl.C)
    else()
        set(OBS_MAIN ${SRC_DIR}/mm4obs.C)
    endif()

    add_executable(mm4obs
        ${OBS_MAIN}
        ${CLIENT_SOURCES}
        ${GRAPHICS_SOURCES}
        ${SRC_DIR}/ServerTeam.C  # Needed for observer
    )

    if(USE_SDL2)
        target_link_libraries(mm4obs
            mm4_common
            pthread
            ${SDL2_LIBRARIES}
            ${SDL2_IMAGE_LIBRARIES}
            ${SDL2_TTF_LIBRARIES}
        )
        target_include_directories(mm4obs PRIVATE
            ${SDL2_INCLUDE_DIRS}
            ${SDL2_IMAGE_INCLUDE_DIRS}
            ${SDL2_TTF_INCLUDE_DIRS}
        )
        target_compile_options(mm4obs PRIVATE
            ${SDL2_CFLAGS_OTHER}
            ${SDL2_IMAGE_CFLAGS_OTHER}
            ${SDL2_TTF_CFLAGS_OTHER}
        )
    else()
        target_link_libraries(mm4obs
            mm4_common
            pthread
            ${X11_LIBRARIES}
            ${X11_Xpm_LIB}
        )
        target_include_directories(mm4obs PRIVATE ${X11_INCLUDE_DIR})
    endif()
endif()

# Installation rules
install(TARGETS mm4serv mm4team DESTINATION bin)
if(BUILD_WITH_GRAPHICS)
    install(TARGETS mm4obs DESTINATION bin)
    install(DIRECTORY ${SRC_DIR}/gfx DESTINATION share/mm4)
    # Install fonts so runtime can load them relative to install prefix
    install(DIRECTORY ${SRC_DIR}/fonts DESTINATION share/mm4)
endif()

# Add a custom target for cleaning generated files
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/mm4serv
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/mm4team
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/mm4obs
)

# Create symbolic links for graphics resources in build directory
add_custom_command(
    TARGET mm4obs POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/team/src/gfx
            ${CMAKE_BINARY_DIR}/gfx
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/team/src/graphics.reg
            ${CMAKE_BINARY_DIR}/graphics.reg
    COMMENT "Setting up graphics resources for observer"
)

# Print build configuration
message(STATUS "MechMania IV Build Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Graphics Support: ${BUILD_WITH_GRAPHICS}")

# Provide a compiled-in data directory for robust resource lookups after install
if(BUILD_WITH_GRAPHICS)
    # Default install tree is <prefix>/share/mm4
    target_compile_definitions(mm4obs PRIVATE MM4_SHARE_DIR="${CMAKE_INSTALL_PREFIX}/share/mm4")
    # Link to repository fonts directory for runtime instructions
    target_compile_definitions(mm4obs PRIVATE MM4_FONTS_GITHUB_URL="https://github.com/digitalmacgyver/mechmania4/tree/master/team/src/fonts")
endif()
