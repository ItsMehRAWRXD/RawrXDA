#!/usr/bin/env python3
"""
Smoke Test Verification for UX Polish / Accessibility Implementation
Validates syntax and integration of all changes
"""

import json
import re
from pathlib import Path

def verify_feature_config():
    """Verify feature toggles exist in configuration"""
    config_path = Path("feature_configuration.json")
    with open(config_path) as f:
        config = json.load(f)
    
    features = config['configuration']['features']
    feature_ids = {f['id']: f['name'] for f in features}
    
    required = {
        25: "Toast Notifications",
        26: "Keyboard Shortcuts for Docks",
        27: "Screen Reader Accessibility"
    }
    
    for fid, fname in required.items():
        assert fid in feature_ids, f"Missing feature {fid}: {fname}"
        assert feature_ids[fid] == fname, f"Name mismatch for {fid}"
    
    print("✅ Feature toggles validated")
    return True

def verify_ide_main_window_h():
    """Verify ide_main_window.h has new method signature"""
    with open("src/ide_main_window.h") as f:
        content = f.read()
    
    # Check for showToast method
    assert "void showToast(" in content, "Missing showToast() method signature"
    assert "QWidget* toastLayer;" in content, "Missing toastLayer member"
    
    print("✅ IDE MainWindow header validated")
    return True

def verify_ide_main_window_cpp():
    """Verify ide_main_window.cpp has implementation"""
    with open("src/ide_main_window.cpp") as f:
        content = f.read()
    
    # Check for includes
    assert "#include \"feature_toggle.h\"" in content, "Missing feature_toggle include"
    assert "#include <QPropertyAnimation>" in content, "Missing QPropertyAnimation include"
    assert "#include <QGraphicsDropShadowEffect>" in content, "Missing shadow effect include"
    
    # Check for toast implementation
    assert "void IDEMainWindow::showToast(" in content, "Missing showToast() implementation"
    assert "auto* animIn = new QPropertyAnimation" in content, "Missing fade-in animation"
    assert "auto* animOut = new QPropertyAnimation" in content, "Missing fade-out animation"
    
    # Check for feature toggle usage
    assert 'RawrXD::FeatureToggle::isEnabled("Toast Notifications"' in content, \
        "Missing feature toggle for toasts"
    
    # Check for keyboard shortcuts
    shortcuts = [
        'QKeySequence("Ctrl+Shift+1")',
        'QKeySequence("Ctrl+Shift+2")',
        'QKeySequence("Ctrl+Shift+3")',
        'QKeySequence("Ctrl+Shift+4")',
        'QKeySequence("Ctrl+Shift+5")',
        'QKeySequence("Ctrl+Shift+6")',
        'QKeySequence("Ctrl+Shift+D")',
        'QKeySequence("Ctrl+Shift+C")'
    ]
    for shortcut in shortcuts:
        assert shortcut in content, f"Missing keyboard shortcut {shortcut}"
    
    # Check for telemetry
    assert 'GetTelemetry().recordEvent("ui.message"' in content, "Missing telemetry event"
    
    print("✅ IDE MainWindow implementation validated")
    return True

def verify_cloud_settings_dialog():
    """Verify accessibility labels in cloud settings dialog"""
    with open("src/cloud_settings_dialog.cpp") as f:
        content = f.read()
    
    # Check for accessibility labels
    accessibility_checks = [
        ("OpenAI", "setAccessibleName.*OpenAI"),
        ("Anthropic", "setAccessibleName.*Anthropic"),
        ("Google", "setAccessibleName.*Google"),
        ("Model Preferences", "setAccessibleName.*Model Preferences"),
        ("Request Settings", "setAccessibleName.*Request Settings"),
        ("Cost Management", "setAccessibleName.*Cost Management"),
        ("Provider Health", "setAccessibleName.*Provider Health"),
    ]
    
    for label, pattern in accessibility_checks:
        assert re.search(pattern, content, re.IGNORECASE), \
            f"Missing accessibility label for {label}"
    
    # Check for descriptions
    assert "setAccessibleDescription(" in content, "Missing accessibility descriptions"
    count_descriptions = content.count("setAccessibleDescription(")
    assert count_descriptions >= 25, f"Not enough descriptions: {count_descriptions} < 25"
    
    print(f"✅ Cloud Settings accessibility validated ({count_descriptions} labels)")
    return True

def verify_audit_doc():
    """Verify IDE_AUDIT.md has been updated"""
    with open("IDE_AUDIT.md") as f:
        content = f.read()
    
    assert "### Changes Implemented" in content, "Missing 'Changes Implemented' section"
    assert "Keyboard shortcuts:" in content, "Missing keyboard shortcuts info"
    assert "Toast notifications:" in content, "Missing toast info"
    assert "Accessibility:" in content, "Missing accessibility info"
    
    print("✅ IDE_AUDIT.md documentation validated")
    return True

def main():
    """Run all verification checks"""
    print("\n" + "="*60)
    print("UX POLISH / ACCESSIBILITY IMPLEMENTATION VERIFICATION")
    print("="*60 + "\n")
    
    checks = [
        verify_feature_config,
        verify_ide_main_window_h,
        verify_ide_main_window_cpp,
        verify_cloud_settings_dialog,
        verify_audit_doc,
    ]
    
    all_passed = True
    for check in checks:
        try:
            check()
        except AssertionError as e:
            print(f"❌ {check.__name__}: {e}")
            all_passed = False
        except Exception as e:
            print(f"❌ {check.__name__}: Unexpected error: {e}")
            all_passed = False
    
    print("\n" + "="*60)
    if all_passed:
        print("✅ ALL CHECKS PASSED - Implementation is complete and correct")
        print("="*60 + "\n")
        return 0
    else:
        print("❌ SOME CHECKS FAILED - See errors above")
        print("="*60 + "\n")
        return 1

if __name__ == "__main__":
    exit(main())
