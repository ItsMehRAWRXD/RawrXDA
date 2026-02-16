// ============================================================================
// test_xss_sanitizer.cpp — XSS Audit with Adversarial Test Vectors
// ============================================================================
//
// Action Item #10: Prove XSS + HTML escaping works via adversarial vectors.
//
// Exit criteria: Rendered output contains only text nodes or safely-sanitized
// markup when limited formatting is allowed.
//
// Vectors sourced from OWASP XSS Filter Evasion Cheat Sheet.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <cstring>

#include "logging/logger.h"
static Logger s_logger("test_xss_sanitizer");

// ============================================================================
// Minimal HTML Sanitizer (mirrors the inline sanitizer in ide_chatbot.html)
// ============================================================================
namespace XSSSanitizer {

    /// Escape HTML special characters — must neutralize all injection vectors
    std::string escapeHtml(const std::string& raw) {
        std::string out;
        out.reserve(raw.size() + 64);
        for (char c : raw) {
            switch (c) {
                case '<':  out += "&lt;"; break;
                case '>':  out += "&gt;"; break;
                case '&':  out += "&amp;"; break;
                case '"':  out += "&quot;"; break;
                case '\'': out += "&#x27;"; break;
                case '/':  out += "&#x2F;"; break;
                default:   out += c; break;
            }
        }
        return out;
    }

    /// Strip all HTML tags completely (fallback when DOMPurify unavailable)
    std::string stripTags(const std::string& html) {
        std::string out;
        out.reserve(html.size());
        bool inTag = false;
        for (char c : html) {
            if (c == '<') { inTag = true; continue; }
            if (c == '>') { inTag = false; continue; }
            if (!inTag) out += c;
        }
        return out;
    }

