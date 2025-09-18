cmake_minimum_required(VERSION 3.10)
project(MechManiaIV VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options
add_compile_options(-Wall -Wextra -g)

# Option to build with graphics (disabled by default for initial testing)
option(BUILD_WITH_GRAPHICS "Build with SDL2 graphics support" OFF)

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
    ${SRC_DIR}/Parser.C
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
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)

    set(GRAPHICS_SOURCES
        ${SRC_DIR}/Observer.C
        ${SRC_DIR}/ObserverImage.C
        ${SRC_DIR}/SpaceViewer.C
    )

    add_compile_definitions(BUILD_WITH_GRAPHICS)
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

# Observer executable (only if graphics enabled)
if(BUILD_WITH_GRAPHICS)
    add_executable(mm4obs
        ${SRC_DIR}/mm4obs.C
        ${CLIENT_SOURCES}
        ${GRAPHICS_SOURCES}
    )
    target_link_libraries(mm4obs
        mm4_common
        pthread
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
    )
    target_include_directories(mm4obs PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

# Installation rules
install(TARGETS mm4serv mm4team DESTINATION bin)
if(BUILD_WITH_GRAPHICS)
    install(TARGETS mm4obs DESTINATION bin)
    install(DIRECTORY ${SRC_DIR}/gfx DESTINATION share/mm4)
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

# Print build configuration
message(STATUS "MechMania IV Build Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Graphics Support: ${BUILD_WITH_GRAPHICS}")