// RawrXD-ModelAnalysis — GGUF tensor autopsy + neurological diff (stdout / JSON).
// See docs/ANALYSIS_QUICKSTART.md

#include "core/model_anatomy.hpp"
#include "core/neurological_diff.hpp"

#include <iostream>
#include <string>

static void printUsage()
{
    std::cout
        << "RawrXD-ModelAnalysis — GGUF tensor autopsy & diff\n"
        << "Usage:\n"
        << "  RawrXD-ModelAnalysis.exe --autopsy <model.gguf> [--json]\n"
        << "  RawrXD-ModelAnalysis.exe --diff <A.gguf> <B.gguf> [--json]\n";
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    std::string mode = argv[1];
    bool wantJson = false;
    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--json" || a == "-j")
        {
            wantJson = true;
        }
    }

    if (mode == "--autopsy" || mode == "-a")
    {
        if (argc < 3)
        {
            printUsage();
            return 1;
        }
        const std::string path = argv[2];
        RawrXD::ModelAnatomy an;
        std::string err;
        std::ostream* human = wantJson ? nullptr : &std::cout;
        if (!RawrXD::BuildAnatomyFromGgufPath(path, an, human, &err))
        {
            std::cerr << "[ModelAnalysis] autopsy failed: " << err << '\n';
            return 2;
        }
        if (wantJson)
        {
            std::cout << RawrXD::ExportAnatomyToJson(an, true) << '\n';
        }
        return 0;
    }

    if (mode == "--diff" || mode == "-d")
    {
        if (argc < 4)
        {
            printUsage();
            return 1;
        }
        const std::string pathA = argv[2];
        const std::string pathB = argv[3];
        RawrXD::ModelAnatomy a;
        RawrXD::ModelAnatomy b;
        std::string err;
        if (!RawrXD::BuildAnatomyFromGgufPath(pathA, a, nullptr, &err))
        {
            std::cerr << "[ModelAnalysis] model A: " << err << '\n';
            return 2;
        }
        if (!RawrXD::BuildAnatomyFromGgufPath(pathB, b, nullptr, &err))
        {
            std::cerr << "[ModelAnalysis] model B: " << err << '\n';
            return 2;
        }
        std::ostream* human = wantJson ? nullptr : &std::cout;
        auto diff = RawrXD::DiffAnatomies(a, b, human);
        if (wantJson)
        {
            std::cout << RawrXD::ExportDiffToJson(diff, pathA, pathB, true) << '\n';
        }
        return 0;
    }

    printUsage();
    return 1;
}
