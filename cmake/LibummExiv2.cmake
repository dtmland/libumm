# Checksum-pinned Exiv2 acquisition (session 05). Version/SHA come from backends.env.
# Decision M4a: pinned source via FetchContent on all OSes. Decision M4c: static Exiv2
# is the default. Session 32 / P2: UMM_EXIV2_SHARED links a shared exiv2 instead
# (system via find_package, else FetchContent shared). Shared linkage is not a
# GPL escape — combined-work distribution is GPL-governed in either mode.

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/LibummPins.cmake")

option(UMM_EXIV2_SHARED
  "Link Exiv2 as a shared library (find_package CONFIG at the pinned minor floor, else FetchContent shared). Default OFF (static, M4c). GPL still governs combined-work distribution in either linkage mode (P2)."
  OFF)

set(UMM_EXIV2_ACQUIRED FALSE)
set(UMM_EXIV2_SYSTEM FALSE)
set(UMM_BUNDLED_EXPAT FALSE)
set(UMM_BUNDLED_ZLIB FALSE)

if(NOT UMM_REQUIRE_EXIV2)
  message(STATUS "Exiv2 not acquired (UMM_REQUIRE_EXIV2=OFF)")
  return()
endif()

if(NOT TARGET umm)
  message(FATAL_ERROR "LibummExiv2.cmake must be included after the umm target is created")
endif()

if(NOT UMM_EXIV2_VERSION MATCHES "^([0-9]+)\\.([0-9]+)")
  message(FATAL_ERROR "UMM_EXIV2_VERSION must be major.minor[.patch], got '${UMM_EXIV2_VERSION}'")
