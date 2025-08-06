# Fujisan - Modern Atari Emulator
# Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
# Licensed under the MIT License. See LICENSE file for details.

cmake_minimum_required(VERSION 3.16)
project(Fujisan VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use C++14 on Windows to avoid potential SDK conflicts
if(WIN32 AND MSVC)
    set(CMAKE_CXX_STANDARD 14)
    message(STATUS "Using C++14 on Windows for SDK compatibility")
endif()

# Include ExternalProject for building libatari800
include(ExternalProject)

# Set Qt5 path for Homebrew installation (can be overridden)
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt@5/lib/cmake")
endif()

# Configure builds for macOS (Intel + Apple Silicon)
if(APPLE)
    # Set deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
    
    # Handle architecture-specific builds
    if(NOT CMAKE_OSX_ARCHITECTURES)
        # Default to current architecture if not specified
        execute_process(COMMAND uname -m OUTPUT_VARIABLE CURRENT_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(CURRENT_ARCH STREQUAL "arm64")
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
        else()
            set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for Mac OS X" FORCE)
        endif()
    endif()
    
    message(STATUS "macOS Build Architecture(s): ${CMAKE_OSX_ARCHITECTURES}")
    
endif()

# Configure Qt5 for static linking on Windows cross-compilation
if(CMAKE_CROSSCOMPILING AND WIN32)
    # Force static Qt5 linking for Windows cross-compilation
    set(Qt5_USE_STATIC_LIBS ON)
    set(Qt5_USE_STATIC_RUNTIME ON)
    message(STATUS "Configuring Qt5 for static linking (Windows cross-compilation)")
endif()

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Multimedia Network)

# ============================================================================
# BUILD LIBATARI800 AS EXTERNAL PROJECT
# ============================================================================

# Define atari800 source directory
set(ATARI800_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/atari800-src")
set(ATARI800_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/atari800-build")
set(ATARI800_LIBRARY "${ATARI800_SOURCE_DIR}/src/libatari800.a")

# Configure platform-specific build commands for atari800
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    # Native Windows build with MSYS2
    # Windows: Use MSYS2 environment for building atari800
    # GitHub Actions uses setup-msys2 which sets up environment differently
    # Look for MSYS2 bash in GitHub Actions setup-msys2 location
    find_program(MSYS2_BASH NAMES msys2.cmd msys2.CMD PATHS 
        "D:/a/_temp/setup-msys2"
        DOC "MSYS2 wrapper script")
    
    if(NOT MSYS2_BASH)
        # Fallback to traditional bash.exe locations
        find_program(MSYS2_BASH bash.exe PATHS 
            "$ENV{MSYSTEM_PREFIX}/bin"
            "D:/a/_temp/msys64/usr/bin"
            "C:/tools/msys64/usr/bin" 
            "C:/msys64/usr/bin"
            NO_DEFAULT_PATH
            DOC "MSYS2 bash executable")
    endif()
    if(NOT MSYS2_BASH)
        message(FATAL_ERROR "MSYS2 bash not found. Please install MSYS2 or ensure it's in the expected location.")
    endif()
    
    message(STATUS "Found MSYS2 bash: ${MSYS2_BASH}")
    
    # Use msys2 shell environment with proper PATH for MinGW gcc
    if(MSYS2_BASH MATCHES "msys2\.(cmd|CMD)$")
        # GitHub Actions setup-msys2 wrapper
        set(ATARI800_CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env MSYSTEM=MSYS OS=Windows_NT RUNNER_OS=Windows CI=true ${MSYS2_BASH} -c "export PATH='/mingw64/bin:/usr/bin:$PATH' && cd '<SOURCE_DIR>' && echo 'FORCING MINIMAL BUILD FOR WINDOWS' && '${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-minimal-makefile.sh' '<SOURCE_DIR>'")
        set(ATARI800_BUILD_COMMAND ${CMAKE_COMMAND} -E env MSYSTEM=MSYS ${MSYS2_BASH} -c "export PATH='/mingw64/bin:/usr/bin:$PATH' && cd '<SOURCE_DIR>' && echo '=== Starting make build ===' && make -j4 VERBOSE=1 || (echo '=== Make failed, trying single-threaded build ===' && make VERBOSE=1)")
    else()
        # Traditional bash.exe
        set(ATARI800_CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env MSYSTEM=MSYS OS=Windows_NT RUNNER_OS=Windows CI=true ${MSYS2_BASH} --login -c "export PATH='/mingw64/bin:/usr/bin:$PATH' && cd '<SOURCE_DIR>' && echo 'FORCING MINIMAL BUILD FOR WINDOWS' && '${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-minimal-makefile.sh' '<SOURCE_DIR>'")
        set(ATARI800_BUILD_COMMAND ${CMAKE_COMMAND} -E env MSYSTEM=MSYS ${MSYS2_BASH} --login -c "export PATH='/mingw64/bin:/usr/bin:$PATH' && cd '<SOURCE_DIR>' && echo '=== Starting make build ===' && make -j4 VERBOSE=1 || (echo '=== Make failed, trying single-threaded build ===' && make VERBOSE=1)")
    endif()
elseif(CMAKE_CROSSCOMPILING AND WIN32)
    # Cross-compilation for Windows (using MinGW on Linux with full autotools)
    message(STATUS "Configuring for Windows cross-compilation with autotools")
    set(ATARI800_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-libatari800-simple.sh <SOURCE_DIR>)
    set(ATARI800_BUILD_COMMAND echo "Build completed by configure script")
else()
    # Unix/macOS: Use native tools
    set(ATARI800_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/configure-atari800.sh <SOURCE_DIR>)
    set(ATARI800_BUILD_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> make -j4)
endif()

# Download and build atari800 with Fujisan patches
ExternalProject_Add(atari800_external
    GIT_REPOSITORY https://github.com/atari800/atari800.git
    GIT_TAG 575a943b3523bf0f5c9b73ba8641d83cac672f24  # Known compatible version
    SOURCE_DIR ${ATARI800_SOURCE_DIR}
    BINARY_DIR ${ATARI800_BINARY_DIR}
    
    # Apply Fujisan patches during configure step
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory 
                  "${CMAKE_CURRENT_SOURCE_DIR}/patches" 
                  "<SOURCE_DIR>/fujisan-patches"
    
    CONFIGURE_COMMAND ${ATARI800_CONFIGURE_COMMAND}
    
    BUILD_COMMAND ${ATARI800_BUILD_COMMAND}
    
    # No install step needed, we'll link directly to the library
    INSTALL_COMMAND ""
    
    # Enhanced logging and debugging
    LOG_DOWNLOAD ON
    LOG_UPDATE ON
    LOG_PATCH ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    LOG_MERGED_STDOUTERR ON
    LOG_OUTPUT_ON_FAILURE ON
    
    # Force rebuild to ensure we see all output
    BUILD_ALWAYS ON
    
    # Export the library location
    BUILD_BYPRODUCTS ${ATARI800_LIBRARY}
)

# Verify that the library was created successfully
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    # Add custom command to verify library creation on Windows
    add_custom_command(TARGET atari800_external POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=== Verifying libatari800 build on Windows ==="
        COMMAND ${CMAKE_COMMAND} -E echo "Expected library: ${ATARI800_LIBRARY}"
        COMMAND ${CMAKE_COMMAND} -E echo "Checking if library exists..."
        COMMAND ${CMAKE_COMMAND} -E env MSYSTEM=MSYS ${MSYS2_BASH} -c "export PATH='/mingw64/bin:/usr/bin:$PATH' && if [ -f '${ATARI800_LIBRARY}' ]; then echo 'SUCCESS: libatari800.a exists'; ls -la '${ATARI800_LIBRARY}'; if [ -f '${ATARI800_SOURCE_DIR}/.minimal-build-marker' ]; then echo 'INFO: Built using minimal Makefile approach'; cat '${ATARI800_SOURCE_DIR}/.minimal-build-marker'; fi; else echo 'ERROR: libatari800.a missing'; ls -la '${ATARI800_SOURCE_DIR}/src/'; if [ -f '${ATARI800_SOURCE_DIR}/.minimal-build-marker' ]; then echo 'DEBUG: Minimal build was attempted'; cat '${ATARI800_SOURCE_DIR}/.minimal-build-marker'; fi; fi"
        COMMENT "Verifying libatari800 library creation"
    )
elseif(CMAKE_CROSSCOMPILING AND WIN32)
    # Add custom command to verify library creation during cross-compilation
    add_custom_command(TARGET atari800_external POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=== Verifying libatari800 cross-compilation build ==="
        COMMAND ${CMAKE_COMMAND} -E echo "Expected library: ${ATARI800_LIBRARY}"
        COMMAND test -f "${ATARI800_LIBRARY}" && echo "SUCCESS: libatari800.a exists" || echo "ERROR: libatari800.a missing"
        COMMENT "Verifying cross-compiled libatari800 library creation"
    )
endif()

# Create imported target for libatari800
add_library(libatari800 STATIC IMPORTED)
set_target_properties(libatari800 PROPERTIES
    IMPORTED_LOCATION ${ATARI800_LIBRARY}
)

# Add dependency to ensure atari800 is built first
add_dependencies(libatari800 atari800_external)

# ============================================================================
# FUJISAN SOURCE FILES
# ============================================================================

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/emulatorwidget.cpp
    src/atariemulator.cpp
    src/toggleswitch.cpp
    src/settingsdialog.cpp
    src/disasm6502.cpp
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

# ============================================================================
# CREATE FUJISAN EXECUTABLE
# ============================================================================

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Add dependency to ensure atari800 is built before Fujisan
add_dependencies(${PROJECT_NAME} atari800_external)

# Include directories for Fujisan and atari800 headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ATARI800_SOURCE_DIR}/src/libatari800
    ${ATARI800_SOURCE_DIR}/src
)

# ============================================================================
# MACOS APP BUNDLE CONFIGURATION
# ============================================================================

if(APPLE)
    # Configure as macOS app bundle
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Fujisan"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_IDENTIFIER "com.8bitrelics.fujisan"
        MACOSX_BUNDLE_EXECUTABLE_NAME "Fujisan"
        MACOSX_BUNDLE_ICON_FILE "Fujisan.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.8bitrelics.fujisan"
        MACOSX_BUNDLE_DISPLAY_NAME "Fujisan"
        MACOSX_BUNDLE_INFO_STRING "Fujisan - Modern Atari Emulator"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2025 Paulo Garcia (8bitrelics.com)"
        MACOSX_DEPLOYMENT_TARGET "11.0"
    )
    
    # Copy app icon to bundle
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Fujisan.icns")
    if(EXISTS ${ICON_FILE})
        set_source_files_properties(${ICON_FILE} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(${PROJECT_NAME} PRIVATE ${ICON_FILE})
    else()
        message(WARNING "App icon not found: ${ICON_FILE}")
    endif()
endif()

# ============================================================================
# COMPILE DEFINITIONS AND LINKING
# ============================================================================

# Add compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    SCREENSHOTS
    # Remove dependency on runtime ATARI800_SRC_PATH
    EMBEDDED_LIBATARI800
)

# Add architecture-specific definitions for macOS
if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        target_compile_definitions(${PROJECT_NAME} PRIVATE FUJISAN_X86_64_BUILD)
    endif()
    
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        target_compile_definitions(${PROJECT_NAME} PRIVATE FUJISAN_ARM64_BUILD)
    endif()
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    libatari800
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Multimedia
    Qt5::Network
    -lm
)

