# Checksum-pinned ExifTool acquisition (session 04). Version/SHA come from backends.env.

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/LibummPins.cmake")

set(UMM_EXIFTOOL_SCRIPT "")
set(UMM_PERL_EXECUTABLE "")

if(UMM_REQUIRE_EXIFTOOL)
  find_package(Perl REQUIRED)
else()
  find_package(Perl)
endif()

if(PERL_FOUND)
  set(UMM_PERL_EXECUTABLE "${PERL_EXECUTABLE}")
endif()

set(_umm_want_exiftool FALSE)
if(UMM_REQUIRE_EXIFTOOL OR PERL_FOUND)
  set(_umm_want_exiftool TRUE)
endif()

if(_umm_want_exiftool)
  include(FetchContent)

  set(_umm_exiftool_url
    "https://github.com/exiftool/exiftool/archive/refs/tags/${UMM_EXIFTOOL_VERSION}.tar.gz")
  set(_umm_exiftool_download_dir "${PROJECT_SOURCE_DIR}/.cache/exiftool")
  set(_umm_exiftool_source_dir "${PROJECT_SOURCE_DIR}/.cache/exiftool/src")

  FetchContent_Declare(umm_exiftool
    URL "${_umm_exiftool_url}"
    URL_HASH "SHA256=${UMM_EXIFTOOL_SHA256}"
    DOWNLOAD_DIR "${_umm_exiftool_download_dir}"
    SOURCE_DIR "${_umm_exiftool_source_dir}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  # ExifTool is a Perl tree, not a CMake project. MakeAvailable skips
  # add_subdirectory when the populated tree has no CMakeLists.txt.
  FetchContent_MakeAvailable(umm_exiftool)

  set(UMM_EXIFTOOL_SCRIPT "${umm_exiftool_SOURCE_DIR}/exiftool")
  if(NOT EXISTS "${UMM_EXIFTOOL_SCRIPT}")
    if(UMM_REQUIRE_EXIFTOOL)
      message(FATAL_ERROR "ExifTool script not found at ${UMM_EXIFTOOL_SCRIPT}")
    endif()
    message(STATUS "ExifTool script not found at ${UMM_EXIFTOOL_SCRIPT}")
    set(UMM_EXIFTOOL_SCRIPT "")
  endif()
endif()

if(UMM_REQUIRE_EXIFTOOL)
  if(UMM_EXIFTOOL_SCRIPT STREQUAL "" OR UMM_PERL_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "UMM_REQUIRE_EXIFTOOL=ON but ExifTool was not acquired")
  endif()
elseif(UMM_EXIFTOOL_SCRIPT STREQUAL "")
  message(STATUS "ExifTool not acquired (optional; Perl not found or archive missing)")
endif()

if(NOT UMM_EXIFTOOL_SCRIPT STREQUAL "" AND NOT UMM_PERL_EXECUTABLE STREQUAL "")
  file(APPEND "${CMAKE_BINARY_DIR}/backends-acquired.txt"
    "exiftool.version=${UMM_EXIFTOOL_VERSION}\n"
    "exiftool.script=${UMM_EXIFTOOL_SCRIPT}\n"
    "exiftool.perl=${UMM_PERL_EXECUTABLE}\n")
  message(STATUS
    "ExifTool ${UMM_EXIFTOOL_VERSION} via ${UMM_PERL_EXECUTABLE} ${UMM_EXIFTOOL_SCRIPT}")
endif()

unset(_umm_want_exiftool)
unset(_umm_exiftool_url)
unset(_umm_exiftool_download_dir)
unset(_umm_exiftool_source_dir)
