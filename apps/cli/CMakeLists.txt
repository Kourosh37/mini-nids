add_executable(mininids main.cpp)
target_link_libraries(mininids PRIVATE mininids_core spdlog::spdlog)
install(TARGETS mininids RUNTIME DESTINATION bin)