endif()
set(UMM_EXIV2_MIN_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")

macro(umm_attach_exiv2_backend)
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
endmacro()

function(umm_exiv2_package_version out_var)
  if(DEFINED exiv2_VERSION AND NOT exiv2_VERSION STREQUAL "")
    set(${out_var} "${exiv2_VERSION}" PARENT_SCOPE)
  elseif(DEFINED Exiv2_VERSION AND NOT Exiv2_VERSION STREQUAL "")
    set(${out_var} "${Exiv2_VERSION}" PARENT_SCOPE)
  elseif(DEFINED EXIV2_VERSION AND NOT EXIV2_VERSION STREQUAL "")
    set(${out_var} "${EXIV2_VERSION}" PARENT_SCOPE)
  else()
    set(${out_var} "" PARENT_SCOPE)
  endif()
endfunction()

function(umm_exiv2_imported_target out_var)
  foreach(_umm_name IN ITEMS Exiv2::exiv2lib Exiv2::exiv2 exiv2lib)
    if(TARGET "${_umm_name}")
      set(${out_var} "${_umm_name}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${out_var} "" PARENT_SCOPE)
endfunction()

include(FetchContent)

set(_umm_exiv2_url
  "https://github.com/Exiv2/exiv2/archive/refs/tags/v${UMM_EXIV2_VERSION}.tar.gz")
set(_umm_exiv2_download_dir "${PROJECT_SOURCE_DIR}/.cache/exiv2")
set(_umm_exiv2_source_dir "${PROJECT_SOURCE_DIR}/.cache/exiv2/src")

# Session 32: shared mode prefers a consumer/system Exiv2 at or above the
# pinned minor. Do not run this find when the option is OFF — the static
# default must not pick up a system library.
if(UMM_EXIV2_SHARED)
  find_package(exiv2 CONFIG QUIET)
  umm_exiv2_package_version(_umm_exiv2_found_version)
  umm_exiv2_imported_target(_umm_exiv2_found_target)
  if(exiv2_FOUND AND _umm_exiv2_found_target AND _umm_exiv2_found_version
      AND NOT _umm_exiv2_found_version VERSION_LESS UMM_EXIV2_MIN_VERSION)
    umm_attach_exiv2_backend()
    target_link_libraries(umm PRIVATE "${_umm_exiv2_found_target}")
    set(UMM_EXIV2_ACQUIRED TRUE)
    set(UMM_EXIV2_SYSTEM TRUE)
    set(UMM_EXIV2_TARGET "${_umm_exiv2_found_target}")
    file(APPEND "${CMAKE_BINARY_DIR}/backends-acquired.txt"
      "exiv2.version=${_umm_exiv2_found_version}\n"
      "exiv2.bmff=ON\n"
      "exiv2.xmp=ON\n"
      "exiv2.png=ON\n"
      "exiv2.shared=ON\n"
      "exiv2.system=ON\n"
      "exiv2.floor=${UMM_EXIV2_MIN_VERSION}\n")
    message(STATUS
      "Exiv2 ${_umm_exiv2_found_version} (system, shared, floor ${UMM_EXIV2_MIN_VERSION}) linked into umm")
    unset(_umm_exiv2_found_version)
    unset(_umm_exiv2_found_target)
    unset(_umm_exiv2_url)
    unset(_umm_exiv2_download_dir)
    unset(_umm_exiv2_source_dir)
    return()
  endif()
  if(exiv2_FOUND)
    message(STATUS
      "System Exiv2 ${_umm_exiv2_found_version} is below floor ${UMM_EXIV2_MIN_VERSION} or has no imported target; fetching pinned shared Exiv2")
  else()
    message(STATUS
      "No system Exiv2 CONFIG package at floor ${UMM_EXIV2_MIN_VERSION}; fetching pinned shared Exiv2")
  endif()
  unset(_umm_exiv2_found_version)
  unset(_umm_exiv2_found_target)
endif()

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
  set(UMM_BUNDLED_EXPAT TRUE)
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
  # zlib CMake generates zconf.h in BINARY_DIR and renames the copy next to
  # zlib.h. Exiv2 0.28 compiles pngchunk_int.cpp with ZLIB_INCLUDE_DIR only
  # (not ZLIB::ZLIB), so both trees must be on that variable.
  file(WRITE "${CMAKE_BINARY_DIR}/zlib-config-shim/FindZLIB.cmake"
    "if(NOT TARGET ZLIB::ZLIB)\n"
    "  add_library(ZLIB::ZLIB ALIAS zlibstatic)\n"
    "endif()\n"
    "set(ZLIB_FOUND TRUE)\n"
    "set(ZLIB_INCLUDE_DIR \"${_umm_zlib_include_dir}\" \"${_umm_zlib_binary_dir}\")\n"
    "set(ZLIB_INCLUDE_DIRS \"${_umm_zlib_include_dir}\" \"${_umm_zlib_binary_dir}\")\n"
    "set(ZLIB_LIBRARY zlibstatic)\n"
    "set(ZLIB_LIBRARIES zlibstatic)\n")
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/zlib-config-shim")
  set(ZLIB_INCLUDE_DIR "${_umm_zlib_include_dir}" "${_umm_zlib_binary_dir}")
  set(ZLIB_INCLUDE_DIRS "${_umm_zlib_include_dir}" "${_umm_zlib_binary_dir}")
  set(ZLIB_FOUND TRUE)
  unset(_umm_zlib_include_dir)
  unset(_umm_zlib_binary_dir)
  set(UMM_BUNDLED_ZLIB TRUE)
  message(STATUS "Zlib not found on system; fetched for Exiv2 PNG")
endif()

FetchContent_Declare(umm_exiv2
  URL "${_umm_exiv2_url}"
  URL_HASH "SHA256=${UMM_EXIV2_SHA256}"
  DOWNLOAD_DIR "${_umm_exiv2_download_dir}"
  SOURCE_DIR "${_umm_exiv2_source_dir}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Expat/zlib stay static (their FetchContent shims assume that). Only the
# Exiv2 subtree is shared when UMM_EXIV2_SHARED is ON (session 32).
if(UMM_EXIV2_SHARED)
  set(BUILD_SHARED_LIBS ON)
endif()
FetchContent_MakeAvailable(umm_exiv2)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_SKIP_INSTALL_RULES "${_umm_saved_skip_install_rules}")
set(BUILD_SHARED_LIBS "${_umm_saved_build_shared_libs}")

# CMAKE_SKIP_INSTALL_RULES stops FetchContent subdirs from writing
# cmake_install.cmake, but the parent install script still includes those
# paths. Stub the missing files so cmake --install of libumm succeeds
# without installing Exiv2/Expat/zlib packages or headers.
function(umm_stub_skipped_install_script binary_dir)
  if(binary_dir AND NOT EXISTS "${binary_dir}/cmake_install.cmake")
    file(WRITE "${binary_dir}/cmake_install.cmake"
      "# Skipped: FetchContent dependency install rules (CMAKE_SKIP_INSTALL_RULES).\n")
  endif()
endfunction()
umm_stub_skipped_install_script("${umm_exiv2_BINARY_DIR}")
if(UMM_BUNDLED_EXPAT)
  umm_stub_skipped_install_script("${umm_expat_BINARY_DIR}")
endif()
if(UMM_BUNDLED_ZLIB)
  umm_stub_skipped_install_script("${umm_zlib_BINARY_DIR}")
endif()

if(NOT TARGET exiv2lib)
  message(FATAL_ERROR "UMM_REQUIRE_EXIV2=ON but target exiv2lib was not created")
endif()

# Shim lives in src/; Exiv2 writes exv_conf.h / exiv2lib_export.h to CMAKE_BINARY_DIR.
umm_attach_exiv2_backend()
# BUILD_INTERFACE: in-tree tests still link exiv2lib. The exported umm::umm
# target must not require the FetchContent target (session 29 install/export).
target_link_libraries(umm PRIVATE $<BUILD_INTERFACE:exiv2lib>)
set(UMM_EXIV2_TARGET "exiv2lib")

if(UMM_EXIV2_SHARED)
  if(APPLE)
    set_property(TARGET umm APPEND PROPERTY INSTALL_RPATH "@loader_path")
    set_property(TARGET exiv2lib PROPERTY INSTALL_NAME_DIR "@rpath")
    set_property(TARGET exiv2lib PROPERTY MACOSX_RPATH ON)
  elseif(UNIX)
    set_property(TARGET umm APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()
  if(WIN32)
    # In-tree tests and the static umm archive live in CMAKE_BINARY_DIR;
    # Windows loads DLLs from the executable directory first.
    add_custom_command(TARGET umm POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:exiv2lib>"
        "$<TARGET_FILE_DIR:umm>"
      VERBATIM
    )
  endif()
endif()

set(UMM_EXIV2_ACQUIRED TRUE)

file(APPEND "${CMAKE_BINARY_DIR}/backends-acquired.txt"
  "exiv2.version=${UMM_EXIV2_VERSION}\n"
  "exiv2.bmff=ON\n"
  "exiv2.xmp=ON\n"
  "exiv2.png=ON\n")
if(UMM_EXIV2_SHARED)
  file(APPEND "${CMAKE_BINARY_DIR}/backends-acquired.txt"
    "exiv2.shared=ON\n"
    "exiv2.system=OFF\n"
    "exiv2.floor=${UMM_EXIV2_MIN_VERSION}\n")
  message(STATUS
    "Exiv2 ${UMM_EXIV2_VERSION} (BMFF=ON, XMP=ON, PNG=ON, shared FetchContent) linked privately into umm")
else()
  message(STATUS "Exiv2 ${UMM_EXIV2_VERSION} (BMFF=ON, XMP=ON, PNG=ON) linked privately into umm")
endif()

unset(_umm_exiv2_url)
unset(_umm_exiv2_download_dir)
unset(_umm_exiv2_source_dir)
unset(_umm_saved_build_shared_libs)
unset(_umm_saved_skip_install_rules)
