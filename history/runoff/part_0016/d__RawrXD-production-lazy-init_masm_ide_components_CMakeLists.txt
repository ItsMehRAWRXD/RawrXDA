cmake_minimum_required(VERSION 3.16)
project(RawrXD_MASM_Integration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

# Source files
set(MASM_SOURCES
    src/streaming_token_manager.cpp
    src/model_router.cpp
    src/simple_tool_registry.cpp
    src/agentic_planner.cpp
    src/command_palette.cpp
    src/diff_viewer.cpp
    src/masm_integration_manager.cpp
    src/ai_model_client.cpp
    src/aws_bedrock_bridge.cpp
)

set(MASM_HEADERS
    include/streaming_token_manager.h
    include/model_router.h
    include/masm_tool_registry.h
    include/tool_registry.h
    include/agentic_planner.h
    include/command_palette.h
    include/diff_viewer.h
    include/masm_integration_manager.h
    include/ai_model_client.h
    include/aws_bedrock_bridge.h
)

# Create library
add_library(masm_components STATIC ${MASM_SOURCES} ${MASM_HEADERS})
target_include_directories(masm_components PUBLIC include)
target_link_libraries(masm_components PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

# Export for use in main IDE
set(MASM_LIBRARIES masm_components PARENT_SCOPE)
