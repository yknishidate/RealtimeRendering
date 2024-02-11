cmake_minimum_required(VERSION 3.16)

project(realtime_rendering LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(imguizmo CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)

set(REACTIVE_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(reactive)
add_subdirectory(test)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")
file(GLOB SHADERS shader/*) # exclude spv files

source_group("Shader Files" FILES ${SHADERS})
add_executable(${PROJECT_NAME} ${SOURCES} ${SHADERS})

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

target_link_libraries(${PROJECT_NAME} PUBLIC 
    reactive
    imguizmo::imguizmo
    unofficial::nativefiledialog::nfd
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/reactive/include
    ${TINYGLTF_INCLUDE_DIRS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "DEV_SHADER_DIR=std::filesystem::path{\"${CMAKE_CURRENT_SOURCE_DIR}/shader/\"}")
    
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "DEV_ASSET_DIR=std::filesystem::path{\"${CMAKE_CURRENT_SOURCE_DIR}/asset/\"}")
