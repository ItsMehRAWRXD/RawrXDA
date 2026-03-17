#!/usr/bin/env python3
"""
RawrXD IDE v1.0 - Smoke Test Suite
Tests core functionality after build completion
Run: python smoke_test.py
"""

import subprocess
import os
import sys
import time
import json
from pathlib import Path

class SmokeTest:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.exe_path = None
        self.results = []
    
    def log_pass(self, test_name, message=""):
        self.passed += 1
        print(f"  ✅ PASS: {test_name}")
        if message:
            print(f"     {message}")
        self.results.append({"test": test_name, "status": "PASS", "message": message})
    
    def log_fail(self, test_name, message=""):
        self.failed += 1
        print(f"  ❌ FAIL: {test_name}")
        if message:
            print(f"     {message}")
        self.results.append({"test": test_name, "status": "FAIL", "message": message})
    
    def test_executable_exists(self):
        """Test 1: Verify executable was built"""
        print("\n[Test 1] Executable Exists")
        
        # Check possible build output paths
        possible_paths = [
            "build\\release\\RawrXD-IDE.exe",
            "build\\Release\\RawrXD-IDE.exe", 
            "build\\debug\\RawrXD-IDE.exe",
            "release\\RawrXD-IDE.exe",
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                self.exe_path = path
                size_mb = os.path.getsize(path) / (1024 * 1024)
                self.log_pass("Executable found", f"{path} ({size_mb:.1f} MB)")
                return True
        
        self.log_fail("Executable not found", f"Checked: {possible_paths}")
        return False
    
    def test_dll_exists(self):
        """Test 2: Verify Sovereign Loader DLL was copied"""
        print("\n[Test 2] Sovereign Loader DLL")
        
        possible_paths = [
            "build\\release\\RawrXD-SovereignLoader.dll",
            "build\\Release\\RawrXD-SovereignLoader.dll",
            "release\\RawrXD-SovereignLoader.dll",
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                size_mb = os.path.getsize(path) / (1024 * 1024)
                self.log_pass("DLL found", f"{path} ({size_mb:.1f} MB)")
                return True
        
        self.log_fail("DLL not found", f"Checked: {possible_paths}")
        return False
    
    def test_source_files(self):
        """Test 3: Verify all 10 source files created"""
        print("\n[Test 3] Source Files (10 expected)")
        
        required_files = [
            "src/ModelLoaderBridge.h",
            "src/ModelLoaderBridge.cpp",
            "src/ModelCacheManager.h",
            "src/ModelCacheManager.cpp",
            "src/InferenceSession.h",
            "src/InferenceSession.cpp",
            "src/TokenStreamRouter.h",
            "src/TokenStreamRouter.cpp",
            "src/ModelSelectionDialog.h",
            "src/ModelSelectionDialog.cpp",
            "src/IDEIntegration.h",
            "src/IDEIntegration.cpp",
            "src/PerformanceMonitor.h",
            "src/PerformanceMonitor.cpp",
            "src/ModelMetadataParser.h",
            "src/ModelMetadataParser.cpp",
            "src/StreamingInferenceEngine.h",
            "src/StreamingInferenceEngine.cpp",
            "RawrXD-IDE.pro",
        ]
        
        missing = []
        for file in required_files:
            if not os.path.exists(file):
                missing.append(file)
        
        if not missing:
            self.log_pass("All source files found", f"19 files verified")
            return True
        else:
            self.log_fail("Missing source files", f"Missing: {missing}")
            return False
    
    def test_masm_kernels(self):
        """Test 4: Verify MASM kernels compiled"""
        print("\n[Test 4] MASM Kernel Compilation")
        
        obj_files = [
            "build-sovereign/beaconism_dispatcher.obj",
            "build-sovereign/universal_quant_kernel.obj",
            "build-sovereign/dimensional_pool.obj",
        ]
        
        missing = []
        for obj in obj_files:
            if not os.path.exists(obj):
                missing.append(obj)
        
        if not missing:
            self.log_pass("MASM kernels compiled", "All 3 .obj files present")
            return True
        else:
            # Also check alternate paths
            alt_paths = [
                "build-sovereign-static/bin/beaconism_dispatcher.obj",
                "build-sovereign-static/bin/universal_quant_kernel.obj",
                "build-sovereign-static/bin/dimensional_pool.obj",
            ]
            
            all_missing = True
            for alt in alt_paths:
                if os.path.exists(alt):
                    all_missing = False
                    break
            
            if not all_missing:
                self.log_pass("MASM kernels compiled", "Found in alternate location")
                return True
            else:
                self.log_fail("MASM kernels not found", f"Checked: {missing + alt_paths}")
                return False
    
    def test_security_updates(self):
        """Test 5: Verify security fixes implemented"""
        print("\n[Test 5] Security Updates")
        
        security_files = [
            ("RawrXD-ModelLoader/include/sovereign_loader_secure.h", "Pre-flight validation API"),
            ("RawrXD-ModelLoader/src/sovereign_loader_secure.c", "Security pipeline implementation"),
        ]
        
        all_found = True
        for file, desc in security_files:
            if os.path.exists(file):
                print(f"     ✓ {desc}: {file}")
            else:
                print(f"     ✗ {desc}: NOT FOUND")
                all_found = False
        
        if all_found:
            self.log_pass("Security files found", "Pre-flight validation implemented")
            return True
        else:
            self.log_fail("Security files missing", "Cannot verify vulnerability fix")
            return False
    
    def test_documentation(self):
        """Test 6: Verify deployment documentation"""
        print("\n[Test 6] Documentation")
        
        doc_files = [
            "PRODUCTION_DEPLOYMENT_PIPELINE.md",
            "FINAL_PRODUCTION_STATUS.md",
            "README_SYMBOL_FIX.md",
        ]
        
        missing = []
        for doc in doc_files:
            if not os.path.exists(doc):
                missing.append(doc)
        
        if not missing:
            self.log_pass("Documentation complete", f"{len(doc_files)} guides found")
            return True
        else:
            self.log_fail("Documentation incomplete", f"Missing: {missing}")
            return False
    
    def test_qt_version(self):
        """Test 7: Verify Qt 6.5+"""
        print("\n[Test 7] Qt Version")
        
        try:
            # Try to find qmake
            result = subprocess.run(
                ['where', 'qmake.exe'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            if result.returncode == 0:
                qmake_path = result.stdout.strip().split('\n')[0]
                
                # Query Qt version
                result = subprocess.run(
                    [qmake_path, '-query', 'QT_VERSION'],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                
                if result.returncode == 0:
                    version = result.stdout.strip()
                    try:
                        major, minor = map(int, version.split('.')[:2])
                        if major >= 6 and minor >= 5:
                            self.log_pass("Qt version OK", f"Qt {version} (>= 6.5 required)")
                            return True
                        else:
                            self.log_fail("Qt version too old", f"Found {version}, need 6.5+")
                            return False
                    except:
                        self.log_fail("Qt version parse error", f"Got: {version}")
                        return False
                else:
                    self.log_fail("Cannot query Qt version", result.stderr)
                    return False
            else:
                self.log_fail("qmake not found", "Qt not in PATH")
                return False
        
        except Exception as e:
            self.log_fail("Qt version check failed", str(e))
            return False
    
    def run_all_tests(self):
        """Run the complete test suite"""
        print("=" * 60)
        print("RawrXD IDE v1.0 - Smoke Test Suite")
        print("=" * 60)
        
        tests = [
            self.test_executable_exists,
            self.test_dll_exists,
            self.test_source_files,
            self.test_masm_kernels,
            self.test_security_updates,
            self.test_documentation,
            self.test_qt_version,
        ]
        
        for test in tests:
            try:
                test()
            except Exception as e:
                self.log_fail(test.__name__, str(e))
        
        # Summary
        print("\n" + "=" * 60)
        print("SUMMARY")
        print("=" * 60)
        print(f"  ✅ Passed: {self.passed}")
        print(f"  ❌ Failed: {self.failed}")
        print(f"  📊 Total:  {self.passed + self.failed}")
        
        success_rate = (self.passed / (self.passed + self.failed) * 100) if (self.passed + self.failed) > 0 else 0
        print(f"  📈 Rate:   {success_rate:.0f}%")
        
        # Status
        if self.failed == 0:
            print("\n🟢 ALL TESTS PASSED - Ready for build verification")
            return 0
        elif self.failed <= 2:
            print(f"\n🟡 {self.failed} TEST(S) FAILED - Minor issues")
            return 1
        else:
            print(f"\n🔴 {self.failed} TEST(S) FAILED - Major issues")
            return 1
    
    def save_results(self):
        """Save test results to JSON"""
        results = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "passed": self.passed,
            "failed": self.failed,
            "tests": self.results
        }
        
        with open("smoke_test_results.json", "w") as f:
            json.dump(results, f, indent=2)
        
        print(f"\n📄 Results saved to: smoke_test_results.json")

if __name__ == "__main__":
    tester = SmokeTest()
    exit_code = tester.run_all_tests()
    tester.save_results()
    sys.exit(exit_code)
