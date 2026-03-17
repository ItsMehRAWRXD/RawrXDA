// Comprehensive linker stubs for missing symbols
#include <string>
#include <vector>

// Static instance of VSIXLoader
class VSIXLoader {
public:
    static VSIXLoader& GetInstance();
    void SwitchEngine(const std::string& engine) {}
};

static VSIXLoader g_vsix_loader;

VSIXLoader& VSIXLoader::GetInstance() {
    return g_vsix_loader;
}

// IDEWindow stubs
class IDEWindow {
public:
    void UpdateTabTitle(int index, const std::wstring& title) {}
};

// Namespace stubs
namespace RawrXD {
    struct ReactServerConfig {};
    
    class ReactServerGenerator {
    public:
        static void Generate(const std::string& name, const ReactServerConfig& config) {}
    };
}

// C linkage stubs
extern "C" {
    void register_rawr_inference(void) {}
    void register_sovereign_engines(void) {}
}
