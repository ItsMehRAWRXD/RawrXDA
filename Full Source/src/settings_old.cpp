#include "settings.h"
#include <filesystem>
#include <fstream>
#include <sstream>
// Lightweight constructor - no // Settings initialization removed
        settings_ = nullptr;
    }
}

// Two-phase init: Create // Settings initialization removed
    }
}

void Settings::setValue(const std::string& key, const std::any& value) {
    if (settings_) {
        settings_->setValue(key, value);
        settings_->sync();  // Force immediate write
    }
}

std::any Settings::getValue(const std::string& key, const std::any& default_value) {
    if (settings_) {
        return settings_->value(key, default_value);
    }
    return default_value;
}

static void EnsureSettingsDir(const std::string& path) {
    std::filesystem::path p(path);
    auto dir = p.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::error_code ec; std::filesystem::create_directories(dir, ec);
    }
}

bool Settings::LoadCompute(AppState& state, const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0]=='#') continue;
        auto eq = line.find('=');
        if (eq==std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        bool b = (val=="1" || val=="true" || val=="TRUE");
        if (key=="enable_gpu_matmul") state.enable_gpu_matmul = b;
        else if (key=="enable_gpu_attention") state.enable_gpu_attention = b;
        else if (key=="enable_cpu_gpu_compare") state.enable_cpu_gpu_compare = b;
        else if (key=="enable_detailed_quant") state.enable_detailed_quant = b;
    }
    state.compute_settings_dirty = false;
    return true;
}

bool Settings::SaveCompute(const AppState& state, const std::string& path) {
    EnsureSettingsDir(path);
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << "# RawrXD Model Loader Compute Settings\n";
    ofs << "enable_gpu_matmul=" << (state.enable_gpu_matmul?"1":"0") << "\n";
    ofs << "enable_gpu_attention=" << (state.enable_gpu_attention?"1":"0") << "\n";
    ofs << "enable_cpu_gpu_compare=" << (state.enable_cpu_gpu_compare?"1":"0") << "\n";
    ofs << "enable_detailed_quant=" << (state.enable_detailed_quant?"1":"0") << "\n";
    return true;
}

bool Settings::LoadOverclock(AppState& state, const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0]=='#') continue;
        auto eq = line.find('=');
        if (eq==std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        auto toBool = [&](const std::string& s){ return (s=="1"||s=="true"||s=="TRUE"); };
        try {
            if (key=="enable_overclock_governor") state.enable_overclock_governor = toBool(val);
            else if (key=="target_all_core_mhz") state.target_all_core_mhz = (uint32_t)std::stoul(val);
            else if (key=="boost_step_mhz") state.boost_step_mhz = (uint32_t)std::stoul(val);
            else if (key=="max_cpu_temp_c") state.max_cpu_temp_c = (uint32_t)std::stoul(val);
            else if (key=="max_gpu_hotspot_c") state.max_gpu_hotspot_c = (uint32_t)std::stoul(val);
            else if (key=="max_core_voltage") state.max_core_voltage = std::stof(val);
            else if (key=="pid_kp") state.pid_kp = std::stof(val);
            else if (key=="pid_ki") state.pid_ki = std::stof(val);
            else if (key=="pid_kd") state.pid_kd = std::stof(val);
            else if (key=="pid_integral_clamp") state.pid_integral_clamp = std::stof(val);
            else if (key=="gpu_pid_kp") state.gpu_pid_kp = std::stof(val);
            else if (key=="gpu_pid_ki") state.gpu_pid_ki = std::stof(val);
            else if (key=="gpu_pid_kd") state.gpu_pid_kd = std::stof(val);
            else if (key=="gpu_pid_integral_clamp") state.gpu_pid_integral_clamp = std::stof(val);
        } catch(...) { /* ignore malformed values */ }
    }
    state.overclock_settings_dirty = false;
    return true;
}

