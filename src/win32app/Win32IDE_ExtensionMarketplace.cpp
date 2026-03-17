#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <mutex>

namespace fs = std::filesystem;

namespace RawrXD::IDE {

struct ExtensionItem {
    std::string id;
    std::string name;
    std::string version;
    bool enabled = true;
};

static std::string stateFilePath(const std::string& installDir) {
    return installDir + "/extensions_state.json";
}

class ExtensionMarketplaceCore {
public:
    explicit ExtensionMarketplaceCore(const std::string& root = ".")
        : root_(root), installDir_(root_ + "/extensions") {
        fs::create_directories(installDir_);
        loadFromDisk();
    }

    bool installVsix(const std::string& vsixPath) {
        std::lock_guard<std::mutex> lock(mu_);
        fs::path src(vsixPath);
        if (!fs::exists(src)) return false;

        fs::path dst = fs::path(installDir_) / src.filename();
        std::error_code ec;
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) return false;

        ExtensionItem item;
        item.id = src.stem().string();
        item.name = src.stem().string();
        item.version = "local";
        item.enabled = true;
        upsert(item);
        saveState();
        return true;
    }

    bool setEnabled(const std::string& id, bool enabled) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : items_) {
            if (e.id == id) {
                e.enabled = enabled;
                saveState();
                return true;
            }
        }
        return false;
    }

    int count() const {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int>(items_.size());
    }

private:
    void loadFromDisk() {
        std::lock_guard<std::mutex> lock(mu_);
        items_.clear();
        std::map<std::string, bool> enabledFromState;
        std::string statePath = stateFilePath(installDir_);
        if (fs::exists(statePath)) {
            std::ifstream f(statePath);
            if (f) {
                std::string line;
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    size_t eq = line.find('=');
                    if (eq != std::string::npos) {
                        std::string id = line.substr(0, eq);
                        std::string val = line.substr(eq + 1);
                        enabledFromState[id] = (val == "1" || val == "true");
                    }
                }
            }
        }
        for (const auto& entry : fs::directory_iterator(installDir_)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            if (ext != ".vsix") continue;
            ExtensionItem item;
            item.id = entry.path().stem().string();
            item.name = item.id;
            item.version = "local";
            auto it = enabledFromState.find(item.id);
            item.enabled = (it != enabledFromState.end()) ? it->second : true;
            items_.push_back(item);
        }
    }

    void saveState() {
        std::string path = stateFilePath(installDir_);
        std::ofstream f(path);
        if (!f) return;
        f << "# RawrXD extension enabled state (id=0|1)\n";
        for (const auto& e : items_)
            f << e.id << "=" << (e.enabled ? "1" : "0") << "\n";
    }

    void upsert(const ExtensionItem& item) {
        for (auto& e : items_) {
            if (e.id == item.id) {
                e = item;
                return;
            }
        }
        items_.push_back(item);
    }

    std::string root_;
    std::string installDir_;
    mutable std::mutex mu_;
    std::vector<ExtensionItem> items_;
};

static ExtensionMarketplaceCore g_marketplace(".");

} // namespace RawrXD::IDE

extern "C" {

bool RawrXD_IDE_InstallVsix(const char* vsixPath) {
    if (!vsixPath) return false;
    return RawrXD::IDE::g_marketplace.installVsix(vsixPath);
}

bool RawrXD_IDE_SetExtensionEnabled(const char* extensionId, bool enabled) {
    if (!extensionId) return false;
    return RawrXD::IDE::g_marketplace.setEnabled(extensionId, enabled);
}

int RawrXD_IDE_ExtensionCount() {
    return RawrXD::IDE::g_marketplace.count();
}

}
