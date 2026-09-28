# Parse tools/build/backends.env. Pins must not be duplicated in CMake code.

include_guard(GLOBAL)

set(_umm_backends_env "${CMAKE_CURRENT_LIST_DIR}/../tools/build/backends.env")
get_filename_component(_umm_backends_env "${_umm_backends_env}" ABSOLUTE)
if(NOT EXISTS "${_umm_backends_env}")
  message(FATAL_ERROR "libumm pins file not found: ${_umm_backends_env}")
endif()

file(STRINGS "${_umm_backends_env}" _umm_pin_lines)
foreach(_umm_line IN LISTS _umm_pin_lines)
  if(_umm_line MATCHES "^[ \t]*#" OR _umm_line MATCHES "^[ \t]*$")
    continue()
  endif()
  if(NOT _umm_line MATCHES "^([A-Z_][A-Z0-9_]*)=(.*)$")
    message(FATAL_ERROR "libumm pins: malformed line: ${_umm_line}")
  endif()
  set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
endforeach()

foreach(_umm_required IN ITEMS
    UMM_EXIV2_VERSION
    UMM_EXIV2_SHA256
    UMM_EXIFTOOL_VERSION
    UMM_EXIFTOOL_SHA256
    UMM_STRAWBERRY_PERL_VERSION)
  if("${${_umm_required}}" STREQUAL "")
    message(FATAL_ERROR "libumm pins: missing ${_umm_required} in ${_umm_backends_env}")
  endif()
endforeach()

foreach(_umm_sha IN ITEMS UMM_EXIV2_SHA256 UMM_EXIFTOOL_SHA256)
  if(NOT "${${_umm_sha}}" MATCHES "^[0-9a-fA-F][0-9a-fA-F]*$")
    message(FATAL_ERROR "libumm pins: ${_umm_sha} is not hexadecimal")
  endif()
  string(LENGTH "${${_umm_sha}}" _umm_sha_len)
  if(NOT _umm_sha_len EQUAL 64)
    message(FATAL_ERROR "libumm pins: ${_umm_sha} must be 64 hex characters")
  endif()
endforeach()

file(WRITE "${CMAKE_BINARY_DIR}/backends-acquired.txt" "")

unset(_umm_backends_env)
unset(_umm_pin_lines)
unset(_umm_line)
unset(_umm_required)
unset(_umm_sha)
unset(_umm_sha_len)
