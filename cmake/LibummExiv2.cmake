# Checksum-pinned Exiv2 acquisition (session 05). Version/SHA come from backends.env.
# Decision M4a: pinned source via FetchContent on all OSes. Decision M4c: static Exiv2.

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/LibummPins.cmake")

set(UMM_EXIV2_ACQUIRED FALSE)

if(NOT UMM_REQUIRE_EXIV2)
  message(STATUS "Exiv2 not acquired (UMM_REQUIRE_EXIV2=OFF)")
  return()
endif()

if(NOT TARGET umm)
  message(FATAL_ERROR "LibummExiv2.cmake must be included after the umm target is created")
endif()

include(FetchContent)

set(_umm_exiv2_url
  "https://github.com/Exiv2/exiv2/archive/refs/tags/v${UMM_EXIV2_VERSION}.tar.gz")
set(_umm_exiv2_download_dir "${PROJECT_SOURCE_DIR}/.cache/exiv2")
set(_umm_exiv2_source_dir "${PROJECT_SOURCE_DIR}/.cache/exiv2/src")

# CMP0077: option() in Exiv2 honors these normal variables instead of clobbering them.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(_umm_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)

# Session 05: BMFF on (CR3/HEIC/AVIF read). Samples/tests/docs off. Extra third-party
# deps off so the same source build is dep-free on Linux, Windows, and macOS.
set(EXIV2_ENABLE_BMFF ON)
set(EXIV2_BUILD_SAMPLES OFF)
set(EXIV2_BUILD_EXIV2_COMMAND OFF)
set(EXIV2_BUILD_UNIT_TESTS OFF)
set(EXIV2_BUILD_FUZZ_TESTS OFF)
set(EXIV2_BUILD_DOC OFF)
set(EXIV2_ENABLE_BROTLI OFF)
set(EXIV2_ENABLE_INIH OFF)
set(EXIV2_ENABLE_WEBREADY OFF)
set(EXIV2_ENABLE_CURL OFF)
set(EXIV2_ENABLE_NLS OFF)
set(EXIV2_ENABLE_VIDEO OFF)
set(EXIV2_ENABLE_XMP OFF)
set(EXIV2_ENABLE_EXTERNAL_XMP OFF)
set(EXIV2_ENABLE_PNG OFF)
set(BUILD_WITH_CCACHE OFF)

FetchContent_Declare(umm_exiv2
  URL "${_umm_exiv2_url}"
  URL_HASH "SHA256=${UMM_EXIV2_SHA256}"
  DOWNLOAD_DIR "${_umm_exiv2_download_dir}"
  SOURCE_DIR "${_umm_exiv2_source_dir}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(umm_exiv2)

set(BUILD_SHARED_LIBS "${_umm_saved_build_shared_libs}")

if(NOT TARGET exiv2lib)
  message(FATAL_ERROR "UMM_REQUIRE_EXIV2=ON but target exiv2lib was not created")
endif()

# Shim lives in src/; Exiv2 writes exv_conf.h / exiv2lib_export.h to CMAKE_BINARY_DIR.
target_sources(umm PRIVATE "${PROJECT_SOURCE_DIR}/src/exiv2_shim.cpp")
target_include_directories(umm
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}"
)
target_link_libraries(umm PRIVATE exiv2lib)

set(UMM_EXIV2_ACQUIRED TRUE)

file(APPEND "${CMAKE_BINARY_DIR}/backends-acquired.txt"
  "exiv2.version=${UMM_EXIV2_VERSION}\n"
  "exiv2.bmff=ON\n")
message(STATUS "Exiv2 ${UMM_EXIV2_VERSION} (BMFF=ON) linked privately into umm")

unset(_umm_exiv2_url)
unset(_umm_exiv2_download_dir)
unset(_umm_exiv2_source_dir)
unset(_umm_saved_build_shared_libs)
