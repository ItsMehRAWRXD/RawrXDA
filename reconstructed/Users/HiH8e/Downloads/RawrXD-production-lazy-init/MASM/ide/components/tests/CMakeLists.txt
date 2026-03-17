cmake_minimum_required(VERSION 3.16)
project(MASMPortTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Set Qt path
set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_executable(masm_port_test
    ${CMAKE_CURRENT_SOURCE_DIR}/component_test.cpp
    ${SOURCE_DIR}/streaming_token_manager.cpp
    ${INCLUDE_DIR}/streaming_token_manager.h
    ${SOURCE_DIR}/model_router.cpp
    ${INCLUDE_DIR}/model_router.h
    ${SOURCE_DIR}/simple_tool_registry.cpp
    ${INCLUDE_DIR}/tool_registry.h
    ${SOURCE_DIR}/agentic_planner.cpp
    ${INCLUDE_DIR}/agentic_planner.h
    ${SOURCE_DIR}/command_palette.cpp
    ${INCLUDE_DIR}/command_palette.h
    ${SOURCE_DIR}/diff_viewer.cpp
    ${INCLUDE_DIR}/diff_viewer.h
    ${SOURCE_DIR}/ai_model_client.cpp
    ${INCLUDE_DIR}/ai_model_client.h
    ${SOURCE_DIR}/aws_bedrock_bridge.cpp
    ${INCLUDE_DIR}/aws_bedrock_bridge.h
)

target_include_directories(masm_port_test PRIVATE ${INCLUDE_DIR})
target_link_libraries(masm_port_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
