cmake_minimum_required(VERSION 3.25)
include(${CMAKE_CURRENT_LIST_DIR}/../LinuxRuntimeFilters.cmake)

function(check_generated_arguments)
  cmake_parse_arguments(PARSE_ARGV 0 actual "NO_TRANSLATIONS" "EXECUTABLE" "POST_EXCLUDE_REGEXES")
  if(actual_UNPARSED_ARGUMENTS OR NOT actual_POST_EXCLUDE_REGEXES STREQUAL MININIDS_LINUX_RUNTIME_EXCLUDES)
    message(FATAL_ERROR "Qt deployment serialization changed the exclusion arguments")
  endif()
  foreach(path IN ITEMS /lib64/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/libc.so.6
      /usr/lib/libm.so.6 /usr/lib/libnss_files.so.2 /usr/lib/libGLX_mesa.so.0
      /usr/lib/libEGL.so.1 /usr/lib/libdrm.so.2 /usr/lib/libvulkan.so.1)
    set(excluded FALSE)
    foreach(pattern IN LISTS actual_POST_EXCLUDE_REGEXES)
      if(path MATCHES "${pattern}")
        set(excluded TRUE)
      endif()
    endforeach()
    if(NOT excluded)
      message(FATAL_ERROR "Native OS library would be bundled: ${path}")
    endif()
  endforeach()
  foreach(path IN ITEMS /Qt/6.8.3/gcc_64/lib/libQt6QuickControls2.so.6
      /Qt/6.8.3/gcc_64/lib/libQt6Core.so.6 /usr/lib/libstdc++.so.6
      /usr/lib/libgcc_s.so.1 /usr/lib/libxcb.so.1 /usr/lib/libfontconfig.so.1)
    foreach(pattern IN LISTS actual_POST_EXCLUDE_REGEXES)
      if(path MATCHES "${pattern}")
        message(FATAL_ERROR "Required runtime library would be excluded: ${path} (${pattern})")
      endif()
    endforeach()
  endforeach()
endfunction()

# Reproduce Qt6QmlMacros.cmake 6.8's raw list interpolation into a generated
# script. This catches parser/regex corruption before a native packaging job.
set(common_deploy_args "    NO_TRANSLATIONS\n")
list(APPEND common_deploy_args POST_EXCLUDE_REGEXES ${MININIDS_LINUX_RUNTIME_EXCLUDES})
cmake_language(EVAL CODE "check_generated_arguments(EXECUTABLE app ${common_deploy_args})")
message(STATUS "Generated deployment arguments and Linux library policy passed")
