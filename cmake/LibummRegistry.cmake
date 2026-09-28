# Generated property registry (session 07). Committed sources are compiled so
# builds stay offline; the optional custom target regenerates them.

include_guard(GLOBAL)

set(UMM_REGISTRY_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/registry/generate_cpp.py")
set(UMM_REGISTRY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/registry/iptc-photo")
set(UMM_REGISTRY_OVERLAY "${CMAKE_CURRENT_SOURCE_DIR}/registry/mappings/iptc-exif-overlay.json")
set(UMM_REGISTRY_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
set(UMM_REGISTRY_GENERATED_CPP "${UMM_REGISTRY_GENERATED_DIR}/property_registry.cpp")
set(UMM_REGISTRY_GENERATED_HPP "${UMM_REGISTRY_GENERATED_DIR}/property_registry.hpp")

if(NOT EXISTS "${UMM_REGISTRY_GENERATED_CPP}" OR NOT EXISTS "${UMM_REGISTRY_GENERATED_HPP}")
  message(FATAL_ERROR
    "Committed registry sources missing under ${UMM_REGISTRY_GENERATED_DIR}. "
    "Run: python3 tools/registry/generate_cpp.py")
endif()

file(GLOB UMM_REGISTRY_JSON CONFIGURE_DEPENDS "${UMM_REGISTRY_DIR}/*.json")

find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/umm_registry_codegen.stamp"
    COMMAND "${Python3_EXECUTABLE}" "${UMM_REGISTRY_GENERATOR}"
            --registry-dir "${UMM_REGISTRY_DIR}"
            --overlay "${UMM_REGISTRY_OVERLAY}"
            --output-dir "${UMM_REGISTRY_GENERATED_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E touch
            "${CMAKE_CURRENT_BINARY_DIR}/umm_registry_codegen.stamp"
    DEPENDS
      "${UMM_REGISTRY_GENERATOR}"
      "${UMM_REGISTRY_OVERLAY}"
      ${UMM_REGISTRY_JSON}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerate src/generated property registry"
    VERBATIM
  )
  add_custom_target(umm_registry_codegen
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/umm_registry_codegen.stamp"
  )
endif()
