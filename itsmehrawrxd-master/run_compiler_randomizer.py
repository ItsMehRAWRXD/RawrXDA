#!/usr/bin/env python3
"""
Run Compiler Generator Randomizer
Executes the randomizer and tests generated compilers
"""

import os
import sys
import time
import subprocess
import json
from pathlib import Path

def run_basic_randomizer():
    """Run the basic compiler randomizer"""
    print("🎲 Running Basic Compiler Randomizer")
    print("=" * 50)
    
    try:
        result = subprocess.run([sys.executable, 'compiler_generator_randomizer.py'], 
                              capture_output=True, text=True, timeout=300)
        
        if result.returncode == 0:
            print("✅ Basic randomizer completed successfully")
            print(result.stdout)
        else:
            print("❌ Basic randomizer failed")
            print(result.stderr)
        
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print("⏰ Basic randomizer timed out")
        return False
    except Exception as e:
        print(f"❌ Error running basic randomizer: {e}")
        return False

def run_enhanced_randomizer():
    """Run the enhanced compiler randomizer"""
    print("🧬 Running Enhanced Compiler Randomizer")
    print("=" * 50)
    
    try:
        result = subprocess.run([sys.executable, 'enhanced_compiler_randomizer.py'], 
                              capture_output=True, text=True, timeout=300)
        
        if result.returncode == 0:
            print("✅ Enhanced randomizer completed successfully")
            print(result.stdout)
        else:
            print("❌ Enhanced randomizer failed")
            print(result.stderr)
        
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print("⏰ Enhanced randomizer timed out")
        return False
    except Exception as e:
        print(f"❌ Error running enhanced randomizer: {e}")
        return False

def test_generated_compilers():
    """Test all generated compilers"""
    print("🧪 Testing Generated Compilers")
    print("=" * 50)
    
    # Find all generated compiler files
    compiler_files = []
    for ext in ['py', 'js', 'cpp', 'asm']:
        files = list(Path('.').glob(f'*compiler*.{ext}'))
        compiler_files.extend(files)
    
    print(f"Found {len(compiler_files)} compiler files to test")
    
    working_compilers = []
    failed_compilers = []
    
    for compiler_file in compiler_files:
        print(f"\n🧪 Testing {compiler_file}")
        
        try:
            if compiler_file.suffix == '.py':
                # Test Python compiler
                result = subprocess.run([sys.executable, str(compiler_file)], 
                                      capture_output=True, text=True, timeout=30)
                if result.returncode == 0:
                    print(f"✅ {compiler_file} - SUCCESS")
                    working_compilers.append(str(compiler_file))
                else:
                    print(f"❌ {compiler_file} - FAILED")
                    failed_compilers.append(str(compiler_file))
            
            elif compiler_file.suffix == '.js':
                # Test JavaScript compiler
                result = subprocess.run(['node', str(compiler_file)], 
                                      capture_output=True, text=True, timeout=30)
                if result.returncode == 0:
                    print(f"✅ {compiler_file} - SUCCESS")
                    working_compilers.append(str(compiler_file))
                else:
                    print(f"❌ {compiler_file} - FAILED")
                    failed_compilers.append(str(compiler_file))
            
            elif compiler_file.suffix == '.cpp':
                # Test C++ compiler
                # First compile
                compile_result = subprocess.run(['g++', '-o', f'{compiler_file.stem}', str(compiler_file)], 
                                              capture_output=True, text=True, timeout=30)
                if compile_result.returncode == 0:
                    # Then run
                    run_result = subprocess.run([f'./{compiler_file.stem}'], 
                                              capture_output=True, text=True, timeout=30)
                    if run_result.returncode == 0:
                        print(f"✅ {compiler_file} - SUCCESS")
                        working_compilers.append(str(compiler_file))
                    else:
                        print(f"❌ {compiler_file} - FAILED (runtime error)")
                        failed_compilers.append(str(compiler_file))
                else:
                    print(f"❌ {compiler_file} - FAILED (compilation error)")
                    failed_compilers.append(str(compiler_file))
            
            elif compiler_file.suffix == '.asm':
                # Test Assembly compiler
                # First assemble
                assemble_result = subprocess.run(['nasm', '-f', 'elf64', '-o', f'{compiler_file.stem}.o', str(compiler_file)], 
                                               capture_output=True, text=True, timeout=30)
                if assemble_result.returncode == 0:
                    # Then link
                    link_result = subprocess.run(['ld', '-o', f'{compiler_file.stem}', f'{compiler_file.stem}.o'], 
                                              capture_output=True, text=True, timeout=30)
                    if link_result.returncode == 0:
                        # Then run
                        run_result = subprocess.run([f'./{compiler_file.stem}'], 
                                                  capture_output=True, text=True, timeout=30)
                        if run_result.returncode == 0:
                            print(f"✅ {compiler_file} - SUCCESS")
                            working_compilers.append(str(compiler_file))
                        else:
                            print(f"❌ {compiler_file} - FAILED (runtime error)")
                            failed_compilers.append(str(compiler_file))
                    else:
                        print(f"❌ {compiler_file} - FAILED (linking error)")
                        failed_compilers.append(str(compiler_file))
                else:
                    print(f"❌ {compiler_file} - FAILED (assembly error)")
                    failed_compilers.append(str(compiler_file))
        
        except subprocess.TimeoutExpired:
            print(f"⏰ {compiler_file} - TIMEOUT")
            failed_compilers.append(str(compiler_file))
        except Exception as e:
            print(f"❌ {compiler_file} - ERROR: {e}")
            failed_compilers.append(str(compiler_file))
    
    return working_compilers, failed_compilers

