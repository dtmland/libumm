# CTest driver: <perl> <exiftool> -ver must equal the pinned version exactly.

if(NOT UMM_TEST_EXIFTOOL_SCRIPT)
  message(FATAL_ERROR "UMM_TEST_EXIFTOOL_SCRIPT is not set")
endif()
if(NOT UMM_TEST_EXIFTOOL_PERL)
  message(FATAL_ERROR "UMM_TEST_EXIFTOOL_PERL is not set")
endif()
if(NOT UMM_TEST_EXIFTOOL_VERSION)
  message(FATAL_ERROR "UMM_TEST_EXIFTOOL_VERSION is not set")
endif()

execute_process(
  COMMAND "${UMM_TEST_EXIFTOOL_PERL}" "${UMM_TEST_EXIFTOOL_SCRIPT}" -ver
  RESULT_VARIABLE _umm_exiftool_rc
  OUTPUT_VARIABLE _umm_exiftool_out
  ERROR_VARIABLE _umm_exiftool_err
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT _umm_exiftool_rc EQUAL 0)
  message(FATAL_ERROR
    "exiftool -ver failed (${_umm_exiftool_rc}): ${_umm_exiftool_err}")
endif()

if(NOT _umm_exiftool_out STREQUAL UMM_TEST_EXIFTOOL_VERSION)
  message(FATAL_ERROR
    "exiftool -ver is '${_umm_exiftool_out}', expected '${UMM_TEST_EXIFTOOL_VERSION}'")
endif()