bool Settings::SaveOverclock(const AppState& state, const std::string& path) {
    EnsureSettingsDir(path);
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << "# RawrXD Model Loader Overclock Settings\n";
    ofs << "enable_overclock_governor=" << (state.enable_overclock_governor?"1":"0") << "\n";
    ofs << "target_all_core_mhz=" << state.target_all_core_mhz << "\n";
    ofs << "boost_step_mhz=" << state.boost_step_mhz << "\n";
    ofs << "max_cpu_temp_c=" << state.max_cpu_temp_c << "\n";
    ofs << "max_gpu_hotspot_c=" << state.max_gpu_hotspot_c << "\n";
    ofs << "max_core_voltage=" << state.max_core_voltage << "\n";
    ofs << "pid_kp=" << state.pid_kp << "\n";
    ofs << "pid_ki=" << state.pid_ki << "\n";
    ofs << "pid_kd=" << state.pid_kd << "\n";
    ofs << "pid_integral_clamp=" << state.pid_integral_clamp << "\n";
    ofs << "gpu_pid_kp=" << state.gpu_pid_kp << "\n";
    ofs << "gpu_pid_ki=" << state.gpu_pid_ki << "\n";
    ofs << "gpu_pid_kd=" << state.gpu_pid_kd << "\n";
    ofs << "gpu_pid_integral_clamp=" << state.gpu_pid_integral_clamp << "\n";
    return true;
}

// ==============================================================================
// Monaco Editor Settings - Theme Presets
// ==============================================================================

