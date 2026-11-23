add_library(mininids_core src/Types.cpp src/PcapReader.cpp src/ProtocolDispatcher.cpp src/DnsParser.cpp)
target_sources(mininids_core PRIVATE src/EngineConfig.cpp src/FlowTracker.cpp src/TrafficWindow.cpp)
target_include_directories(mininids_core PUBLIC include)
target_link_libraries(mininids_core PUBLIC mininids_options nlohmann_json::nlohmann_json)
