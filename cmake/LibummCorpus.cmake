# Tier B checksummed corpus (session 27, test-media-plan §3).

include_guard(GLOBAL)

set(UMM_CORPUS_MANIFEST "${PROJECT_SOURCE_DIR}/tests/corpus/manifest.json")
set(UMM_CORPUS_FETCHER "${PROJECT_SOURCE_DIR}/tools/corpus/fetch.py")
set(UMM_CORPUS_DIR "${PROJECT_SOURCE_DIR}/.cache/corpus")
set(UMM_CORPUS_READY FALSE)

if(UMM_TIER_B)
  if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "UMM_TIER_B=ON requires a Python 3 interpreter")
  endif()
  if(NOT EXISTS "${UMM_CORPUS_MANIFEST}")
    message(FATAL_ERROR "UMM_TIER_B=ON but missing ${UMM_CORPUS_MANIFEST}")
  endif()
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${UMM_CORPUS_FETCHER}"
            --manifest "${UMM_CORPUS_MANIFEST}"
            --dest "${UMM_CORPUS_DIR}"
            --require
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    RESULT_VARIABLE _umm_corpus_fetch_result
    OUTPUT_VARIABLE _umm_corpus_fetch_output
    ERROR_VARIABLE _umm_corpus_fetch_error
  )
  if(NOT _umm_corpus_fetch_result EQUAL 0)
    message(FATAL_ERROR
      "Tier B corpus fetch failed (fail-closed):\n${_umm_corpus_fetch_output}\n${_umm_corpus_fetch_error}")
  endif()
  if(NOT EXISTS "${UMM_CORPUS_DIR}/jpeg/makernote.jpg")
    message(FATAL_ERROR "Tier B fetch succeeded but jpeg/makernote.jpg is missing")
  endif()
  set(UMM_CORPUS_READY TRUE)
  file(TO_CMAKE_PATH "${UMM_CORPUS_DIR}" UMM_CORPUS_DIR_CMAKE)
  message(STATUS "Tier B corpus ready under ${UMM_CORPUS_DIR}")
endif()

function(umm_apply_corpus_dir target)
  if(UMM_CORPUS_READY)
    target_compile_definitions(${target}
      PRIVATE
        UMM_TIER_B=1
        "UMM_CORPUS_DIR=R\"(${UMM_CORPUS_DIR_CMAKE})\""
    )
  endif()
endfunction()

function(umm_apply_corpus_dir_to_test test_name)
  if(UMM_CORPUS_READY)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "UMM_CORPUS_DIR=set:${UMM_CORPUS_DIR}")
  endif()
endfunction()
