#include <iostream>
#include <cassert>
#include "backend/agentic_tools.h"

using namespace RawrXD::Backend;

int main() {
    AgenticToolExecutor exec(".");

    // 1. Initial state should be false
    auto r1 = exec.executeTool("get_split_answers", "{}");
    assert(r1.success);
    assert(r1.result_data == "false");
    std::cout << "PASS: get_split_answers initial = false\n";

    // 2. Toggle to true
    auto r2 = exec.executeTool("toggle_split_answers", "{\"enabled\":true}");
    assert(r2.success);
    assert(r2.result_data == "split_answers=true");
    std::cout << "PASS: toggle_split_answers enabled=true\n";

    // 3. Verify persisted state
    auto r3 = exec.executeTool("get_split_answers", "{}");
    assert(r3.success);
    assert(r3.result_data == "true");
    std::cout << "PASS: get_split_answers after toggle = true\n";

    // 4. Toggle back to false
    auto r4 = exec.executeTool("toggle_split_answers", "{\"enabled\":false}");
    assert(r4.success);
    assert(r4.result_data == "split_answers=false");
    std::cout << "PASS: toggle_split_answers enabled=false\n";

    // 5. Enum dispatch path (via string conversion)
    auto r5 = exec.executeTool("get_split_answers", "{}");
    assert(r5.success);
    assert(r5.result_data == "false");
    std::cout << "PASS: enum dispatch GET_SPLIT_ANSWERS = false\n";

    // 6. Schema registration check
    auto schemas = exec.getToolSchemas();
    bool found_toggle = false, found_get = false;
    for (const auto& s : schemas) {
        if (s.name == "toggle_split_answers") found_toggle = true;
        if (s.name == "get_split_answers") found_get = true;
    }
    assert(found_toggle);
    assert(found_get);
    std::cout << "PASS: schemas registered\n";

    // 7. Invalid parameter handling
    auto r6 = exec.executeTool("toggle_split_answers", "{\"enabled\":\"not_a_bool\"}");
    assert(!r6.success);
    std::cout << "PASS: invalid param rejected\n";

    // 8. stringToTool round-trip
    assert(exec.stringToTool("toggle_split_answers") == AgenticTool::TOGGLE_SPLIT_ANSWERS);
    assert(exec.stringToTool("get_split_answers") == AgenticTool::GET_SPLIT_ANSWERS);
    std::cout << "PASS: stringToTool round-trip\n";

    std::cout << "\n=== ALL 8 TESTS PASSED ===\n";
    return 0;
}
