#include "SchematicStudio.h"
#include "IDELogger.h"
#include <fstream>

int main()
{
    IDELogger::getInstance().initialize("SchematicStudioTest.log");

    RawrXD::Schematic::Model model;
    RawrXD::Schematic::Node node;
    node.id = "editor";
    node.label = "Editor";
    node.caps = 0x4;
    node.bounds = {10, 10, 210, 110};
    node.state = RawrXD::Schematic::ApprovalState::Approved;
    model.addNode(node);

    RawrXD::Schematic::Wire wire;
    wire.fromId = "editor";
    wire.toId = "status_bar";
    wire.caps = 0x20;
    model.addWire(wire);

    const std::string path = "schematic_test.json";
    if (!model.saveToFile(path)) {
        return 1;
    }

    RawrXD::Schematic::Model reloaded;
    if (!reloaded.loadFromFile(path)) {
        return 2;
    }

    if (reloaded.nodes().size() != 1 || reloaded.wires().size() != 1) {
        return 3;
    }

    return 0;
}
