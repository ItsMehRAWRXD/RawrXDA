# ============================================================================
# Skill System CMake Integration
# ============================================================================
# Add these lines to the main CMakeLists.txt in d:\rawrxd\src\
#
# set(SKILL_SYSTEM_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/skill_system/SkillInjectionEngine.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/skill_system/SkillToggleUI.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/skill_system/SkillSystemBuildIntegration.cpp
# )
#
# target_sources(rawrxd PRIVATE ${SKILL_SYSTEM_SOURCES})
# target_include_directories(rawrxd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/skill_system)
# ============================================================================

# Standalone skill system library (optional)
add_library(rawrxd_skill_system STATIC
    SkillInjectionEngine.cpp
    SkillToggleUI.cpp
    SkillSystemBuildIntegration.cpp
)

target_include_directories(rawrxd_skill_system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(rawrxd_skill_system PUBLIC cxx_std_17)

# Link against Windows libraries
target_link_libraries(rawrxd_skill_system PUBLIC
    shlwapi
    ws2_32
)

# Test executable
if(RAWRXD_BUILD_TESTS)
    add_executable(skill_system_test
        SkillSystemBuildIntegration.cpp
    )
    target_link_libraries(skill_system_test PRIVATE rawrxd_skill_system)
    target_compile_definitions(skill_system_test PRIVATE RAWRXD_BUILD_SMOKE_TEST)
endif()
