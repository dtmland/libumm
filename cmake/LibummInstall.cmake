# Session 29 — install rules and CMake package export.
#
# Chosen static-transitive mechanism: install private dependency *archives* as
# IMPORTED STATIC targets of the export (not object-library merge).
# FetchContent Exiv2/Expat/zlib keep CMAKE_SKIP_INSTALL_RULES so their headers
# and CMake packages never enter the prefix. LibummExiv2 stubs the missing
# subdirectory cmake_install.cmake files so parent `cmake --install` still
# succeeds. The static libumm archive does
# not contain Exiv2 objects; consumers of umm::umm therefore need those
# archives at link time. ummConfig.cmake reconstructs IMPORTED locations under
# ${CMAKE_INSTALL_LIBDIR}/umm/ and does not locate Exiv2 as a CMake package.
# System zlib/expat (typical on Linux CI) are find_dependency()'d.
# Shared-library install verification is session 32's cut line.

include_guard(GLOBAL)

if(NOT TARGET umm)
  message(FATAL_ERROR "LibummInstall.cmake must be included after the umm target is created")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

target_include_directories(umm
  PUBLIC
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(umm PUBLIC cxx_std_20)

set(UMM_INSTALL_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/umm")
set(UMM_INSTALL_PRIVATE_LIBDIR "${CMAKE_INSTALL_LIBDIR}/umm")
set(UMM_INSTALL_FIND_ZLIB FALSE)
set(UMM_INSTALL_FIND_EXPAT FALSE)
set(UMM_INSTALL_FIND_ICONV FALSE)
set(UMM_INSTALL_FIND_THREADS FALSE)
set(UMM_INSTALL_SYSTEM_LIBS "")
set(UMM_INSTALL_BUNDLE_EXIV2 FALSE)
set(UMM_INSTALL_BUNDLE_ZLIB FALSE)
set(UMM_INSTALL_BUNDLE_EXPAT FALSE)

function(umm_real_target name out_var)
  if(NOT TARGET "${name}")
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()
  get_target_property(_umm_alias "${name}" ALIASED_TARGET)
  if(_umm_alias)
    set(${out_var} "${_umm_alias}" PARENT_SCOPE)
  else()
    set(${out_var} "${name}" PARENT_SCOPE)
  endif()
endfunction()

function(umm_target_is_bundled_static name out_var)
  set(${out_var} FALSE PARENT_SCOPE)
  umm_real_target("${name}" _umm_tgt)
  if(NOT _umm_tgt)
    return()
  endif()
  get_target_property(_umm_type "${_umm_tgt}" TYPE)
  if(NOT _umm_type STREQUAL "STATIC_LIBRARY")
    return()
  endif()
  get_target_property(_umm_imported "${_umm_tgt}" IMPORTED)
  if(_umm_imported)
    return()
  endif()
  set(${out_var} TRUE PARENT_SCOPE)
endfunction()

function(umm_unwrap_link_item item out_var)
  set(_umm_item "${item}")
  foreach(_umm_pass RANGE 4)
    if(_umm_item MATCHES "^\\$<LINK_ONLY:(.*)>$")
      set(_umm_item "${CMAKE_MATCH_1}")
    elseif(_umm_item MATCHES "^\\$<BUILD_INTERFACE:(.*)>$")
      set(_umm_item "${CMAKE_MATCH_1}")
    elseif(_umm_item MATCHES "^\\$<INSTALL_INTERFACE:(.*)>$")
      set(_umm_item "")
      break()
    else()
      break()
    endif()
  endforeach()
  set(${out_var} "${_umm_item}" PARENT_SCOPE)
endfunction()

function(umm_collect_exiv2_link_usage)
  if(NOT TARGET exiv2lib)
    return()
  endif()
  set(_umm_libs "")
  get_target_property(_umm_link exiv2lib LINK_LIBRARIES)
  if(_umm_link)
    list(APPEND _umm_libs ${_umm_link})
  endif()
  get_target_property(_umm_iface exiv2lib INTERFACE_LINK_LIBRARIES)
  if(_umm_iface)
    list(APPEND _umm_libs ${_umm_iface})
  endif()
  if(NOT _umm_libs)
    return()
  endif()
  list(REMOVE_DUPLICATES _umm_libs)
  set(_umm_system "")
  foreach(_umm_raw IN LISTS _umm_libs)
    if(NOT _umm_raw)
      continue()
    endif()
    umm_unwrap_link_item("${_umm_raw}" _umm_item)
    if(NOT _umm_item OR _umm_item MATCHES "^\\$<")
      continue()
    endif()
    # Exiv2 may store Iconv::Iconv as a bare name before FindIconv creates
    # the target. Never pass imported-target names through as system libs.
    if(_umm_item MATCHES "Iconv::Iconv" OR _umm_item STREQUAL "Iconv"
        OR _umm_item STREQUAL "iconv" OR _umm_item MATCHES "libiconv")
      set(UMM_INSTALL_FIND_ICONV TRUE PARENT_SCOPE)
      continue()
    endif()
    if(TARGET "${_umm_item}")
      umm_real_target("${_umm_item}" _umm_tgt)
      if(_umm_item STREQUAL "ZLIB::ZLIB" OR _umm_tgt STREQUAL "zlibstatic" OR _umm_tgt STREQUAL "zlib")
        umm_target_is_bundled_static("${_umm_item}" _umm_bundled)
        if(_umm_bundled)
          set(UMM_INSTALL_BUNDLE_ZLIB TRUE PARENT_SCOPE)
        else()
          set(UMM_INSTALL_FIND_ZLIB TRUE PARENT_SCOPE)
        endif()
      elseif(_umm_item STREQUAL "EXPAT::EXPAT" OR _umm_tgt STREQUAL "expat")
        umm_target_is_bundled_static("${_umm_item}" _umm_bundled)
        if(_umm_bundled)
          set(UMM_INSTALL_BUNDLE_EXPAT TRUE PARENT_SCOPE)
        else()
          set(UMM_INSTALL_FIND_EXPAT TRUE PARENT_SCOPE)
        endif()
      elseif(_umm_item STREQUAL "Threads::Threads")
        set(UMM_INSTALL_FIND_THREADS TRUE PARENT_SCOPE)
      endif()
    elseif(NOT _umm_item MATCHES "::")
      list(APPEND _umm_system "${_umm_item}")
    endif()
  endforeach()
  set(UMM_INSTALL_SYSTEM_LIBS "${_umm_system}" PARENT_SCOPE)
endfunction()

if(UMM_EXIV2_ACQUIRED AND TARGET exiv2lib)
  set(UMM_INSTALL_BUNDLE_EXIV2 TRUE)
  umm_collect_exiv2_link_usage()
  if(UMM_BUNDLED_ZLIB)
    set(UMM_INSTALL_BUNDLE_ZLIB TRUE)
    set(UMM_INSTALL_FIND_ZLIB FALSE)
  endif()
  if(UMM_BUNDLED_EXPAT)
    set(UMM_INSTALL_BUNDLE_EXPAT TRUE)
    set(UMM_INSTALL_FIND_EXPAT FALSE)
  endif()
endif()

if(BUILD_SHARED_LIBS)
  set(UMM_INSTALL_IS_STATIC FALSE)
else()
  set(UMM_INSTALL_IS_STATIC TRUE)
endif()

if(TARGET Iconv::Iconv OR (APPLE AND UMM_INSTALL_BUNDLE_EXIV2))
  set(UMM_INSTALL_FIND_ICONV TRUE)
endif()

# Static umm privately links FetchContent exiv2lib. CMake still exports
# exiv2lib's imported usage requirements (Iconv::Iconv on Apple) into
# ummTargets.cmake. Keep those names BUILD_INTERFACE-only; ummConfig.cmake
# reattaches reconstructed private archives and find_dependency() results.
get_target_property(_umm_iface_link umm INTERFACE_LINK_LIBRARIES)
if(_umm_iface_link AND NOT _umm_iface_link STREQUAL "NOTFOUND")
  set(_umm_install_iface)
  foreach(_umm_iface_item IN LISTS _umm_iface_link)
    list(APPEND _umm_install_iface "$<BUILD_INTERFACE:${_umm_iface_item}>")
  endforeach()
  set_property(TARGET umm PROPERTY INTERFACE_LINK_LIBRARIES "${_umm_install_iface}")
  unset(_umm_install_iface)
  unset(_umm_iface_item)
endif()
unset(_umm_iface_link)

install(TARGETS umm
  EXPORT ummTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/umm"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.hpp"
)

if(UMM_INSTALL_IS_STATIC AND UMM_INSTALL_BUNDLE_EXIV2)
  install(FILES "$<TARGET_FILE:exiv2lib>"
    DESTINATION "${UMM_INSTALL_PRIVATE_LIBDIR}"
  )
  if(UMM_INSTALL_BUNDLE_ZLIB)
    umm_real_target(ZLIB::ZLIB _umm_zlib_tgt)
    if(NOT _umm_zlib_tgt AND TARGET zlibstatic)
      set(_umm_zlib_tgt zlibstatic)
    endif()
    if(_umm_zlib_tgt)
      install(FILES "$<TARGET_FILE:${_umm_zlib_tgt}>"
        DESTINATION "${UMM_INSTALL_PRIVATE_LIBDIR}"
      )
    endif()
  endif()
  if(UMM_INSTALL_BUNDLE_EXPAT)
    umm_real_target(EXPAT::EXPAT _umm_expat_tgt)
    if(NOT _umm_expat_tgt AND TARGET expat)
      set(_umm_expat_tgt expat)
    endif()
    if(_umm_expat_tgt)
      install(FILES "$<TARGET_FILE:${_umm_expat_tgt}>"
        DESTINATION "${UMM_INSTALL_PRIVATE_LIBDIR}"
      )
    endif()
  endif()
endif()

set(_umm_private_deps_content
  "# Generated private archive names for the installed umm package (session 29).\n"
  "set(UMM_PRIVATE_EXIV2_FILE \"\")\n"
  "set(UMM_PRIVATE_ZLIB_FILE \"\")\n"
  "set(UMM_PRIVATE_EXPAT_FILE \"\")\n"
)
if(UMM_INSTALL_IS_STATIC AND UMM_INSTALL_BUNDLE_EXIV2)
  set(_umm_private_deps_content
    "# Generated private archive names for the installed umm package (session 29).\n"
    "set(UMM_PRIVATE_EXIV2_FILE \"$<TARGET_FILE_NAME:exiv2lib>\")\n"
  )
  if(UMM_INSTALL_BUNDLE_ZLIB)
    umm_real_target(ZLIB::ZLIB _umm_zlib_gen)
    if(NOT _umm_zlib_gen AND TARGET zlibstatic)
      set(_umm_zlib_gen zlibstatic)
    endif()
    if(_umm_zlib_gen)
      list(APPEND _umm_private_deps_content
        "set(UMM_PRIVATE_ZLIB_FILE \"$<TARGET_FILE_NAME:${_umm_zlib_gen}>\")\n"
      )
    else()
      list(APPEND _umm_private_deps_content "set(UMM_PRIVATE_ZLIB_FILE \"\")\n")
    endif()
  else()
    list(APPEND _umm_private_deps_content "set(UMM_PRIVATE_ZLIB_FILE \"\")\n")
  endif()
  if(UMM_INSTALL_BUNDLE_EXPAT)
    umm_real_target(EXPAT::EXPAT _umm_expat_gen)
    if(NOT _umm_expat_gen AND TARGET expat)
      set(_umm_expat_gen expat)
    endif()
    if(_umm_expat_gen)
      list(APPEND _umm_private_deps_content
        "set(UMM_PRIVATE_EXPAT_FILE \"$<TARGET_FILE_NAME:${_umm_expat_gen}>\")\n"
      )
    else()
      list(APPEND _umm_private_deps_content "set(UMM_PRIVATE_EXPAT_FILE \"\")\n")
    endif()
  else()
    list(APPEND _umm_private_deps_content "set(UMM_PRIVATE_EXPAT_FILE \"\")\n")
  endif()
endif()
string(JOIN "" _umm_private_deps_joined ${_umm_private_deps_content})
file(GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ummPrivateDeps.cmake"
  CONTENT "${_umm_private_deps_joined}"
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ummConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ummConfig.cmake"
  INSTALL_DESTINATION "${UMM_INSTALL_EXPORT_DIR}"
)

# Session 30 / docs/abi-policy.md: SameMinorVersion pre-1.0, SameMajorVersion from 1.0.
if(NOT UMM_PACKAGE_COMPATIBILITY)
  message(FATAL_ERROR "UMM_PACKAGE_COMPATIBILITY must be set (docs/abi-policy.md)")
endif()

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ummConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY ${UMM_PACKAGE_COMPATIBILITY}
)

install(EXPORT ummTargets
  NAMESPACE umm::
  FILE ummTargets.cmake
  DESTINATION "${UMM_INSTALL_EXPORT_DIR}"
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/ummConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/ummConfigVersion.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/ummPrivateDeps.cmake"
  DESTINATION "${UMM_INSTALL_EXPORT_DIR}"
)

unset(_umm_private_deps_content)
unset(_umm_private_deps_joined)
unset(_umm_zlib_tgt)
unset(_umm_expat_tgt)
unset(_umm_zlib_gen)
unset(_umm_expat_gen)