# Platform-specific linking and resources
if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        "-framework Cocoa"
        "-framework UniformTypeIdentifiers"
    )
elseif(WIN32)
    # Static linking for MinGW to avoid DLL dependencies
    if(CMAKE_CROSSCOMPILING)
        # Force static Qt5 linking
        set(Qt5_USE_STATIC_LIBS ON)
        set(Qt5_USE_STATIC_RUNTIME ON)
        
        # Add static linking flags
        target_link_libraries(${PROJECT_NAME}
            -static-libgcc
            -static-libstdc++
            -static
            # Windows system libraries needed for static Qt5
            -lws2_32
            -lkernel32
            -luser32
            -lgdi32
            -lwinspool
            -lshell32
            -lole32
            -loleaut32
            -luuid
            -lcomdlg32
            -ladvapi32
            -limm32
            -lwinmm
            -lversion
            -ldwmapi
            -luxtheme
        )
        
        # Additional static linking options - force all linking to be static
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "-static-libgcc -static-libstdc++ -static -Wl,-Bstatic -Wl,--no-undefined"
        )
    endif()
    # Windows-specific compiler settings to fix SDK compatibility
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00  # Windows 10
        WINVER=0x0A00        # Windows 10
        UNICODE
        _UNICODE
        # Prevent common macro conflicts
        NOSERVICE
        NOMCX
        NOIME
        NOSOUND
        NOKANJI
        NOHELP
        NOPROFILER
        NODEFERWINDOWPOS
        NOMCX
    )
    
    # MSVC-specific compiler options
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
            /permissive-    # Disable non-conforming code
            /Zc:__cplusplus # Enable __cplusplus macro
            /utf-8          # Specify UTF-8 encoding
        )
    endif()
    
    # Add Windows application icon
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Fujisan.ico")
        # Create Windows resource file for icon
        set(WINDOWS_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/fujisan_icon.rc")
        file(WRITE ${WINDOWS_RC_FILE} "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/Fujisan.ico\"\n")
        target_sources(${PROJECT_NAME} PRIVATE ${WINDOWS_RC_FILE})
    endif()
endif()

# Enable Qt's MOC (Meta-Object Compiler)
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
)

# ============================================================================
# INSTALL TARGET
# ============================================================================

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# ============================================================================
# HELPFUL MESSAGES
# ============================================================================

# Print build information
message(STATUS "=== Fujisan Build Configuration ===")
message(STATUS "Project Version: ${PROJECT_VERSION}")
message(STATUS "Qt5 Path: ${CMAKE_PREFIX_PATH}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "libatari800 will be built automatically as external project")
message(STATUS "No ATARI800_SRC_PATH environment variable required")
if(APPLE)
    message(STATUS "macOS app bundle will be created")
    message(STATUS "Bundle ID: com.8bitrelics.fujisan")
endif()
message(STATUS "=====================================")