MonacoThemeColors Settings::GetThemePresetColors(MonacoThemePreset preset) {
    MonacoThemeColors colors;
    
    switch (preset) {
        case MonacoThemePreset::Default: // VS Code Dark+
            colors.background = 0x001E1E1E;
            colors.foreground = 0x00D4D4D4;
            colors.lineNumber = 0x00858585;
            colors.cursorColor = 0x00AEAFAD;
            colors.selectionBg = 0x00264F78;
            colors.lineHighlight = 0x00282828;
            colors.keyword = 0x00569CD6;
            colors.string = 0x00CE9178;
            colors.number = 0x00B5CEA8;
            colors.comment = 0x006A9955;
            colors.function = 0x00DCDCAA;
            colors.type = 0x004EC9B0;
            colors.variable = 0x009CDCFE;
            colors.operator_ = 0x00D4D4D4;
            colors.preprocessor = 0x00C586C0;
            colors.constant = 0x004FC1FF;
            colors.glowColor = 0x0000FFFF;
            colors.glowSecondary = 0x00FF00FF;
            break;
            
        case MonacoThemePreset::NeonCyberpunk:
            colors.background = 0x00080808;
            colors.foreground = 0x0000FF99;
            colors.lineNumber = 0x00FF00FF;
            colors.cursorColor = 0x0000FFFF;
            colors.selectionBg = 0x00330066;
            colors.lineHighlight = 0x00111122;
            colors.keyword = 0x00FF00FF;       // Magenta
            colors.string = 0x0000FF99;        // Neon green
            colors.number = 0x00FFFF00;        // Yellow
            colors.comment = 0x00666699;
            colors.function = 0x0000FFFF;      // Cyan
            colors.type = 0x00FF6699;          // Pink
            colors.variable = 0x0099FFFF;      // Light cyan
            colors.operator_ = 0x00FFFFFF;
            colors.preprocessor = 0x00FF9900;  // Orange
            colors.constant = 0x00FFFF00;
            colors.glowColor = 0x0000FFFF;     // Cyan glow
            colors.glowSecondary = 0x00FF00FF; // Magenta glow
            colors.scanlineColor = 0x00101010;
            colors.particleColor = 0x0000FF66;
            break;
            
        case MonacoThemePreset::MatrixGreen:
            colors.background = 0x00000000;
            colors.foreground = 0x0000FF00;
            colors.lineNumber = 0x00006600;
            colors.cursorColor = 0x0000FF00;
            colors.selectionBg = 0x00003300;
            colors.lineHighlight = 0x00001100;
            colors.keyword = 0x0066FF66;
            colors.string = 0x0000CC00;
            colors.number = 0x0033FF33;
            colors.comment = 0x00336633;
            colors.function = 0x0099FF99;
            colors.type = 0x0000FF66;
            colors.variable = 0x0000DD00;
            colors.operator_ = 0x0000FF00;
            colors.preprocessor = 0x0066FF00;
            colors.constant = 0x0033FF99;
            colors.glowColor = 0x0000FF00;
            colors.glowSecondary = 0x0033FF33;
            colors.scanlineColor = 0x00050505;
            colors.particleColor = 0x0000FF00;
            break;
            
        case MonacoThemePreset::HackerRed:
            colors.background = 0x00100000;
            colors.foreground = 0x00FF3333;
            colors.lineNumber = 0x00993333;
            colors.cursorColor = 0x00FF6600;
            colors.selectionBg = 0x00330000;
            colors.lineHighlight = 0x00200000;
            colors.keyword = 0x00FF6666;
            colors.string = 0x00FF9966;
            colors.number = 0x00FFCC00;
            colors.comment = 0x00663333;
            colors.function = 0x00FF9999;
            colors.type = 0x00FF6600;
            colors.variable = 0x00FF4444;
            colors.operator_ = 0x00FF3333;
            colors.preprocessor = 0x00FFAA00;
            colors.constant = 0x00FFDD00;
            colors.glowColor = 0x00FF0000;
            colors.glowSecondary = 0x00FF6600;
            break;
            
        case MonacoThemePreset::Monokai:
            colors.background = 0x00272822;
            colors.foreground = 0x00F8F8F2;
            colors.lineNumber = 0x0090908A;
            colors.cursorColor = 0x00F8F8F0;
            colors.selectionBg = 0x0049483E;
            colors.lineHighlight = 0x003E3D32;
            colors.keyword = 0x00F92672;       // Pink
            colors.string = 0x00E6DB74;        // Yellow
            colors.number = 0x00AE81FF;        // Purple
            colors.comment = 0x0075715E;
            colors.function = 0x00A6E22E;      // Green
            colors.type = 0x0066D9EF;          // Cyan
            colors.variable = 0x00F8F8F2;
            colors.operator_ = 0x00F92672;
            colors.preprocessor = 0x00AE81FF;
            colors.constant = 0x00AE81FF;
            break;
            
        case MonacoThemePreset::SolarizedDark:
            colors.background = 0x00002B36;
            colors.foreground = 0x00839496;
            colors.lineNumber = 0x00586E75;
            colors.cursorColor = 0x00819090;
            colors.selectionBg = 0x00073642;
            colors.lineHighlight = 0x00073642;
            colors.keyword = 0x00859900;
            colors.string = 0x002AA198;
            colors.number = 0x00D33682;
            colors.comment = 0x00586E75;
            colors.function = 0x00268BD2;
            colors.type = 0x00B58900;
            colors.variable = 0x00839496;
            colors.operator_ = 0x00859900;
            colors.preprocessor = 0x00CB4B16;
            colors.constant = 0x00D33682;
            break;
            
        case MonacoThemePreset::SolarizedLight:
            colors.background = 0x00FDF6E3;
            colors.foreground = 0x00657B83;
            colors.lineNumber = 0x0093A1A1;
            colors.cursorColor = 0x00586E75;
            colors.selectionBg = 0x00EEE8D5;
            colors.lineHighlight = 0x00EEE8D5;
            colors.keyword = 0x00859900;
            colors.string = 0x002AA198;
            colors.number = 0x00D33682;
            colors.comment = 0x0093A1A1;
            colors.function = 0x00268BD2;
            colors.type = 0x00B58900;
            colors.variable = 0x00657B83;
            colors.operator_ = 0x00859900;
            colors.preprocessor = 0x00CB4B16;
            colors.constant = 0x00D33682;
            break;
            
        case MonacoThemePreset::OneDark:
            colors.background = 0x00282C34;
            colors.foreground = 0x00ABB2BF;
            colors.lineNumber = 0x00636D83;
            colors.cursorColor = 0x00528BFF;
            colors.selectionBg = 0x003E4451;
            colors.lineHighlight = 0x002C313C;
            colors.keyword = 0x00C678DD;       // Purple
            colors.string = 0x0098C379;        // Green
            colors.number = 0x00D19A66;        // Orange
            colors.comment = 0x005C6370;
            colors.function = 0x0061AFEF;      // Blue
            colors.type = 0x00E5C07B;          // Yellow
            colors.variable = 0x00E06C75;      // Red
            colors.operator_ = 0x0056B6C2;     // Cyan
            colors.preprocessor = 0x00C678DD;
            colors.constant = 0x00D19A66;
            break;
            
        case MonacoThemePreset::Dracula:
            colors.background = 0x00282A36;
            colors.foreground = 0x00F8F8F2;
            colors.lineNumber = 0x006272A4;
            colors.cursorColor = 0x00F8F8F2;
            colors.selectionBg = 0x0044475A;
            colors.lineHighlight = 0x0044475A;
            colors.keyword = 0x00FF79C6;       // Pink
            colors.string = 0x00F1FA8C;        // Yellow
            colors.number = 0x00BD93F9;        // Purple
            colors.comment = 0x006272A4;
            colors.function = 0x0050FA7B;      // Green
            colors.type = 0x008BE9FD;          // Cyan
            colors.variable = 0x00F8F8F2;
            colors.operator_ = 0x00FF79C6;
            colors.preprocessor = 0x00BD93F9;
            colors.constant = 0x00BD93F9;
            break;
            
        case MonacoThemePreset::GruvboxDark:
            colors.background = 0x00282828;
            colors.foreground = 0x00EBDBB2;
            colors.lineNumber = 0x00928374;
            colors.cursorColor = 0x00EBDBB2;
            colors.selectionBg = 0x00504945;
            colors.lineHighlight = 0x003C3836;
            colors.keyword = 0x00FB4934;       // Red
            colors.string = 0x00B8BB26;        // Green
            colors.number = 0x00D3869B;        // Purple
            colors.comment = 0x00928374;
            colors.function = 0x00FABD2F;      // Yellow
            colors.type = 0x0083A598;          // Aqua
            colors.variable = 0x00EBDBB2;
            colors.operator_ = 0x00EBDBB2;
            colors.preprocessor = 0x00FE8019;  // Orange
            colors.constant = 0x00D3869B;
            break;
            
        case MonacoThemePreset::Nord:
            colors.background = 0x002E3440;
            colors.foreground = 0x00D8DEE9;
            colors.lineNumber = 0x004C566A;
            colors.cursorColor = 0x00D8DEE9;
            colors.selectionBg = 0x00434C5E;
            colors.lineHighlight = 0x003B4252;
            colors.keyword = 0x0081A1C1;       // Frost blue
            colors.string = 0x00A3BE8C;        // Green
            colors.number = 0x00B48EAD;        // Purple
            colors.comment = 0x00616E88;
            colors.function = 0x0088C0D0;      // Cyan
            colors.type = 0x008FBCBB;          // Teal
            colors.variable = 0x00D8DEE9;
            colors.operator_ = 0x0081A1C1;
            colors.preprocessor = 0x00D08770;  // Orange
            colors.constant = 0x00B48EAD;
            break;
            
        default: // Custom - use existing colors
            break;
    }
    
    return colors;
}

