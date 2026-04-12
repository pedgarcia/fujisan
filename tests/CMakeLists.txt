# Fujisan Test Suite
# Uses QTest (Qt5::Test) for unit and integration tests.
# Build with: cmake -DBUILD_TESTS=ON ..

find_package(Qt5 REQUIRED COMPONENTS Test Core Widgets Gui Multimedia Network)

enable_testing()

set(FUJISAN_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(FUJISAN_INC_DIR "${CMAKE_SOURCE_DIR}/include")

# Common include directories for all test targets
set(TEST_INCLUDE_DIRS
    ${FUJISAN_INC_DIR}
    ${CMAKE_BINARY_DIR}/include  # Generated version.h
)

# Fixture directory available as a compile definition
set(TEST_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# ---------------------------------------------------------------------------
# Macro: add a QTest executable with common setup
# ---------------------------------------------------------------------------
macro(add_fujisan_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_include_directories(${TEST_NAME} PRIVATE ${TEST_INCLUDE_DIRS})
    target_compile_definitions(${TEST_NAME} PRIVATE
        TEST_FIXTURES_DIR="${TEST_FIXTURES_DIR}"
    )
    set_target_properties(${TEST_NAME} PROPERTIES AUTOMOC ON)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# ---------------------------------------------------------------------------
# 1. Settings persistence tests (standalone -- only needs QSettings)
# ---------------------------------------------------------------------------
add_fujisan_test(test_settings test_settings.cpp)
target_link_libraries(test_settings Qt5::Test Qt5::Core)

# ---------------------------------------------------------------------------
# 2. Configuration profile tests (compiles configurationprofile.cpp)
# ---------------------------------------------------------------------------
add_fujisan_test(test_profiles
    test_profiles.cpp
    ${FUJISAN_SRC_DIR}/configurationprofile.cpp
)
target_link_libraries(test_profiles Qt5::Test Qt5::Core)

# ---------------------------------------------------------------------------
# 3. ROM loading / argv-construction tests (standalone logic)
# ---------------------------------------------------------------------------
add_fujisan_test(test_rom_loading test_rom_loading.cpp)
target_link_libraries(test_rom_loading Qt5::Test Qt5::Core)

# ---------------------------------------------------------------------------
# 4. Audio pipeline tests (format, buffer sizing)
# ---------------------------------------------------------------------------
add_fujisan_test(test_audio test_audio.cpp)
target_link_libraries(test_audio Qt5::Test Qt5::Core Qt5::Multimedia)

# ---------------------------------------------------------------------------
# 5. FujiNet widget tests (needs Widgets for QPushButton / QFrame)
# ---------------------------------------------------------------------------
add_fujisan_test(test_fujinet_widget
    test_fujinet_widget.cpp
    ${FUJISAN_SRC_DIR}/fujinetwidget.cpp
    ${FUJISAN_INC_DIR}/fujinetwidget.h
)
target_link_libraries(test_fujinet_widget Qt5::Test Qt5::Core Qt5::Widgets Qt5::Gui)

# ---------------------------------------------------------------------------
# 6. FujiNet service tests (HTTP mock via QTcpServer)
# ---------------------------------------------------------------------------
add_fujisan_test(test_fujinet_service
    test_fujinet_service.cpp
    ${FUJISAN_SRC_DIR}/fujinetservice.cpp
    ${FUJISAN_INC_DIR}/fujinetservice.h
)
target_link_libraries(test_fujinet_service Qt5::Test Qt5::Core Qt5::Network Qt5::Widgets)

# ---------------------------------------------------------------------------
# 7. FujiNet process manager tests
# ---------------------------------------------------------------------------
add_fujisan_test(test_fujinet_process
    test_fujinet_process.cpp
    ${FUJISAN_SRC_DIR}/fujinetprocessmanager.cpp
    ${FUJISAN_INC_DIR}/fujinetprocessmanager.h
)
target_link_libraries(test_fujinet_process Qt5::Test Qt5::Core)

# Console subsystem: QTest -v2 output is often invisible for GUI-linked or MSYS-run
# Windows executables in CI logs.
if(WIN32)
    set_target_properties(test_fujinet_process PROPERTIES WIN32_EXECUTABLE OFF)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(test_fujinet_process PRIVATE -mconsole)
    endif()
endif()

# ---------------------------------------------------------------------------
# 8. Character injection / paste (AtariEmulator + libatari800, threaded)
# ---------------------------------------------------------------------------
set(TEST_CHARACTER_INJECTION_SRC ${FUJISAN_SRC_DIR}/atariemulator.cpp)
if(HAVE_SDL2_JOYSTICK)
    list(APPEND TEST_CHARACTER_INJECTION_SRC ${FUJISAN_SRC_DIR}/sdl2joystickmanager.cpp)
endif()
if(HAVE_SDL2_AUDIO)
    list(APPEND TEST_CHARACTER_INJECTION_SRC
        ${FUJISAN_SRC_DIR}/unifiedaudiobackend.cpp
        ${FUJISAN_SRC_DIR}/sdl2audiobackend.cpp)
endif()

add_fujisan_test(test_character_injection
    test_character_injection.cpp
    ${TEST_CHARACTER_INJECTION_SRC}
)
add_dependencies(test_character_injection atari800_external)

# Q_OBJECT in AtariEmulator / SDL2JoystickManager — headers must be visible to AUTOMOC
target_sources(test_character_injection PRIVATE ${FUJISAN_INC_DIR}/atariemulator.h)
if(HAVE_SDL2_JOYSTICK)
    target_sources(test_character_injection PRIVATE ${FUJISAN_INC_DIR}/sdl2joystickmanager.h)
endif()

target_include_directories(test_character_injection PRIVATE
    ${ATARI800_SOURCE_DIR}/src/libatari800
    ${ATARI800_SOURCE_DIR}/src
    $<$<BOOL:${HAVE_SDL2_JOYSTICK}>:${SDL2_INCLUDE_DIRS}>
    $<$<BOOL:${HAVE_SDL2_AUDIO}>:${SDL2_INCLUDE_DIRS}>
)

target_compile_definitions(test_character_injection PRIVATE
    SCREENSHOTS
    EMBEDDED_LIBATARI800
    NETSIO
    $<$<BOOL:${HAVE_SDL2_AUDIO}>:HAVE_SDL2_AUDIO>
    $<$<BOOL:${HAVE_SDL2_JOYSTICK}>:HAVE_SDL2_JOYSTICK>
)

target_link_libraries(test_character_injection
    libatari800
    Qt5::Test
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Multimedia
    Qt5::Network
)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_character_injection -lm)
endif()

if(HAVE_SDL2_JOYSTICK OR HAVE_SDL2_AUDIO)
    if(TARGET SDL2::SDL2)
        target_link_libraries(test_character_injection SDL2::SDL2)
    elseif(SDL2_LIBRARIES)
        target_link_libraries(test_character_injection ${SDL2_LIBRARIES})
    endif()
endif()

if(APPLE)
    target_link_libraries(test_character_injection
        "-framework Cocoa"
        "-framework UniformTypeIdentifiers"
    )
elseif(WIN32)
    target_compile_definitions(test_character_injection PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        UNICODE
        _UNICODE
    )
    set_target_properties(test_character_injection PROPERTIES WIN32_EXECUTABLE OFF)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(test_character_injection PRIVATE -mconsole)
    endif()
endif()

# ---------------------------------------------------------------------------
# Convenience target: build all test executables without touching atari800_external
# ---------------------------------------------------------------------------
add_custom_target(build_tests DEPENDS
    test_settings
    test_profiles
    test_rom_loading
    test_audio
    test_fujinet_widget
    test_fujinet_service
    test_fujinet_process
    test_character_injection
)
