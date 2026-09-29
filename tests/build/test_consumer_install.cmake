# CTest driver (session 29): cmake --install into a scratch prefix, then
# configure/build/run tests/consumer via find_package(umm CONFIG).

if(NOT UMM_BUILD_DIR)
  message(FATAL_ERROR "UMM_BUILD_DIR is not set")
endif()
if(NOT UMM_INSTALL_PREFIX)
  message(FATAL_ERROR "UMM_INSTALL_PREFIX is not set")
endif()
if(NOT UMM_CONSUMER_SOURCE_DIR)
  message(FATAL_ERROR "UMM_CONSUMER_SOURCE_DIR is not set")
endif()
if(NOT UMM_CONSUMER_BINARY_DIR)
  message(FATAL_ERROR "UMM_CONSUMER_BINARY_DIR is not set")
endif()
if(NOT UMM_CONSUMER_FIXTURE)
  message(FATAL_ERROR "UMM_CONSUMER_FIXTURE is not set")
endif()

function(umm_run)
  set(options)
  set(oneValueArgs RESULT_VARIABLE OUTPUT_VARIABLE ERROR_VARIABLE)
  set(multiValueArgs COMMAND)
  cmake_parse_arguments(_umm "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  execute_process(
    COMMAND ${_umm_COMMAND}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  set(${_umm_RESULT_VARIABLE} "${_rc}" PARENT_SCOPE)
  set(${_umm_OUTPUT_VARIABLE} "${_out}" PARENT_SCOPE)
  set(${_umm_ERROR_VARIABLE} "${_err}" PARENT_SCOPE)
endfunction()

set(_umm_install_cmd "${CMAKE_COMMAND}" --install "${UMM_BUILD_DIR}" --prefix "${UMM_INSTALL_PREFIX}")
if(UMM_BUILD_TYPE)
  list(APPEND _umm_install_cmd --config "${UMM_BUILD_TYPE}")
endif()

umm_run(COMMAND ${_umm_install_cmd} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "cmake --install failed (${_rc}): ${_err}\n${_out}")
endif()

foreach(_umm_forbidden IN ITEMS
    "${UMM_INSTALL_PREFIX}/include/exiv2"
    "${UMM_INSTALL_PREFIX}/include/expat.h"
    "${UMM_INSTALL_PREFIX}/include/zlib.h"
    "${UMM_INSTALL_PREFIX}/include/zconf.h"
  )
  if(EXISTS "${_umm_forbidden}")
    message(FATAL_ERROR "install prefix leaked dependency file: ${_umm_forbidden}")
  endif()
endforeach()

file(GLOB_RECURSE _umm_leaked_cmake
  "${UMM_INSTALL_PREFIX}/*exiv2Config.cmake"
  "${UMM_INSTALL_PREFIX}/*exiv2Targets.cmake"
  "${UMM_INSTALL_PREFIX}/*expat-config.cmake"
  "${UMM_INSTALL_PREFIX}/*EXPATConfig.cmake"
  "${UMM_INSTALL_PREFIX}/*zlib-config.cmake"
  "${UMM_INSTALL_PREFIX}/*ZLIBConfig.cmake"
)
if(_umm_leaked_cmake)
  message(FATAL_ERROR "install prefix leaked dependency CMake files: ${_umm_leaked_cmake}")
endif()

file(GLOB_RECURSE _umm_notices "${UMM_INSTALL_PREFIX}/*/THIRD-PARTY-NOTICES.md")
if(NOT _umm_notices)
  message(FATAL_ERROR "THIRD-PARTY-NOTICES.md not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_gpl3 "${UMM_INSTALL_PREFIX}/*/GPL-3.0.txt")
if(NOT _umm_gpl3)
  message(FATAL_ERROR "licenses/GPL-3.0.txt not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_gpl2 "${UMM_INSTALL_PREFIX}/*/GPL-2.0.txt")
if(NOT _umm_gpl2)
  message(FATAL_ERROR "licenses/GPL-2.0.txt not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_expat_lic "${UMM_INSTALL_PREFIX}/*/Expat.txt")
if(NOT _umm_expat_lic)
  message(FATAL_ERROR "licenses/Expat.txt not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_zlib_lic "${UMM_INSTALL_PREFIX}/*/Zlib.txt")
if(NOT _umm_zlib_lic)
  message(FATAL_ERROR "licenses/Zlib.txt not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_csrc "${UMM_INSTALL_PREFIX}/*/corresponding-source.json")
if(NOT _umm_csrc)
  message(FATAL_ERROR "corresponding-source.json not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_notice "${UMM_INSTALL_PREFIX}/*/NOTICE.md")
if(NOT _umm_notice)
  message(FATAL_ERROR "NOTICE.md not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_license "${UMM_INSTALL_PREFIX}/*/LICENSE")
if(NOT _umm_license)
  message(FATAL_ERROR "LICENSE not found under ${UMM_INSTALL_PREFIX}")
endif()

if(NOT EXISTS "${UMM_INSTALL_PREFIX}/include/umm/umm.hpp")
  message(FATAL_ERROR "installed headers missing: include/umm/umm.hpp")
endif()
if(NOT EXISTS "${UMM_INSTALL_PREFIX}/include/umm/version.hpp")
  message(FATAL_ERROR "installed headers missing: include/umm/version.hpp")
endif()

file(GLOB_RECURSE _umm_config "${UMM_INSTALL_PREFIX}/*/ummConfig.cmake")
if(NOT _umm_config)
  message(FATAL_ERROR "ummConfig.cmake not found under ${UMM_INSTALL_PREFIX}")
endif()
file(GLOB_RECURSE _umm_version "${UMM_INSTALL_PREFIX}/*/ummConfigVersion.cmake")
if(NOT _umm_version)
  message(FATAL_ERROR "ummConfigVersion.cmake not found under ${UMM_INSTALL_PREFIX}")
endif()

set(_umm_configure_cmd
  "${CMAKE_COMMAND}"
  -S "${UMM_CONSUMER_SOURCE_DIR}"
  -B "${UMM_CONSUMER_BINARY_DIR}"
  "-DCMAKE_PREFIX_PATH=${UMM_INSTALL_PREFIX}"
)
if(UMM_GENERATOR)
  list(APPEND _umm_configure_cmd -G "${UMM_GENERATOR}")
endif()
if(UMM_BUILD_TYPE)
  list(APPEND _umm_configure_cmd "-DCMAKE_BUILD_TYPE=${UMM_BUILD_TYPE}")
endif()
if(UMM_CXX_COMPILER)
  list(APPEND _umm_configure_cmd "-DCMAKE_CXX_COMPILER=${UMM_CXX_COMPILER}")
endif()
if(UMM_MAKE_PROGRAM)
  list(APPEND _umm_configure_cmd "-DCMAKE_MAKE_PROGRAM=${UMM_MAKE_PROGRAM}")
endif()

umm_run(COMMAND ${_umm_configure_cmd} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "consumer configure failed (${_rc}): ${_err}\n${_out}")
endif()

set(_umm_build_cmd "${CMAKE_COMMAND}" --build "${UMM_CONSUMER_BINARY_DIR}")
if(UMM_BUILD_TYPE)
  list(APPEND _umm_build_cmd --config "${UMM_BUILD_TYPE}")
endif()

umm_run(COMMAND ${_umm_build_cmd} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "consumer build failed (${_rc}): ${_err}\n${_out}")
endif()

set(_umm_consumer "${UMM_CONSUMER_BINARY_DIR}/umm_consumer")
if(EXISTS "${UMM_CONSUMER_BINARY_DIR}/umm_consumer.exe")
  set(_umm_consumer "${UMM_CONSUMER_BINARY_DIR}/umm_consumer.exe")
endif()
if(UMM_BUILD_TYPE AND EXISTS "${UMM_CONSUMER_BINARY_DIR}/${UMM_BUILD_TYPE}/umm_consumer.exe")
  set(_umm_consumer "${UMM_CONSUMER_BINARY_DIR}/${UMM_BUILD_TYPE}/umm_consumer.exe")
endif()
if(UMM_BUILD_TYPE AND EXISTS "${UMM_CONSUMER_BINARY_DIR}/${UMM_BUILD_TYPE}/umm_consumer")
  set(_umm_consumer "${UMM_CONSUMER_BINARY_DIR}/${UMM_BUILD_TYPE}/umm_consumer")
endif()
if(NOT EXISTS "${_umm_consumer}")
  message(FATAL_ERROR "consumer executable not found under ${UMM_CONSUMER_BINARY_DIR}")
endif()

if(NOT EXISTS "${UMM_CONSUMER_FIXTURE}")
  message(FATAL_ERROR "consumer fixture missing: ${UMM_CONSUMER_FIXTURE}")
endif()

umm_run(
  COMMAND "${_umm_consumer}" "${UMM_CONSUMER_FIXTURE}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "consumer run failed (${_rc}): ${_err}\n${_out}")
endif()
if(NOT _out)
  message(FATAL_ERROR "consumer printed empty umm::version()")
endif()
