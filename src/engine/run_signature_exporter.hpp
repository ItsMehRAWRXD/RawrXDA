#pragma once

#include "replay_core.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace RawrXD::Replay {

class RunSignatureExporter {
  public:
    struct ExportResult {
        bool success = false;
        std::string signature_json_path;
        std::string dag_bin_path;
        std::string hash_txt_path;
        std::string error;
    };

    static ExportResult Export(const ReplayGraph& graph, const std::filesystem::path& output_dir,
                               const std::string& run_name)
    {
        ExportResult out;

        std::error_code ec;
        std::filesystem::create_directories(output_dir, ec);
        if (ec)
        {
            out.error = "failed to create output directory";
            return out;
        }

        const std::filesystem::path hash_path = output_dir / (run_name + ".hash.txt");
        const std::filesystem::path dag_path = output_dir / (run_name + ".dag.bin");
        const std::filesystem::path sig_path = output_dir / (run_name + ".signature.json");

        if (!WriteHash(graph, hash_path))
        {
            out.error = "failed to write hash file";
            return out;
        }
        if (!WriteDag(graph, dag_path))
        {
            out.error = "failed to write DAG binary";
            return out;
        }
        if (!WriteSignature(graph, sig_path, hash_path.filename().string(), dag_path.filename().string()))
        {
            out.error = "failed to write signature json";
            return out;
        }

        out.success = true;
        out.signature_json_path = sig_path.string();
        out.dag_bin_path = dag_path.string();
        out.hash_txt_path = hash_path.string();
        return out;
    }

  private:
    static bool WriteHash(const ReplayGraph& graph, const std::filesystem::path& path)
    {
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            return false;
        }

        ofs << std::hex << std::setw(16) << std::setfill('0') << graph.canonical_hash << "\n";
        return ofs.good();
    }

    static bool WriteDag(const ReplayGraph& graph, const std::filesystem::path& path)
    {
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            return false;
        }

        const uint64_t node_count = static_cast<uint64_t>(graph.nodes.size());
        const uint64_t edge_count = static_cast<uint64_t>(graph.edges.size());
        ofs.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        ofs.write(reinterpret_cast<const char*>(&edge_count), sizeof(edge_count));
        ofs.write(reinterpret_cast<const char*>(graph.nodes.data()),
                  static_cast<std::streamsize>(graph.nodes.size() * sizeof(ReplayNode)));
        ofs.write(reinterpret_cast<const char*>(graph.edges.data()),
                  static_cast<std::streamsize>(graph.edges.size() * sizeof(ReplayEdge)));
        return ofs.good();
    }

    static bool WriteSignature(const ReplayGraph& graph, const std::filesystem::path& path,
                               const std::string& hash_filename, const std::string& dag_filename)
    {
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            return false;
        }

        std::ostringstream hash_hex;
        hash_hex << std::hex << std::setw(16) << std::setfill('0') << graph.canonical_hash;

        ofs << "{\n";
        ofs << "  \"canonical_hash\": \"" << hash_hex.str() << "\",\n";
        ofs << "  \"causal_integrity_ok\": " << (graph.causal_integrity_ok ? "true" : "false") << ",\n";
        ofs << "  \"node_count\": " << graph.nodes.size() << ",\n";
        ofs << "  \"edge_count\": " << graph.edges.size() << ",\n";
        ofs << "  \"hash_file\": \"" << hash_filename << "\",\n";
        ofs << "  \"dag_file\": \"" << dag_filename << "\"\n";
        ofs << "}\n";
        return ofs.good();
    }
};

}  // namespace RawrXD::Replay
