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
)
