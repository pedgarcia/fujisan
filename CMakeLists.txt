# Fujisan - Modern Atari Emulator
# Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
# Licensed under the MIT License. See LICENSE file for details.

cmake_minimum_required(VERSION 3.16)
project(Fujisan VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Qt5 path for Homebrew installation
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt@5/lib/cmake")

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Multimedia)

# Find libatari800 in parent directory
set(ATARI800_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
find_library(ATARI800_LIBRARY 
    NAMES libatari800.a atari800
    PATHS ${ATARI800_ROOT}/src
    NO_DEFAULT_PATH
)

if(NOT ATARI800_LIBRARY)
    message(FATAL_ERROR "libatari800 not found. Please build it first with: cd .. && ./configure --target=libatari800 && make")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ATARI800_ROOT}/src/libatari800
    ${ATARI800_ROOT}/src
)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/emulatorwidget.cpp
    src/atariemulator.cpp
    src/toggleswitch.cpp
    src/settingsdialog.cpp
    src/configurationprofile.cpp
    src/configurationprofilemanager.cpp
    src/profileselectionwidget.cpp
    src/debuggerwidget.cpp
)

set(HEADERS
    include/mainwindow.h
    include/emulatorwidget.h
    include/atariemulator.h
    include/toggleswitch.h
    include/settingsdialog.h
    include/configurationprofile.h
    include/configurationprofilemanager.h
    include/profileselectionwidget.h
    include/debuggerwidget.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${ATARI800_LIBRARY}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Multimedia
    -lm
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        "-framework Cocoa"
        "-framework UniformTypeIdentifiers"
    )
endif()

# Enable Qt's MOC (Meta-Object Compiler)
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
)

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)