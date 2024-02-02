include(CTest)

add_executable(${PROJECT_NAME}_TEST "Handle.test.cpp"  "Main.test.cpp")
target_link_libraries(${PROJECT_NAME}_TEST PRIVATE tsl::robin_map)
target_link_libraries(${PROJECT_NAME}_TEST PRIVATE GTest::gtest GTest::gmock)
target_compile_features(${PROJECT_NAME}_TEST PRIVATE cxx_std_20)
add_test(NAME TestName COMMAND ${PROJECT_NAME}_TEST)
