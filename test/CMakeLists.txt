include(CTest)

add_executable(${PROJECT_NAME}_TEST "Handle.test.cpp")
target_link_libraries(${PROJECT_NAME}_TEST PRIVATE tsl::robin_map)
target_link_libraries(${PROJECT_NAME}_TEST PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_test(NAME TestName COMMAND ${PROJECT_NAME}_TEST)
