# Embedded disk storage is shared by the Qt-free statistics engine and GUI sessions.
include(FetchContent)
if(NOT MININIDS_FETCH_DEPS)
  find_package(SQLite3 3.34 REQUIRED)
  add_library(mininids_database ALIAS SQLite::SQLite3)
  return()
endif()
FetchContent_Declare(mininids_sqlite
  URL https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip
  URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mininids_sqlite)
add_library(mininids_database STATIC "${mininids_sqlite_SOURCE_DIR}/sqlite3.c")
target_include_directories(mininids_database SYSTEM PUBLIC "${mininids_sqlite_SOURCE_DIR}")
target_compile_definitions(mininids_database PRIVATE SQLITE_ENABLE_FTS5 SQLITE_OMIT_LOAD_EXTENSION)
set_target_properties(mininids_database PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(mininids_database PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
