cmake_minimum_required(VERSION 3.24)

project(libumm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static (decision M4c: static default)" OFF)
option(UMM_BUILD_TESTS "Build unit tests" ON)
option(UMM_ENABLE_FAILING_SELFTEST "Build and register the deliberately failing self-test" OFF)
option(UMM_REQUIRE_EXIV2 "Require Exiv2 at configure time (placeholder; no effect yet)" OFF)
option(UMM_REQUIRE_EXIFTOOL "Require ExifTool at configure time (placeholder; no effect yet)" OFF)

add_library(umm src/version.cpp)
add_library(umm::umm ALIAS umm)

target_include_directories(umm
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_definitions(umm
  PRIVATE
    "UMM_VERSION=\"${PROJECT_VERSION}\""
)

set_target_properties(umm PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

if(UMM_BUILD_TESTS)
  enable_testing()

  add_executable(test_version tests/unit/test_version.cpp)
  target_link_libraries(test_version PRIVATE umm::umm)
  target_compile_definitions(test_version
    PRIVATE
      "UMM_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  )
  add_test(NAME test_version COMMAND test_version)

  if(UMM_ENABLE_FAILING_SELFTEST)
    add_executable(test_failing_selftest tests/unit/test_failing_selftest.cpp)
    add_test(NAME test_failing_selftest COMMAND test_failing_selftest)
  endif()
endif()
