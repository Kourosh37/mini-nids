add_executable(mininids_tests TypesTest.cpp ParserTest.cpp StateTest.cpp DetectionTest.cpp)
target_link_libraries(mininids_tests PRIVATE mininids_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(mininids_tests DISCOVERY_MODE PRE_TEST)
