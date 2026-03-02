#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace RawrXD::IDE {

struct ExtensionItem {
    std::string id;
    std::string name;
    std::string version;
    bool enabled = true;
};

class ExtensionMarketplaceCore {
public:
    explicit ExtensionMarketplaceCore(const std::string& root = ".")
        : root_(root), installDir_(root_ + "/extensions") {
        fs::create_directories(installDir_);
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
        return true;
    }

    bool setEnabled(const std::string& id, bool enabled) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : items_) {
            if (e.id == id) {
                e.enabled = enabled;
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
