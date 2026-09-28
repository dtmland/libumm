cmake_minimum_required(VERSION 3.24)

project(libumm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static (decision M4c: static default)" OFF)
option(UMM_BUILD_TESTS "Build unit tests" ON)
option(UMM_ENABLE_FAILING_SELFTEST "Build and register the deliberately failing self-test" OFF)
option(UMM_REQUIRE_EXIV2 "Require Exiv2 at configure time" OFF)
option(UMM_REQUIRE_EXIFTOOL "Require ExifTool at configure time" OFF)

if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
  find_program(UMM_CCACHE ccache)
  if(UMM_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER "${UMM_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${UMM_CCACHE}")
    message(STATUS "Using ccache: ${UMM_CCACHE}")
  endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummPins.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummExifTool.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummRegistry.cmake")

add_library(umm
  src/version.cpp
  src/value.cpp
  src/metadata.cpp
  src/backends/backend_manager.cpp
  "${UMM_REGISTRY_GENERATED_CPP}"
)
add_library(umm::umm ALIAS umm)

target_include_directories(umm
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE
    "${UMM_REGISTRY_GENERATED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummExiv2.cmake")

if(UMM_BUILD_TESTS)
  enable_testing()

  set(UMM_FIXTURES_DIR "${PROJECT_SOURCE_DIR}/tests/fixtures")
  file(TO_CMAKE_PATH "${UMM_FIXTURES_DIR}" UMM_FIXTURES_DIR_CMAKE)

  function(umm_apply_fixture_dir target)
    target_compile_definitions(${target}
      PRIVATE
        "UMM_FIXTURES_DIR=R\"(${UMM_FIXTURES_DIR_CMAKE})\""
    )
  endfunction()

  function(umm_apply_fixture_dir_to_test test_name)
    set_property(TEST ${test_name} PROPERTY ENVIRONMENT_MODIFICATION
      "UMM_FIXTURES_DIR=set:${UMM_FIXTURES_DIR}")
  endfunction()

  add_executable(test_version tests/unit/test_version.cpp)
  target_link_libraries(test_version PRIVATE umm::umm)
  target_compile_definitions(test_version
    PRIVATE
      "UMM_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  )
  add_test(NAME test_version COMMAND test_version)

  add_executable(test_registry_lookup tests/unit/test_registry_lookup.cpp)
  target_link_libraries(test_registry_lookup PRIVATE umm::umm)
  add_test(NAME test_registry_lookup COMMAND test_registry_lookup)

  add_executable(test_result tests/unit/test_result.cpp)
  target_link_libraries(test_result PRIVATE umm::umm)
  add_test(NAME test_result COMMAND test_result)

  add_executable(test_value tests/unit/test_value.cpp)
  target_link_libraries(test_value PRIVATE umm::umm)
  add_test(NAME test_value COMMAND test_value)

  add_executable(test_metadata tests/unit/test_metadata.cpp)
  target_link_libraries(test_metadata PRIVATE umm::umm)
  add_test(NAME test_metadata COMMAND test_metadata)

  add_executable(test_fixtures_path tests/unit/test_fixtures_path.cpp)
  umm_apply_fixture_dir(test_fixtures_path)
  add_test(NAME test_fixtures_path COMMAND test_fixtures_path)
  umm_apply_fixture_dir_to_test(test_fixtures_path)

  if(UMM_ENABLE_FAILING_SELFTEST)
    add_executable(test_failing_selftest tests/unit/test_failing_selftest.cpp)
    add_test(NAME test_failing_selftest COMMAND test_failing_selftest)
  endif()

  if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
    add_test(
      NAME exiftool_smoke
      COMMAND "${CMAKE_COMMAND}"
        "-DUMM_TEST_EXIFTOOL_SCRIPT=${UMM_EXIFTOOL_SCRIPT}"
        "-DUMM_TEST_EXIFTOOL_PERL=${UMM_PERL_EXECUTABLE}"
        "-DUMM_TEST_EXIFTOOL_VERSION=${UMM_EXIFTOOL_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend/test_exiftool_smoke.cmake"
    )

    if(Python3_Interpreter_FOUND)
      add_test(
        NAME fixtures_exiftool
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
                -s "${PROJECT_SOURCE_DIR}/tests/build"
                -p test_fixtures.py
                -v
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      )
      set_property(TEST fixtures_exiftool PROPERTY ENVIRONMENT_MODIFICATION
        "UMM_EXIFTOOL_SCRIPT=set:${UMM_EXIFTOOL_SCRIPT}"
        "UMM_PERL_EXECUTABLE=set:${UMM_PERL_EXECUTABLE}"
        "UMM_FIXTURES_DIR=set:${UMM_FIXTURES_DIR}")
    endif()
  elseif(UMM_REQUIRE_EXIFTOOL)
    message(FATAL_ERROR "UMM_REQUIRE_EXIFTOOL=ON but exiftool_smoke cannot be registered")
  endif()

  if(UMM_EXIV2_ACQUIRED)
    add_executable(test_exiv2_smoke tests/backend/test_exiv2_smoke.cpp)
    target_link_libraries(test_exiv2_smoke PRIVATE umm::umm)
    target_include_directories(test_exiv2_smoke
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_definitions(test_exiv2_smoke
      PRIVATE
        "UMM_TEST_EXIV2_VERSION=\"${UMM_EXIV2_VERSION}\""
    )
    add_test(NAME exiv2_smoke COMMAND test_exiv2_smoke)

    add_executable(test_exiv2_read tests/backend/test_exiv2_read.cpp)
    target_link_libraries(test_exiv2_read PRIVATE umm::umm)
    umm_apply_fixture_dir(test_exiv2_read)
    target_compile_definitions(test_exiv2_read
      PRIVATE
        "UMM_TEST_EXIV2_VERSION=\"${UMM_EXIV2_VERSION}\""
    )
    add_test(NAME test_exiv2_read COMMAND test_exiv2_read)
    umm_apply_fixture_dir_to_test(test_exiv2_read)
  elseif(UMM_REQUIRE_EXIV2)
    message(FATAL_ERROR "UMM_REQUIRE_EXIV2=ON but exiv2_smoke cannot be registered")
  endif()
endif()
