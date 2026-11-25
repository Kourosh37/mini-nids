cmake_minimum_required(VERSION 3.25)
# A deterministic backend refuses to open. Valid durations must reach that failure;
# invalid input must be rejected first, without capturing or waiting out the duration.
set(ENV{MININIDS_CAPTURE_LIBRARY} "${BACKEND}")
foreach(duration IN ITEMS "0" "-0" "0.001" "1.25" "1e-3" "86400")
  execute_process(COMMAND "${CLI}" live --interface fail --output unused --duration "${duration}"
    RESULT_VARIABLE status ERROR_VARIABLE error OUTPUT_VARIABLE output TIMEOUT 5)
  set(diagnostics "${output}${error}")
  if(NOT status EQUAL 1 OR NOT diagnostics MATCHES "fixture permission denied")
    message(FATAL_ERROR "Valid duration '${duration}' did not reach the backend: ${status}: ${diagnostics}")
  endif()
endforeach()
foreach(duration IN ITEMS "" "+1" "-1" "0.0001" "86400.001" "NaN" "inf" "1e999" " 1" "1 " "1,5" "1s")
  execute_process(COMMAND "${CLI}" live --interface fail --output unused --duration "${duration}"
    RESULT_VARIABLE status ERROR_VARIABLE error OUTPUT_QUIET TIMEOUT 5)
  if(NOT status EQUAL 2 OR NOT error MATCHES "duration must be")
    message(FATAL_ERROR "Invalid duration '${duration}' was not rejected: ${status}: ${error}")
  endif()
endforeach()