// ==============================================================================
// Monaco Settings Load/Save
// ==============================================================================

bool Settings::LoadMonaco(MonacoSettings& settings, const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        
        auto toBool = [&](const std::string& s) { return (s=="1"||s=="true"||s=="TRUE"); };
        auto toUint = [&](const std::string& s) -> uint32_t { return (uint32_t)std::stoul(s, nullptr, 0); };
        auto toInt = [&](const std::string& s) -> int { return std::stoi(s); };
        
        try {
            // Variant and theme
            if (key == "variant") settings.variant = (MonacoVariantType)toInt(val);
            else if (key == "theme_preset") settings.themePreset = (MonacoThemePreset)toInt(val);
            
            // Colors (hex format)
            else if (key == "color_background") settings.colors.background = toUint(val);
            else if (key == "color_foreground") settings.colors.foreground = toUint(val);
            else if (key == "color_line_number") settings.colors.lineNumber = toUint(val);
            else if (key == "color_cursor") settings.colors.cursorColor = toUint(val);
            else if (key == "color_selection_bg") settings.colors.selectionBg = toUint(val);
            else if (key == "color_line_highlight") settings.colors.lineHighlight = toUint(val);
            else if (key == "color_keyword") settings.colors.keyword = toUint(val);
            else if (key == "color_string") settings.colors.string = toUint(val);
            else if (key == "color_number") settings.colors.number = toUint(val);
            else if (key == "color_comment") settings.colors.comment = toUint(val);
            else if (key == "color_function") settings.colors.function = toUint(val);
            else if (key == "color_type") settings.colors.type = toUint(val);
            else if (key == "color_variable") settings.colors.variable = toUint(val);
            else if (key == "color_operator") settings.colors.operator_ = toUint(val);
            else if (key == "color_preprocessor") settings.colors.preprocessor = toUint(val);
            else if (key == "color_constant") settings.colors.constant = toUint(val);
            else if (key == "color_glow") settings.colors.glowColor = toUint(val);
            else if (key == "color_glow_secondary") settings.colors.glowSecondary = toUint(val);
            else if (key == "color_scanline") settings.colors.scanlineColor = toUint(val);
            else if (key == "color_particle") settings.colors.particleColor = toUint(val);
            
            // Font settings
            else if (key == "font_family") settings.fontFamily = val;
            else if (key == "font_size") settings.fontSize = toInt(val);
            else if (key == "line_height") settings.lineHeight = toInt(val);
            else if (key == "font_ligatures") settings.fontLigatures = toBool(val);
            else if (key == "font_weight") settings.fontWeight = toInt(val);
            
            // Editor behavior
            else if (key == "word_wrap") settings.wordWrap = toBool(val);
            else if (key == "tab_size") settings.tabSize = toInt(val);
            else if (key == "insert_spaces") settings.insertSpaces = toBool(val);
            else if (key == "auto_indent") settings.autoIndent = toBool(val);
            else if (key == "bracket_matching") settings.bracketMatching = toBool(val);
            else if (key == "auto_closing_brackets") settings.autoClosingBrackets = toBool(val);
            else if (key == "auto_closing_quotes") settings.autoClosingQuotes = toBool(val);
            else if (key == "format_on_paste") settings.formatOnPaste = toBool(val);
            else if (key == "format_on_type") settings.formatOnType = toBool(val);
            
            // IntelliSense
            else if (key == "enable_intellisense") settings.enableIntelliSense = toBool(val);
            else if (key == "quick_suggestions") settings.quickSuggestions = toBool(val);
            else if (key == "suggest_delay") settings.suggestDelay = toInt(val);
            else if (key == "parameter_hints") settings.parameterHints = toBool(val);
            
            // Visual effects
            else if (key == "enable_neon_effects") settings.enableNeonEffects = toBool(val);
            else if (key == "glow_intensity") settings.glowIntensity = toInt(val);
            else if (key == "scanline_density") settings.scanlineDensity = toInt(val);
            else if (key == "glitch_probability") settings.glitchProbability = toInt(val);
            else if (key == "particles_enabled") settings.particlesEnabled = toBool(val);
            else if (key == "particle_count") settings.particleCount = toInt(val);
            
            // ESP mode
            else if (key == "enable_esp_mode") settings.enableESPMode = toBool(val);
            else if (key == "esp_highlight_variables") settings.espHighlightVariables = toBool(val);
            else if (key == "esp_highlight_functions") settings.espHighlightFunctions = toBool(val);
            else if (key == "esp_wallhack_symbols") settings.espWallhackSymbols = toBool(val);
            
            // Minimap
            else if (key == "minimap_enabled") settings.minimapEnabled = toBool(val);
            else if (key == "minimap_render_characters") settings.minimapRenderCharacters = toBool(val);
            else if (key == "minimap_scale") settings.minimapScale = toInt(val);
            else if (key == "minimap_show_slider") settings.minimapShowSlider = toBool(val);
            
            // Debugging
            else if (key == "enable_debugging") settings.enableDebugging = toBool(val);
            else if (key == "inline_debugging") settings.inlineDebugging = toBool(val);
            else if (key == "breakpoint_gutter") settings.breakpointGutter = toBool(val);
            
            // Performance
            else if (key == "render_delay") settings.renderDelay = toInt(val);
            else if (key == "vblank_sync") settings.vblankSync = toBool(val);
            else if (key == "predictive_fetch_lines") settings.predictiveFetchLines = toInt(val);
            else if (key == "lazy_tokenization") settings.lazyTokenization = toBool(val);
            else if (key == "lazy_tokenization_delay") settings.lazyTokenizationDelay = toInt(val);
            
        } catch (...) { /* ignore malformed values */ }
    }
    
    // If using a preset, load its colors
    if (settings.themePreset != MonacoThemePreset::Custom) {
        settings.colors = GetThemePresetColors(settings.themePreset);
    }
    
    settings.dirty = false;
    return true;
}

