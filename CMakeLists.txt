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
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Multimedia Network)

# Find libatari800 using environment variable
if(NOT DEFINED ENV{ATARI800_SRC_PATH})
    message(FATAL_ERROR "ATARI800_SRC_PATH environment variable not set. Please set it to the atari800 source directory.")
endif()

set(ATARI800_ROOT $ENV{ATARI800_SRC_PATH})
message(STATUS "Using Atari800 source from: ${ATARI800_ROOT}")

find_library(ATARI800_LIBRARY 
    NAMES libatari800.a atari800
    PATHS ${ATARI800_ROOT}/src
    NO_DEFAULT_PATH
)

if(NOT ATARI800_LIBRARY)
    message(FATAL_ERROR "libatari800 not found at ${ATARI800_ROOT}/src. Please build it first with: cd ${ATARI800_ROOT} && ./configure --target=libatari800 && make")
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
    src/diskdrivewidget.cpp
    src/diskdrawerwidget.cpp
    src/cassettewidget.cpp
    src/cartridgewidget.cpp
    src/joystickswapwidget.cpp
    src/volumeknob.cpp
    src/printerwidget.cpp
    src/mediaperipheralsdock.cpp
    src/tcpserver.cpp
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
    include/diskdrivewidget.h
    include/diskdrawerwidget.h
    include/cassettewidget.h
    include/cartridgewidget.h
    include/joystickswapwidget.h
    include/volumeknob.h
    include/printerwidget.h
    include/mediaperipheralsdock.h
    include/tcpserver.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Add compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE SCREENSHOTS)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${ATARI800_LIBRARY}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Multimedia
    Qt5::Network
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