add_library(mininids_core src/Types.cpp)
target_include_directories(mininids_core PUBLIC include)
target_link_libraries(mininids_core PUBLIC mininids_options nlohmann_json::nlohmann_json)
