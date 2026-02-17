cmake_minimum_required(VERSION 3.20)
project(hyprsync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_SYSTEMD "Enable systemd sd_notify integration" OFF)
option(BUILD_TESTING "Build tests" ON)

# CalVer version generation (Year.Month.Number)
string(TIMESTAMP HYPRSYNC_VERSION_YEAR "%Y")
string(TIMESTAMP HYPRSYNC_VERSION_MONTH "%m")
string(TIMESTAMP HYPRSYNC_BUILD_DATE "%Y-%m-%d")

# Remove leading zero from month for cleaner version
string(REGEX REPLACE "^0" "" HYPRSYNC_VERSION_MONTH "${HYPRSYNC_VERSION_MONTH}")

# Get git commit count this month for the version number
execute_process(
    COMMAND git rev-list --count --since="${HYPRSYNC_VERSION_YEAR}-${HYPRSYNC_VERSION_MONTH}-01" HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE HYPRSYNC_VERSION_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT HYPRSYNC_VERSION_NUMBER OR HYPRSYNC_VERSION_NUMBER STREQUAL "")
    set(HYPRSYNC_VERSION_NUMBER "1")
endif()

# Get git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE HYPRSYNC_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT HYPRSYNC_GIT_COMMIT)
    set(HYPRSYNC_GIT_COMMIT "unknown")
endif()

set(HYPRSYNC_VERSION "${HYPRSYNC_VERSION_YEAR}.${HYPRSYNC_VERSION_MONTH}.${HYPRSYNC_VERSION_NUMBER}")
message(STATUS "HyprSync version: ${HYPRSYNC_VERSION} (${HYPRSYNC_GIT_COMMIT})")

set(SOURCES
    src/main.cpp
    src/daemon.cpp
    src/config.cpp
    src/watcher.cpp
    src/sync.cpp
    src/git.cpp
    src/setup.cpp
    src/cli.cpp
    src/util.cpp
    src/tui.cpp
    src/upgrade.cpp
)

add_executable(hyprsync ${SOURCES})

target_include_directories(hyprsync PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/tomlplusplus/include
    ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
    ${CMAKE_SOURCE_DIR}/include
)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/hyprsync/version.hpp.in
    ${CMAKE_BINARY_DIR}/include/hyprsync/version.hpp
)
target_include_directories(hyprsync PRIVATE ${CMAKE_BINARY_DIR}/include)

if(WITH_SYSTEMD)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    target_link_libraries(hyprsync PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(hyprsync PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    target_compile_definitions(hyprsync PRIVATE WITH_SYSTEMD)
endif()

find_package(Threads REQUIRED)
target_link_libraries(hyprsync PRIVATE Threads::Threads)

target_compile_options(hyprsync PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(vendor/googletest)
    add_subdirectory(tests)
endif()

install(TARGETS hyprsync DESTINATION bin)
install(FILES config/hyprsync.toml.example DESTINATION share/hyprsync)
install(FILES systemd/hyprsync.service DESTINATION lib/systemd/user)
