cmake_minimum_required(VERSION 3.24)

project(libumm VERSION 0.1.1 LANGUAGES CXX)

# Session 30: header macros are normative; CMake PROJECT_VERSION must match.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/umm/version.hpp" _umm_version_hpp)
if(NOT _umm_version_hpp MATCHES "#define[ \t]+UMM_VERSION_MAJOR[ \t]+([0-9]+)")
  message(FATAL_ERROR "include/umm/version.hpp must define UMM_VERSION_MAJOR")
endif()
set(_umm_hdr_major "${CMAKE_MATCH_1}")
if(NOT _umm_version_hpp MATCHES "#define[ \t]+UMM_VERSION_MINOR[ \t]+([0-9]+)")
  message(FATAL_ERROR "include/umm/version.hpp must define UMM_VERSION_MINOR")
endif()
set(_umm_hdr_minor "${CMAKE_MATCH_1}")
if(NOT _umm_version_hpp MATCHES "#define[ \t]+UMM_VERSION_PATCH[ \t]+([0-9]+)")
  message(FATAL_ERROR "include/umm/version.hpp must define UMM_VERSION_PATCH")
endif()
set(_umm_hdr_patch "${CMAKE_MATCH_1}")
set(_umm_hdr_version "${_umm_hdr_major}.${_umm_hdr_minor}.${_umm_hdr_patch}")
if(NOT _umm_hdr_version STREQUAL PROJECT_VERSION)
  message(FATAL_ERROR
    "include/umm/version.hpp version ${_umm_hdr_version} does not match PROJECT_VERSION ${PROJECT_VERSION}")
endif()
unset(_umm_version_hpp)
unset(_umm_hdr_major)
unset(_umm_hdr_minor)
unset(_umm_hdr_patch)
unset(_umm_hdr_version)

# SOVERSION and find_package compatibility follow docs/abi-policy.md (P4).
if(PROJECT_VERSION_MAJOR EQUAL 0)
  set(UMM_SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
  set(UMM_PACKAGE_COMPATIBILITY SameMinorVersion)
else()
  set(UMM_SOVERSION "${PROJECT_VERSION_MAJOR}")
  set(UMM_PACKAGE_COMPATIBILITY SameMajorVersion)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static (decision M4c: static default)" OFF)
option(UMM_BUILD_TESTS "Build unit tests" ON)
option(UMM_ENABLE_FAILING_SELFTEST "Build and register the deliberately failing self-test" OFF)
option(UMM_REQUIRE_EXIV2 "Require Exiv2 at configure time" OFF)
option(UMM_REQUIRE_EXIFTOOL "Require ExifTool at configure time" OFF)
option(UMM_TIER_B "Fetch and test the checksummed Tier B corpus" OFF)

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
  src/read.cpp
  src/describe.cpp
  src/cast.cpp
  src/conflict.cpp
  src/sync.cpp
  src/track.cpp
  src/write.cpp
  src/capabilities.cpp
  src/core/cast.cpp
  src/core/reconcile.cpp
  src/core/xmp_codec.cpp
  src/core/transpose.cpp
  src/core/write_sync.cpp
  src/core/atomic_write.cpp
  src/core/sidecar.cpp
  src/backends/backend_manager.cpp
  src/backends/exiftool/json.cpp
  src/backends/exiftool/keys.cpp
  src/backends/exiftool/process.cpp
  src/backends/exiftool/exiftool_backend.cpp
  "${UMM_REGISTRY_GENERATED_CPP}"
)
add_library(umm::umm ALIAS umm)

target_include_directories(umm
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PRIVATE
    "${UMM_REGISTRY_GENERATED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
)

