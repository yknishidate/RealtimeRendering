cmake_minimum_required(VERSION 3.16)

project(test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    reactive
    GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/../reactive/include
)

add_test(AllTestsInMain main)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "DEV_ASSET_DIR=std::filesystem::path{\"${CMAKE_CURRENT_SOURCE_DIR}/asset/\"}")