def generate_summary_report(working_compilers, failed_compilers):
    """Generate a summary report"""
    print("\n📊 COMPILER GENERATOR RANDOMIZER SUMMARY")
    print("=" * 60)
    
    total_compilers = len(working_compilers) + len(failed_compilers)
    success_rate = len(working_compilers) / total_compilers if total_compilers > 0 else 0
    
    print(f"📈 Total compilers generated: {total_compilers}")
    print(f"✅ Working compilers: {len(working_compilers)}")
    print(f"❌ Failed compilers: {len(failed_compilers)}")
    print(f"📊 Success rate: {success_rate:.2%}")
    
    if working_compilers:
        print("\n🎉 WORKING COMPILERS:")
        for compiler in working_compilers:
            print(f"  ✅ {compiler}")
    
    if failed_compilers:
        print("\n💥 FAILED COMPILERS:")
        for compiler in failed_compilers:
            print(f"  ❌ {compiler}")
    
    # Save report
    report = {
        'total_compilers': total_compilers,
        'working_compilers': working_compilers,
        'failed_compilers': failed_compilers,
        'success_rate': success_rate,
        'timestamp': time.time()
    }
    
    with open('compiler_randomizer_report.json', 'w') as f:
        json.dump(report, f, indent=2)
    
    print(f"\n💾 Report saved to: compiler_randomizer_report.json")

def main():
    """Main function"""
    print("🎲 Compiler Generator Randomizer Test Runner")
    print("=" * 60)
    print("This will run both randomizers and test all generated compilers")
    print("")
    
    # Run basic randomizer
    print("Phase 1: Running Basic Randomizer")
    basic_success = run_basic_randomizer()
    print("")
    
    # Run enhanced randomizer
    print("Phase 2: Running Enhanced Randomizer")
    enhanced_success = run_enhanced_randomizer()
    print("")
    
    # Test all generated compilers
    print("Phase 3: Testing All Generated Compilers")
    working_compilers, failed_compilers = test_generated_compilers()
    print("")
    
    # Generate summary report
    generate_summary_report(working_compilers, failed_compilers)
    
    print("\n🎲 Compiler Generator Randomizer Test Runner Complete!")
    
    if working_compilers:
        print(f"🎉 SUCCESS! Found {len(working_compilers)} working compilers!")
    else:
        print("😞 No working compilers found")

if __name__ == "__main__":
    main()