set_target_properties(umm PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  VERSION ${PROJECT_VERSION}
  SOVERSION ${UMM_SOVERSION}
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummExiv2.cmake")

if(UMM_BUILD_TESTS)
  enable_testing()
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummCorpus.cmake")

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
      "UMM_CMAKE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
      "UMM_CMAKE_VERSION_MINOR=${PROJECT_VERSION_MINOR}"
      "UMM_CMAKE_VERSION_PATCH=${PROJECT_VERSION_PATCH}"
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

  add_executable(test_transpose tests/unit/test_transpose.cpp)
  target_link_libraries(test_transpose PRIVATE umm::umm)
  target_include_directories(test_transpose
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  add_test(NAME test_transpose COMMAND test_transpose)

  add_executable(test_reconcile tests/unit/test_reconcile.cpp)
  target_link_libraries(test_reconcile PRIVATE umm::umm)
  target_include_directories(test_reconcile
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  add_test(NAME test_reconcile COMMAND test_reconcile)

  add_executable(test_conflict tests/unit/test_conflict.cpp)
  target_link_libraries(test_conflict PRIVATE umm::umm)
  target_include_directories(test_conflict
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  add_test(NAME test_conflict COMMAND test_conflict)

  add_executable(test_atomic_write tests/unit/test_atomic_write.cpp)
  target_link_libraries(test_atomic_write PRIVATE umm::umm)
  target_include_directories(test_atomic_write
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  add_test(NAME test_atomic_write COMMAND test_atomic_write)

  add_executable(test_write_sync tests/unit/test_write_sync.cpp)
  target_link_libraries(test_write_sync PRIVATE umm::umm)
  target_include_directories(test_write_sync
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  add_test(NAME test_write_sync COMMAND test_write_sync)

  add_executable(test_cast tests/unit/test_cast.cpp)
  target_link_libraries(test_cast PRIVATE umm::umm)
  target_include_directories(test_cast
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
      "${UMM_REGISTRY_GENERATED_DIR}"
  )
  add_test(NAME test_cast COMMAND test_cast)

  add_executable(test_describe tests/unit/test_describe.cpp)
  target_link_libraries(test_describe PRIVATE umm::umm)
  target_include_directories(test_describe
    PRIVATE
      "${UMM_REGISTRY_GENERATED_DIR}"
  )
  add_test(NAME test_describe COMMAND test_describe)

  add_executable(test_gps_codec tests/unit/test_gps_codec.cpp)
  target_link_libraries(test_gps_codec PRIVATE umm::umm)
  target_include_directories(test_gps_codec
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  add_test(NAME test_gps_codec COMMAND test_gps_codec)

  add_executable(test_storage_policy tests/unit/test_storage_policy.cpp)
  target_link_libraries(test_storage_policy PRIVATE umm::umm)
  add_test(NAME test_storage_policy COMMAND test_storage_policy)

  add_executable(test_capabilities tests/unit/test_capabilities.cpp)
  target_link_libraries(test_capabilities PRIVATE umm::umm)
  add_test(NAME test_capabilities COMMAND test_capabilities)

  add_executable(test_fixtures_path tests/unit/test_fixtures_path.cpp)
  umm_apply_fixture_dir(test_fixtures_path)
  add_test(NAME test_fixtures_path COMMAND test_fixtures_path)
  umm_apply_fixture_dir_to_test(test_fixtures_path)

  add_executable(test_track tests/unit/test_track.cpp)
  target_link_libraries(test_track PRIVATE umm::umm)
  umm_apply_fixture_dir(test_track)
  add_test(NAME test_track COMMAND test_track)
  umm_apply_fixture_dir_to_test(test_track)

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
        "UMM_TEST_EXIV2_MIN_VERSION=\"${UMM_EXIV2_MIN_VERSION}\""
    )
    if(UMM_EXIV2_SYSTEM)
      target_compile_definitions(test_exiv2_smoke PRIVATE UMM_TEST_EXIV2_SYSTEM=1)
    endif()
    add_test(NAME exiv2_smoke COMMAND test_exiv2_smoke)

    add_executable(test_exiv2_read tests/backend/test_exiv2_read.cpp)
    target_link_libraries(test_exiv2_read PRIVATE umm::umm)
    target_include_directories(test_exiv2_read
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
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

  add_executable(test_exiftool_keys tests/backend/test_exiftool_keys.cpp)
  target_link_libraries(test_exiftool_keys PRIVATE umm::umm)
  target_include_directories(test_exiftool_keys
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
  )
  add_test(NAME test_exiftool_keys COMMAND test_exiftool_keys)

  add_executable(test_child_process tests/backend/test_child_process.cpp)
  target_link_libraries(test_child_process PRIVATE umm::umm)
  target_include_directories(test_child_process
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
  )
  add_test(NAME test_child_process COMMAND test_child_process)

  add_executable(test_exiftool_unavailable tests/backend/test_exiftool_unavailable.cpp)
  target_link_libraries(test_exiftool_unavailable PRIVATE umm::umm)
  add_test(NAME test_exiftool_unavailable COMMAND test_exiftool_unavailable)

  add_executable(test_exiftool_native_exe tests/backend/test_exiftool_native_exe.cpp)
  target_link_libraries(test_exiftool_native_exe PRIVATE umm::umm)
  target_include_directories(test_exiftool_native_exe
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
  )
  add_test(NAME test_exiftool_native_exe COMMAND test_exiftool_native_exe)

  if(UMM_EXIV2_ACQUIRED OR (UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE))
    add_executable(test_write tests/backend/test_write.cpp)
    target_link_libraries(test_write PRIVATE umm::umm)
    target_include_directories(test_write
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_write)
    umm_apply_corpus_dir(test_write)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_WRITE_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_WRITE_EXIFTOOL_PERL)
      target_compile_definitions(test_write
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_WRITE_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_WRITE_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_write COMMAND test_write)
    umm_apply_fixture_dir_to_test(test_write)
    umm_apply_corpus_dir_to_test(test_write)

    add_executable(test_cross_media tests/backend/test_cross_media.cpp)
    target_link_libraries(test_cross_media PRIVATE umm::umm)
    target_include_directories(test_cross_media
      PRIVATE
        "${UMM_REGISTRY_GENERATED_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_cross_media)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_XMEDIA_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_XMEDIA_EXIFTOOL_PERL)
      target_compile_definitions(test_cross_media
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_XMEDIA_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_XMEDIA_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_cross_media COMMAND test_cross_media)
    umm_apply_fixture_dir_to_test(test_cross_media)

    add_executable(test_read tests/backend/test_read.cpp)
    target_link_libraries(test_read PRIVATE umm::umm)
    target_include_directories(test_read
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_read)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_READ_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_READ_EXIFTOOL_PERL)
      target_compile_definitions(test_read
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_READ_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_READ_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_read COMMAND test_read)
    umm_apply_fixture_dir_to_test(test_read)

    add_executable(test_capabilities_probe tests/backend/test_capabilities_probe.cpp)
    target_link_libraries(test_capabilities_probe PRIVATE umm::umm)
    target_include_directories(test_capabilities_probe
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_capabilities_probe)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_CAP_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_CAP_EXIFTOOL_PERL)
      target_compile_definitions(test_capabilities_probe
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_CAP_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_CAP_EXIFTOOL_PERL})\""
          "UMM_TEST_EXIFTOOL_VERSION=\"${UMM_EXIFTOOL_VERSION}\""
      )
    endif()
    add_test(NAME test_capabilities_probe COMMAND test_capabilities_probe)
    umm_apply_fixture_dir_to_test(test_capabilities_probe)

    add_executable(test_sidecar tests/backend/test_sidecar.cpp)
    target_link_libraries(test_sidecar PRIVATE umm::umm)
    target_include_directories(test_sidecar
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_sidecar)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_SIDECAR_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_SIDECAR_EXIFTOOL_PERL)
      target_compile_definitions(test_sidecar
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_SIDECAR_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_SIDECAR_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_sidecar COMMAND test_sidecar)
    umm_apply_fixture_dir_to_test(test_sidecar)

    add_executable(test_sync tests/backend/test_sync.cpp)
    target_link_libraries(test_sync PRIVATE umm::umm)
    target_include_directories(test_sync
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_sync)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_SYNC_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_SYNC_EXIFTOOL_PERL)
      target_compile_definitions(test_sync
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_SYNC_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_SYNC_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_sync COMMAND test_sync)
    umm_apply_fixture_dir_to_test(test_sync)

    add_executable(test_cast_api tests/backend/test_cast.cpp)
    target_link_libraries(test_cast_api PRIVATE umm::umm)
    target_include_directories(test_cast_api
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_cast_api)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_CAST_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_CAST_EXIFTOOL_PERL)
      target_compile_definitions(test_cast_api
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_CAST_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_CAST_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_cast_api COMMAND test_cast_api)
    umm_apply_fixture_dir_to_test(test_cast_api)

    add_executable(test_describe_api tests/backend/test_describe.cpp)
    target_link_libraries(test_describe_api PRIVATE umm::umm)
    target_include_directories(test_describe_api
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_describe_api)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_DESCRIBE_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_DESCRIBE_EXIFTOOL_PERL)
      target_compile_definitions(test_describe_api
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_DESCRIBE_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_DESCRIBE_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_describe_api COMMAND test_describe_api)
    umm_apply_fixture_dir_to_test(test_describe_api)

    add_executable(test_conflict_api tests/backend/test_conflict.cpp)
    target_link_libraries(test_conflict_api PRIVATE umm::umm)
    target_include_directories(test_conflict_api
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_conflict_api)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_CONFLICT_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_CONFLICT_EXIFTOOL_PERL)
      target_compile_definitions(test_conflict_api
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_CONFLICT_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_CONFLICT_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_conflict_api COMMAND test_conflict_api)
    umm_apply_fixture_dir_to_test(test_conflict_api)

    add_executable(test_track_match tests/backend/test_track_match.cpp)
    target_link_libraries(test_track_match PRIVATE umm::umm)
    target_include_directories(test_track_match
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_track_match)
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_TRACK_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_TRACK_EXIFTOOL_PERL)
      target_compile_definitions(test_track_match
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_TRACK_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_TRACK_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_track_match COMMAND test_track_match)
    umm_apply_fixture_dir_to_test(test_track_match)

    set(UMM_VERIFICATION_LEDGER
        "${PROJECT_SOURCE_DIR}/tests/verification/ledger.json")
    file(TO_CMAKE_PATH "${UMM_VERIFICATION_LEDGER}" UMM_VERIFICATION_LEDGER_CMAKE)
    add_executable(test_cross_backend tests/backend/test_cross_backend.cpp)
    target_link_libraries(test_cross_backend PRIVATE umm::umm)
    target_include_directories(test_cross_backend
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_cross_backend)
    umm_apply_corpus_dir(test_cross_backend)
    target_compile_definitions(test_cross_backend
      PRIVATE
        "UMM_VERIFICATION_LEDGER=R\"(${UMM_VERIFICATION_LEDGER_CMAKE})\""
    )
    if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
      file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_XBV_EXIFTOOL_SCRIPT)
      file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_XBV_EXIFTOOL_PERL)
      target_compile_definitions(test_cross_backend
        PRIVATE
          "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_XBV_EXIFTOOL_SCRIPT})\""
          "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_XBV_EXIFTOOL_PERL})\""
      )
    endif()
    add_test(NAME test_cross_backend COMMAND test_cross_backend)
    umm_apply_fixture_dir_to_test(test_cross_backend)
    umm_apply_corpus_dir_to_test(test_cross_backend)

    if(UMM_CORPUS_READY)
      add_executable(test_tier_b tests/backend/test_tier_b.cpp)
      target_link_libraries(test_tier_b PRIVATE umm::umm)
      target_include_directories(test_tier_b
        PRIVATE
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
      )
      umm_apply_fixture_dir(test_tier_b)
      umm_apply_corpus_dir(test_tier_b)
      if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
        file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_TEST_TIER_B_EXIFTOOL_SCRIPT)
        file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_TEST_TIER_B_EXIFTOOL_PERL)
        target_compile_definitions(test_tier_b
          PRIVATE
            "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_TEST_TIER_B_EXIFTOOL_SCRIPT})\""
            "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_TEST_TIER_B_EXIFTOOL_PERL})\""
        )
      endif()
      add_test(NAME test_tier_b COMMAND test_tier_b)
      umm_apply_fixture_dir_to_test(test_tier_b)
      umm_apply_corpus_dir_to_test(test_tier_b)
      add_test(
        NAME tier_b_fetch
        COMMAND "${Python3_EXECUTABLE}" "${UMM_CORPUS_FETCHER}"
                --manifest "${UMM_CORPUS_MANIFEST}"
                --dest "${UMM_CORPUS_DIR}"
                --offline
                --require
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      )
    endif()
  endif()

  if(UMM_EXIV2_ACQUIRED)
    add_test(
      NAME consumer_install
      COMMAND "${CMAKE_COMMAND}"
        "-DUMM_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DUMM_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/consumer-prefix"
        "-DUMM_CONSUMER_SOURCE_DIR=${PROJECT_SOURCE_DIR}/tests/consumer"
        "-DUMM_CONSUMER_BINARY_DIR=${CMAKE_BINARY_DIR}/consumer-smoke"
        "-DUMM_CONSUMER_FIXTURE=${UMM_FIXTURES_DIR_CMAKE}/jpeg/full-agreeing.jpg"
        "-DUMM_GENERATOR=${CMAKE_GENERATOR}"
        "-DUMM_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DUMM_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DUMM_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DUMM_EXIV2_SHARED=${UMM_EXIV2_SHARED}"
        "-DUMM_EXIV2_SYSTEM=${UMM_EXIV2_SYSTEM}"
        -P "${PROJECT_SOURCE_DIR}/tests/build/test_consumer_install.cmake"
    )
  endif()

  if(NOT UMM_TIER_B)
    add_test(
      NAME tier_b_skipped
      COMMAND "${CMAKE_COMMAND}" -E echo
              "SKIP: Tier B corpus not requested (UMM_TIER_B=OFF)"
    )
  endif()

  if(UMM_EXIFTOOL_SCRIPT AND UMM_PERL_EXECUTABLE)
    file(TO_CMAKE_PATH "${UMM_EXIFTOOL_SCRIPT}" UMM_EXIFTOOL_SCRIPT_CMAKE)
    file(TO_CMAKE_PATH "${UMM_PERL_EXECUTABLE}" UMM_PERL_EXECUTABLE_CMAKE)

    add_executable(test_exiftool_read tests/backend/test_exiftool_read.cpp)
    target_link_libraries(test_exiftool_read PRIVATE umm::umm)
    target_include_directories(test_exiftool_read
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backends"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/backend"
    )
    umm_apply_fixture_dir(test_exiftool_read)
    target_compile_definitions(test_exiftool_read
      PRIVATE
        "UMM_TEST_EXIFTOOL_SCRIPT=R\"(${UMM_EXIFTOOL_SCRIPT_CMAKE})\""
        "UMM_TEST_EXIFTOOL_PERL=R\"(${UMM_PERL_EXECUTABLE_CMAKE})\""
        "UMM_TEST_EXIFTOOL_VERSION=\"${UMM_EXIFTOOL_VERSION}\""
    )
    add_test(NAME test_exiftool_read COMMAND test_exiftool_read)
    umm_apply_fixture_dir_to_test(test_exiftool_read)
  endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibummInstall.cmake")
