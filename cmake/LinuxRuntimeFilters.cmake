# Qt 6.8 writes these values unquoted into generated deployment code. Keep the
# patterns free of grouping parentheses/backslash escapes so they survive that hop.
set(MININIDS_LINUX_RUNTIME_EXCLUDES "/ld-linux[^/]*[.]so")
foreach(library IN ITEMS c m mvec anl pthread dl rt resolv util BrokenLocale
    "nss_[^/]*" "GL[^/]*" "EGL[^/]*" OpenGL gbm "drm[^/]*" "vulkan[^/]*")
  list(APPEND MININIDS_LINUX_RUNTIME_EXCLUDES "/lib${library}[.]so")
endforeach()