bool Settings::SaveMonaco(const MonacoSettings& settings, const std::string& path) {
    EnsureSettingsDir(path);
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    
    ofs << "# RawrXD Monaco Editor Settings\n";
    ofs << "# Theme presets: 0=Default, 1=NeonCyberpunk, 2=MatrixGreen, 3=HackerRed,\n";
    ofs << "#                4=Monokai, 5=SolarizedDark, 6=SolarizedLight, 7=OneDark,\n";
    ofs << "#                8=Dracula, 9=GruvboxDark, 10=Nord, 255=Custom\n";
    ofs << "# Variants: 0=Core, 1=NeonCore, 2=NeonHack, 3=ZeroDependency, 4=Enterprise\n\n";
    
    // Variant and theme
    ofs << "[Monaco]\n";
    ofs << "variant=" << (int)settings.variant << "\n";
    ofs << "theme_preset=" << (int)settings.themePreset << "\n\n";
    
    // Colors (only save if Custom theme)
    ofs << "[Colors]\n";
    ofs << "color_background=0x" << std::hex << settings.colors.background << "\n";
    ofs << "color_foreground=0x" << std::hex << settings.colors.foreground << "\n";
    ofs << "color_line_number=0x" << std::hex << settings.colors.lineNumber << "\n";
    ofs << "color_cursor=0x" << std::hex << settings.colors.cursorColor << "\n";
    ofs << "color_selection_bg=0x" << std::hex << settings.colors.selectionBg << "\n";
    ofs << "color_line_highlight=0x" << std::hex << settings.colors.lineHighlight << "\n";
    ofs << "color_keyword=0x" << std::hex << settings.colors.keyword << "\n";
    ofs << "color_string=0x" << std::hex << settings.colors.string << "\n";
    ofs << "color_number=0x" << std::hex << settings.colors.number << "\n";
    ofs << "color_comment=0x" << std::hex << settings.colors.comment << "\n";
    ofs << "color_function=0x" << std::hex << settings.colors.function << "\n";
    ofs << "color_type=0x" << std::hex << settings.colors.type << "\n";
    ofs << "color_variable=0x" << std::hex << settings.colors.variable << "\n";
    ofs << "color_operator=0x" << std::hex << settings.colors.operator_ << "\n";
    ofs << "color_preprocessor=0x" << std::hex << settings.colors.preprocessor << "\n";
    ofs << "color_constant=0x" << std::hex << settings.colors.constant << "\n";
    ofs << "color_glow=0x" << std::hex << settings.colors.glowColor << "\n";
    ofs << "color_glow_secondary=0x" << std::hex << settings.colors.glowSecondary << "\n";
    ofs << "color_scanline=0x" << std::hex << settings.colors.scanlineColor << "\n";
    ofs << "color_particle=0x" << std::hex << settings.colors.particleColor << "\n\n";
    
    // Font settings
    ofs << std::dec; // Reset to decimal
    ofs << "[Font]\n";
    ofs << "font_family=" << settings.fontFamily << "\n";
    ofs << "font_size=" << settings.fontSize << "\n";
    ofs << "line_height=" << settings.lineHeight << "\n";
    ofs << "font_ligatures=" << (settings.fontLigatures ? "1" : "0") << "\n";
    ofs << "font_weight=" << settings.fontWeight << "\n\n";
    
    // Editor behavior
    ofs << "[Editor]\n";
    ofs << "word_wrap=" << (settings.wordWrap ? "1" : "0") << "\n";
    ofs << "tab_size=" << settings.tabSize << "\n";
    ofs << "insert_spaces=" << (settings.insertSpaces ? "1" : "0") << "\n";
    ofs << "auto_indent=" << (settings.autoIndent ? "1" : "0") << "\n";
    ofs << "bracket_matching=" << (settings.bracketMatching ? "1" : "0") << "\n";
    ofs << "auto_closing_brackets=" << (settings.autoClosingBrackets ? "1" : "0") << "\n";
    ofs << "auto_closing_quotes=" << (settings.autoClosingQuotes ? "1" : "0") << "\n";
    ofs << "format_on_paste=" << (settings.formatOnPaste ? "1" : "0") << "\n";
    ofs << "format_on_type=" << (settings.formatOnType ? "1" : "0") << "\n\n";
    
    // IntelliSense
    ofs << "[IntelliSense]\n";
    ofs << "enable_intellisense=" << (settings.enableIntelliSense ? "1" : "0") << "\n";
    ofs << "quick_suggestions=" << (settings.quickSuggestions ? "1" : "0") << "\n";
    ofs << "suggest_delay=" << settings.suggestDelay << "\n";
    ofs << "parameter_hints=" << (settings.parameterHints ? "1" : "0") << "\n\n";
    
    // Visual effects
    ofs << "[NeonEffects]\n";
    ofs << "enable_neon_effects=" << (settings.enableNeonEffects ? "1" : "0") << "\n";
    ofs << "glow_intensity=" << settings.glowIntensity << "\n";
    ofs << "scanline_density=" << settings.scanlineDensity << "\n";
    ofs << "glitch_probability=" << settings.glitchProbability << "\n";
    ofs << "particles_enabled=" << (settings.particlesEnabled ? "1" : "0") << "\n";
    ofs << "particle_count=" << settings.particleCount << "\n\n";
    
    // ESP mode
    ofs << "[ESPMode]\n";
    ofs << "enable_esp_mode=" << (settings.enableESPMode ? "1" : "0") << "\n";
    ofs << "esp_highlight_variables=" << (settings.espHighlightVariables ? "1" : "0") << "\n";
    ofs << "esp_highlight_functions=" << (settings.espHighlightFunctions ? "1" : "0") << "\n";
    ofs << "esp_wallhack_symbols=" << (settings.espWallhackSymbols ? "1" : "0") << "\n\n";
    
    // Minimap
    ofs << "[Minimap]\n";
    ofs << "minimap_enabled=" << (settings.minimapEnabled ? "1" : "0") << "\n";
    ofs << "minimap_render_characters=" << (settings.minimapRenderCharacters ? "1" : "0") << "\n";
    ofs << "minimap_scale=" << settings.minimapScale << "\n";
    ofs << "minimap_show_slider=" << (settings.minimapShowSlider ? "1" : "0") << "\n\n";
    
    // Debugging
    ofs << "[Debugging]\n";
    ofs << "enable_debugging=" << (settings.enableDebugging ? "1" : "0") << "\n";
    ofs << "inline_debugging=" << (settings.inlineDebugging ? "1" : "0") << "\n";
    ofs << "breakpoint_gutter=" << (settings.breakpointGutter ? "1" : "0") << "\n\n";
    
    // Performance
    ofs << "[Performance]\n";
    ofs << "render_delay=" << settings.renderDelay << "\n";
    ofs << "vblank_sync=" << (settings.vblankSync ? "1" : "0") << "\n";
    ofs << "predictive_fetch_lines=" << settings.predictiveFetchLines << "\n";
    ofs << "lazy_tokenization=" << (settings.lazyTokenization ? "1" : "0") << "\n";
    ofs << "lazy_tokenization_delay=" << settings.lazyTokenizationDelay << "\n";
    
    return true;
}

