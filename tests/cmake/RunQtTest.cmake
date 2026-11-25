cmake_minimum_required(VERSION 3.25)
# QtTest's Windows binary SDK can suppress stdout even for a console executable.
# A file logger plus explicit forwarding preserves every assertion in CTest/CI logs.
file(REMOVE "${REPORT_FILE}")
execute_process(COMMAND "${EXECUTABLE}" -o "${REPORT_FILE},txt"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 600)
if(EXISTS "${REPORT_FILE}")
  file(READ "${REPORT_FILE}" report)
  message("${report}")
endif()
if(NOT "${status}" STREQUAL "0")
  message(FATAL_ERROR "Qt controller test failed (${status})\n${output}${error}")
endif()