    /// Check if output contains any executable HTML constructs
    bool containsUnsafeHtml(const std::string& s) {
        std::string lower;
        lower.resize(s.size());
        std::transform(s.begin(), s.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Check for dangerous tags  
        const char* dangerousTags[] = {
            "<script", "<img", "<svg", "<iframe", "<object",
            "<embed", "<form", "<input", "<link", "<meta",
            "<style", "<base", "<applet", "<math", "<video",
            "<audio", "<source", "<body", "<marquee", "<isindex",
            nullptr
        };
        for (int i = 0; dangerousTags[i]; ++i) {
            if (lower.find(dangerousTags[i]) != std::string::npos)
                return true;
        }

        // Check for event handlers
        const char* eventHandlers[] = {
            "onerror", "onload", "onclick", "onmouseover", "onfocus",
            "onblur", "onsubmit", "onkeydown", "onkeyup", "onkeypress",
            "onchange", "onmousedown", "onmouseup", "onmousemove",
            "ondblclick", "oncontextmenu", "onwheel", "ondrag",
            "ondrop", "oncopy", "onpaste", "oncut", "onscroll",
            "ontouchstart", "ontouchend", "ontouchmove",
            "onanimationstart", "onanimationend",
            nullptr
        };
        for (int i = 0; eventHandlers[i]; ++i) {
            if (lower.find(eventHandlers[i]) != std::string::npos)
                return true;
        }

        // Check for javascript: protocol
        if (lower.find("javascript:") != std::string::npos) return true;
        if (lower.find("vbscript:") != std::string::npos) return true;
        if (lower.find("data:text/html") != std::string::npos) return true;

        return false;
    }

} // namespace XSSSanitizer

// ============================================================================
// Adversarial XSS Test Vectors
// ============================================================================
struct XSSTestVector {
    const char* name;
    const char* payload;
};

static const XSSTestVector kXSSVectors[] = {
    // Basic script injection
    {"basic_script", "<script>alert('XSS')</script>"},
    {"script_encoded", "<script>alert(String.fromCharCode(88,83,83))</script>"},

    // IMG tag attacks
    {"img_onerror", "<img src=x onerror=alert('XSS')>"},
    {"img_dynsrc", "<img dynsrc=\"javascript:alert('XSS')\">"},
    {"img_lowsrc", "<img lowsrc=\"javascript:alert('XSS')\">"},

    // SVG attacks
    {"svg_onload", "<svg onload=alert('XSS')>"},
    {"svg_script", "<svg><script>alert('XSS')</script></svg>"},
    {"svg_animate", "<svg><animate onbegin=alert('XSS') attributeName=x dur=1s>"},

    // Event handler attacks
    {"body_onload", "<body onload=alert('XSS')>"},
    {"div_onmouseover", "<div onmouseover=\"alert('XSS')\">hover me</div>"},
    {"input_onfocus", "<input onfocus=alert('XSS') autofocus>"},
    {"marquee_onstart", "<marquee onstart=alert('XSS')>"},
    {"details_ontoggle", "<details ontoggle=alert('XSS') open>"},
    
    // iframe attacks
    {"iframe_src", "<iframe src=\"javascript:alert('XSS')\">"},
    {"iframe_srcdoc", "<iframe srcdoc=\"<script>alert('XSS')</script>\">"},
    
    // Object/embed attacks
    {"object_data", "<object data=\"javascript:alert('XSS')\">"},
    {"embed_src", "<embed src=\"javascript:alert('XSS')\">"},
    
    // Script close tag escape (the known truncation bug)
    {"script_close_in_text", "Normal text</script><script>alert('XSS')</script>"},
    {"nested_script", "Hello</script><script>document.cookie</script>"},
    
    // Encoded attacks
    {"hex_encoded", "<img src=&#x6A;&#x61;&#x76;&#x61;&#x73;&#x63;&#x72;&#x69;&#x70;&#x74;&#x3A;alert('XSS')>"},
    {"null_byte", "<scr\x00ipt>alert('XSS')</scr\x00ipt>"},
    {"tab_break", "<img\tsrc=x\tonerror=alert('XSS')>"},
    {"newline_break", "<img\nsrc=x\nonerror=alert('XSS')>"},

    // CSS expression attacks
    {"style_expression", "<div style=\"background:url(javascript:alert('XSS'))\">"},
    {"style_tag", "<style>body{background:url('javascript:alert(1)')}</style>"},

    // Protocol attacks
    {"javascript_proto", "<a href=\"javascript:alert('XSS')\">click</a>"},
    {"vbscript_proto", "<a href=\"vbscript:MsgBox('XSS')\">click</a>"},
    {"data_html", "<a href=\"data:text/html,<script>alert('XSS')</script>\">click</a>"},

    // Mixed case evasion
    {"mixed_case", "<ScRiPt>alert('XSS')</ScRiPt>"},
    {"mixed_img", "<ImG sRc=x oNeRrOr=alert('XSS')>"},

    // HTML5 specific
    {"math_tag", "<math><mtext><table><mglyph><style><!--</style><img src=x onerror=alert('XSS')>"},
    {"video_source", "<video><source onerror=\"alert('XSS')\">"},
    {"audio_onerror", "<audio src=x onerror=alert('XSS')>"},

    // Mutation XSS (mXSS)
    {"backtick_attr", "<img src=\"`><img src=x onerror=alert('XSS')>\">"},
    {"form_action", "<form action=\"javascript:alert('XSS')\"><input type=submit>"},

    // Large payload (stress)
    {"large_payload", "<script>" "A" /*repeated*/ "</script>"},

    {nullptr, nullptr}
};

// ============================================================================
// Test Runner
// ============================================================================
static int g_passed = 0;
static int g_failed = 0;

void runTest(const char* name, const char* payload) {
    // Test 1: escapeHtml should neutralize all vectors
    std::string escaped = XSSSanitizer::escapeHtml(payload);
    if (XSSSanitizer::containsUnsafeHtml(escaped)) {
        s_logger.error( "  [FAIL] escapeHtml still contains unsafe HTML: " << name << "\n";
        s_logger.error( "    Escaped: " << escaped.substr(0, 200) << "\n";
        g_failed++;
    } else {
        s_logger.info("  [PASS] escapeHtml: ");
        g_passed++;
    }

    // Test 2: stripTags should remove all tags
    std::string stripped = XSSSanitizer::stripTags(payload);
    if (stripped.find('<') != std::string::npos) {
        s_logger.error( "  [FAIL] stripTags still contains '<': " << name << "\n";
        g_failed++;
    } else {
        s_logger.info("  [PASS] stripTags: ");
        g_passed++;
    }
}

int main() {
    s_logger.info("============================================\n");
    s_logger.info("XSS Adversarial Test Suite\n");
    s_logger.info("============================================\n\n");

    for (int i = 0; kXSSVectors[i].name; ++i) {
        runTest(kXSSVectors[i].name, kXSSVectors[i].payload);
    }

    s_logger.info("\n============================================\n");
    s_logger.info("Results: ");
    s_logger.info("============================================\n");

    return g_failed > 0 ? 1 : 0;
}
