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

# FetchContent Expat/zlib/Exiv2 are private build deps. Their install(EXPORT)
# rules still run at generate time. zlib 1.3.x has no EXPORT, and adding
# zlibstatic to exiv2Targets then fails because its INTERFACE_INCLUDE_DIRECTORIES
# point at the source/build trees. libumm links exiv2lib privately and does
# not install those targets.
set(_umm_saved_skip_install_rules "${CMAKE_SKIP_INSTALL_RULES}")
set(CMAKE_SKIP_INSTALL_RULES ON)

# CMP0077: option() in Exiv2 honors these normal variables instead of clobbering them.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(_umm_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)

# Session 05: BMFF on (CR3/HEIC/AVIF read). Samples/tests/docs off. Extra
# third-party deps off except Expat (XMP) and zlib (PNG metadata, session 18).
# Brotli/inih/curl stay off so the same source build remains otherwise
# dep-free on Linux, Windows, and macOS. Exiv2 PNG support uses zlib only,
# not libpng.
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
set(EXIV2_ENABLE_XMP ON)
set(EXIV2_ENABLE_EXTERNAL_XMP OFF)
set(EXIV2_ENABLE_PNG ON)
set(BUILD_WITH_CCACHE OFF)

# Exiv2 find_package(EXPAT REQUIRED) when XMP is on. Prefer a system Expat
# (linux-packages.txt already lists libexpat1-dev); FetchContent only when
# CMake cannot find one (typical on Windows CI).
find_package(EXPAT QUIET)
if(NOT EXPAT_FOUND)
  set(EXPAT_SHARED_LIBS OFF)
  set(EXPAT_BUILD_TOOLS OFF)
  set(EXPAT_BUILD_EXAMPLES OFF)
  set(EXPAT_BUILD_TESTS OFF)
  set(EXPAT_BUILD_DOCS OFF)
  FetchContent_Declare(umm_expat
    URL "https://github.com/libexpat/libexpat/releases/download/R_2_6_4/expat-2.6.4.tar.gz"
    URL_HASH SHA256=fd03b7172b3bd7427a3e7a812063f74754f24542429b634e0db6511b53fb2278
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/expat"
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/.cache/expat/src"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(umm_expat)
  if(NOT TARGET EXPAT::EXPAT)
    add_library(EXPAT::EXPAT ALIAS expat)
  endif()
  # Exiv2 find_package(EXPAT REQUIRED) in module mode. xmpsdk uses
  # EXPAT_INCLUDE_DIRS (plural) for ExpatAdapter.cpp; exiv2lib also uses
  # EXPAT_INCLUDE_DIR and EXPAT::EXPAT. Match CMake's FindEXPAT variables.
  file(TO_CMAKE_PATH "${umm_expat_SOURCE_DIR}/lib" _umm_expat_include_dir)
  file(WRITE "${CMAKE_BINARY_DIR}/expat-config-shim/FindEXPAT.cmake"
    "if(NOT TARGET EXPAT::EXPAT)\n"
    "  add_library(EXPAT::EXPAT ALIAS expat)\n"
    "endif()\n"
    "set(EXPAT_FOUND TRUE)\n"
    "set(EXPAT_INCLUDE_DIR \"${_umm_expat_include_dir}\")\n"
    "set(EXPAT_INCLUDE_DIRS \"${_umm_expat_include_dir}\")\n"
    "set(EXPAT_LIBRARY expat)\n"
    "set(EXPAT_LIBRARIES expat)\n")
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/expat-config-shim")
  set(EXPAT_INCLUDE_DIR "${_umm_expat_include_dir}")
  set(EXPAT_INCLUDE_DIRS "${_umm_expat_include_dir}")
  set(EXPAT_FOUND TRUE)
  unset(_umm_expat_include_dir)
  message(STATUS "Expat not found on system; fetched for Exiv2 XMP")
endif()

# Exiv2 find_package(ZLIB REQUIRED) when PNG is on. Prefer a system zlib
# (linux-packages.txt lists zlib1g-dev; macOS has it). FetchContent only when
# CMake cannot find one (typical on Windows CI).
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
  set(ZLIB_BUILD_EXAMPLES OFF)
  FetchContent_Declare(umm_zlib
    URL "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/zlib"
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/.cache/zlib/src"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(umm_zlib)
  if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
  endif()
  file(TO_CMAKE_PATH "${umm_zlib_SOURCE_DIR}" _umm_zlib_include_dir)
  file(TO_CMAKE_PATH "${umm_zlib_BINARY_DIR}" _umm_zlib_binary_dir)
  file(WRITE "${CMAKE_BINARY_DIR}/zlib-config-shim/FindZLIB.cmake"
    "if(NOT TARGET ZLIB::ZLIB)\n"
    "  add_library(ZLIB::ZLIB ALIAS zlibstatic)\n"
    "endif()\n"
    "set(ZLIB_FOUND TRUE)\n"
    "set(ZLIB_INCLUDE_DIR \"${_umm_zlib_include_dir}\")\n"
    "set(ZLIB_INCLUDE_DIRS \"${_umm_zlib_include_dir};${_umm_zlib_binary_dir}\")\n"
    "set(ZLIB_LIBRARY zlibstatic)\n"
    "set(ZLIB_LIBRARIES zlibstatic)\n")
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/zlib-config-shim")
  set(ZLIB_INCLUDE_DIR "${_umm_zlib_include_dir}")
  set(ZLIB_INCLUDE_DIRS "${_umm_zlib_include_dir};${_umm_zlib_binary_dir}")
  set(ZLIB_FOUND TRUE)
  unset(_umm_zlib_include_dir)
  unset(_umm_zlib_binary_dir)
  message(STATUS "Zlib not found on system; fetched for Exiv2 PNG")
endif()

FetchContent_Declare(umm_exiv2
  URL "${_umm_exiv2_url}"
  URL_HASH "SHA256=${UMM_EXIV2_SHA256}"
  DOWNLOAD_DIR "${_umm_exiv2_download_dir}"
  SOURCE_DIR "${_umm_exiv2_source_dir}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(umm_exiv2)

set(CMAKE_SKIP_INSTALL_RULES "${_umm_saved_skip_install_rules}")
set(BUILD_SHARED_LIBS "${_umm_saved_build_shared_libs}")

if(NOT TARGET exiv2lib)
  message(FATAL_ERROR "UMM_REQUIRE_EXIV2=ON but target exiv2lib was not created")
endif()

# Shim lives in src/; Exiv2 writes exv_conf.h / exiv2lib_export.h to CMAKE_BINARY_DIR.
target_sources(umm PRIVATE
  "${PROJECT_SOURCE_DIR}/src/exiv2_shim.cpp"
  "${PROJECT_SOURCE_DIR}/src/backends/exiv2/exiv2_backend.cpp"
)
target_include_directories(umm
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/backends"
    "${CMAKE_BINARY_DIR}"
)
target_compile_definitions(umm PRIVATE UMM_HAS_EXIV2=1)
target_link_libraries(umm PRIVATE exiv2lib)

set(UMM_EXIV2_ACQUIRED TRUE)

file(APPEND "${CMAKE_BINARY_DIR}/backends-acquired.txt"
  "exiv2.version=${UMM_EXIV2_VERSION}\n"
  "exiv2.bmff=ON\n"
  "exiv2.xmp=ON\n"
  "exiv2.png=ON\n")
message(STATUS "Exiv2 ${UMM_EXIV2_VERSION} (BMFF=ON, XMP=ON, PNG=ON) linked privately into umm")

unset(_umm_exiv2_url)
unset(_umm_exiv2_download_dir)
unset(_umm_exiv2_source_dir)
unset(_umm_saved_build_shared_libs)
unset(_umm_saved_skip_install_rules)
