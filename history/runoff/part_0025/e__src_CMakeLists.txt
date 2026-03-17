# Create the library
add_library(AgentHotPatcher
    agent_hot_patcher.hpp
    agent_hot_patcher.cpp
)

# Set C++ standard
target_compile_features(AgentHotPatcher PUBLIC cxx_std_17)

# Link Qt libraries
target_link_libraries(AgentHotPatcher 
    Qt6::Core
)

# Include current directory
target_include_directories(AgentHotPatcher PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler-specific options
if(MSVC)
    target_compile_options(AgentHotPatcher PRIVATE /W4)
else()
    target_compile_options(AgentHotPatcher PRIVATE -Wall -Wextra -pedantic)
endif()

# Install rules
install(TARGETS AgentHotPatcher
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES agent_hot_patcher.hpp DESTINATION include)