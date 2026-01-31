#!/usr/bin/env python3
"""
ASM to All Languages Transpiler
Complete transpilation system: ASM -> Java -> All Languages
"""

import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Any
import tempfile
import shutil

# Import our transpilation components
from asm_to_java_compiler import ASMToJavaCompiler
from plugins.java_components import JavaLexer, JavaParser
from plugins.java_to_cplusplus_codegen import JavaToCppCodeGenerator
from plugins.java_to_python_codegen import JavaToPythonCodeGenerator
from plugins.java_to_rust_codegen import JavaToRustCodeGenerator

class ASMToAllLanguagesTranspiler:
    """Complete ASM to all languages transpilation system"""
    
    def __init__(self):
        self.asm_compiler = ASMToJavaCompiler()
        self.java_lexer = JavaLexer()
        self.java_parser = JavaParser()
        
        # Language generators
        self.generators = {
            'cpp': JavaToCppCodeGenerator(),
            'python': JavaToPythonCodeGenerator(),
            'rust': JavaToRustCodeGenerator(),
        }
        
        self.temp_dir = None
    
    def transpile_asm_to_language(self, asm_source: str, target_language: str, 
                                 output_file: str = None) -> Dict[str, Any]:
        """Transpile ASM source to target language"""
        print(f"🔨 Transpiling ASM to {target_language.upper()}...")
        
        try:
            # Step 1: ASM to Java
            print("📝 Step 1: Converting ASM to Java...")
            java_result = self.asm_compiler.compile_asm_to_java(asm_source, "TranspiledClass")
            
            if not java_result["success"]:
                return {
                    "success": False,
                    "error": f"ASM to Java failed: {java_result['error']}",
                    "java_source": None,
                    "target_source": None
                }
            
            java_source = java_result["java_source"]
            print("✅ Java source generated")
            
            # Step 2: Java to AST
            print("🌳 Step 2: Parsing Java to AST...")
            java_tokens = self.java_lexer.tokenize(java_source)
            java_ast = self.java_parser.parse(java_tokens)
            print("✅ Java AST generated")
            
            # Step 3: AST to target language
            print(f"🎯 Step 3: Generating {target_language.upper()} code...")
            if target_language not in self.generators:
                return {
                    "success": False,
                    "error": f"Unsupported target language: {target_language}",
                    "java_source": java_source,
                    "target_source": None
                }
            
            generator = self.generators[target_language]
            target_source = generator.generate(java_ast, None)
            print(f"✅ {target_language.upper()} source generated")
            
            # Step 4: Save files
            if output_file:
                self._save_output_files(java_source, target_source, target_language, output_file)
            
            return {
                "success": True,
                "java_source": java_source,
                "target_source": target_source,
                "target_language": target_language,
                "java_ast": java_ast
            }
            
        except Exception as e:
            return {
                "success": False,
                "error": f"Transpilation error: {str(e)}",
                "java_source": None,
                "target_source": None
            }
    
    def transpile_asm_to_all_languages(self, asm_source: str, output_dir: str = "transpiled_output") -> Dict[str, Any]:
        """Transpile ASM source to all supported languages"""
        print("🚀 Transpiling ASM to ALL languages...")
        
        results = {}
        os.makedirs(output_dir, exist_ok=True)
        
        for language in self.generators.keys():
            print(f"\n🔨 Transpiling to {language.upper()}...")
            result = self.transpile_asm_to_language(
                asm_source, 
                language, 
                os.path.join(output_dir, f"transpiled.{language}")
            )
            results[language] = result
            
            if result["success"]:
                print(f"✅ {language.upper()} transpilation successful!")
            else:
                print(f"❌ {language.upper()} transpilation failed: {result['error']}")
        
        return {
            "success": True,
            "results": results,
            "output_dir": output_dir
        }
    
    def _save_output_files(self, java_source: str, target_source: str, 
                          target_language: str, output_file: str):
        """Save output files"""
        # Save Java source
        java_file = output_file.replace(f".{target_language}", ".java")
        with open(java_file, 'w', encoding='utf-8') as f:
            f.write(java_source)
        print(f"💾 Java source saved: {java_file}")
        
        # Save target language source
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(target_source)
        print(f"💾 {target_language.upper()} source saved: {output_file}")
    
    def get_supported_languages(self) -> List[str]:
        """Get list of supported target languages"""
        return list(self.generators.keys())
    
    def get_transpilation_info(self) -> Dict[str, Any]:
        """Get information about the transpilation system"""
        return {
            "asm_to_java": "✅ Available",
            "java_lexer": "✅ Available", 
            "java_parser": "✅ Available",
            "target_languages": self.get_supported_languages(),
            "generators": {
                lang: f"✅ {gen.__class__.__name__}" 
                for lang, gen in self.generators.items()
            }
        }

def test_asm_to_all_languages_transpiler():
    """Test the complete transpilation system"""
    print("🧪 Testing ASM to All Languages Transpiler...")
    
    transpiler = ASMToAllLanguagesTranspiler()
    info = transpiler.get_transpilation_info()
    
    print(f"📋 Transpilation System Info:")
    for key, value in info.items():
        if isinstance(value, dict):
            print(f"   {key}:")
            for k, v in value.items():
                print(f"     {k}: {v}")
        else:
            print(f"   {key}: {value}")
    
    # Test ASM source
    asm_source = """
; Simple ASM program for transpilation
mov eax, 42
mov ebx, 8
add eax, ebx
mov ecx, eax
mul ecx, 2
ret
"""
    
    print(f"\n🔨 Testing transpilation to all languages...")
    result = transpiler.transpile_asm_to_all_languages(asm_source, "test_transpilation")
    
    if result["success"]:
        print("✅ Complete transpilation successful!")
        print(f"📁 Output directory: {result['output_dir']}")
        
        for lang, lang_result in result["results"].items():
            if lang_result["success"]:
                print(f"   ✅ {lang.upper()}: Generated successfully")
            else:
                print(f"   ❌ {lang.upper()}: {lang_result['error']}")
        
        return True
    else:
        print("❌ Complete transpilation failed!")
        return False

if __name__ == "__main__":
    test_asm_to_all_languages_transpiler()
