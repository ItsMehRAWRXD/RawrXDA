#!/usr/bin/env python3
"""
Meta-Prompting AST/IR Integration System
Main integration point for auto-generating ASTs and IR mappings
Connects all components: meta-prompting, language registry, AST generation, and IR mapping
"""

import os
import json
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

# Import all our meta-prompting components
from meta_prompting_engine import MetaPromptingEngine
from universal_language_registry import UniversalLanguageRegistry
from universal_ast_generator import UniversalASTGenerator
from universal_ir_mapper import UniversalIRMapper

class MetaASTIRSystem:
    """
    Comprehensive meta-prompting system for auto-generating ASTs and IR mappings
    for every programming language the IDE supports
    """
    
    def __init__(self):
        print("🚀 Initializing Meta-Prompting AST/IR System...")
        
        # Initialize core components
        self.language_registry = UniversalLanguageRegistry()
        self.ast_generator = UniversalASTGenerator(self.language_registry)
        self.ir_mapper = UniversalIRMapper(self.language_registry)
        
        # Generated artifacts
        self.generated_asts = {}
        self.generated_irs = {}
        self.generated_parsers = {}
        
        print("✅ Meta-Prompting AST/IR System initialized")
    
    def generate_complete_language_support(self, language_name: str, sample_code: str = None) -> Dict[str, Any]:
        """Generate complete language support: AST spec, IR mappings, and parser"""
        
        print(f"🔄 Generating complete support for {language_name.upper()}...")
        
        try:
            # Step 1: Generate AST specification
            print(f"  🌳 Generating AST for {language_name}...")
            ast_spec = self.language_registry.generate_ast_for_language(language_name, sample_code)
            
            # Step 2: Generate IR mappings
            print(f"  🔄 Generating IR mappings for {language_name}...")
            ir_mappings = self.language_registry.generate_ir_for_language(language_name, ast_spec)
            
            # Step 3: Generate parser
            print(f"  📝 Generating parser for {language_name}...")
            parser_code = self.language_registry.generate_parser_for_language(language_name)
            
            # Store results
            result = {
                'language': language_name,
                'ast_spec': ast_spec,
                'ir_mappings': ir_mappings,
                'parser_code': parser_code,
                'status': 'success'
            }
            
            self.generated_asts[language_name] = ast_spec
            self.generated_irs[language_name] = ir_mappings
            self.generated_parsers[language_name] = parser_code
            
            print(f"✅ Complete support generated for {language_name.upper()}")
            return result
            
        except Exception as e:
            error_result = {
                'language': language_name,
                'status': 'error',
                'error': str(e)
            }
            print(f"❌ Error generating support for {language_name.upper()}: {e}")
            return error_result
    
    def generate_all_language_support(self) -> Dict[str, Any]:
        """Generate complete support for all registered languages"""
        
        print("🌍 Generating support for ALL languages...")
        
        results = {}
        languages = self.language_registry.get_supported_languages()
        
        print(f"📋 Processing {len(languages)} languages...")
        
        for i, language in enumerate(languages, 1):
            print(f"\n[{i}/{len(languages)}] Processing {language.upper()}...")
            
            result = self.generate_complete_language_support(language)
            results[language] = result
            
            # Show progress
            if result['status'] == 'success':
                print(f"  ✅ {language.upper()} completed successfully")
            else:
                print(f"  ❌ {language.upper()} failed: {result.get('error', 'Unknown error')}")
        
        print(f"\n🎉 Batch processing complete! Generated support for {len(results)} languages")
        return results
    
    def compile_source_to_ir(self, source_code: str, language_name: str) -> List[Any]:
        """Complete pipeline: source code → AST → IR"""
        
        print(f"⚙️ Compiling {language_name.upper()} source to IR...")
        
        try:
            # Step 1: Generate AST from source code
            print(f"  🌳 Parsing {language_name} source to AST...")
            ast = self.ast_generator.generate_ast(source_code, language_name)
            
            # Step 2: Convert AST to IR
            print(f"  🔄 Converting AST to IR...")
            ir_instructions = self.ir_mapper.map_ast_to_ir(ast, language_name)
            
            # Step 3: Optimize IR
            print(f"  ⚡ Optimizing IR...")
            optimized_ir = self.ir_mapper.optimize_ir(ir_instructions)
            
            print(f"✅ Successfully compiled {language_name.upper()} to IR ({len(optimized_ir)} instructions)")
            return optimized_ir
            
        except Exception as e:
            print(f"❌ Error compiling {language_name.upper()}: {e}")
            raise
    
    def cross_language_compilation(self, source_files: List[Tuple[str, str, str]]) -> List[Any]:
        """
        Cross-language compilation: multiple sources → unified IR
        source_files: List of (source_code, language, filename) tuples
        """
        
        print("🌐 Starting cross-language compilation...")
        
        all_asts = []
        
        for source_code, language, filename in source_files:
            print(f"  📁 Processing {filename} ({language.upper()})...")
            
            try:
                # Generate AST for this source file
                ast = self.ast_generator.generate_ast(source_code, language)
                all_asts.append((ast, language))
                
                print(f"    ✅ {filename} parsed successfully")
                
            except Exception as e:
                print(f"    ❌ Error parsing {filename}: {e}")
                continue
        
        # Generate unified IR from all ASTs
        print(f"  🔄 Generating unified IR from {len(all_asts)} sources...")
        unified_ir = self.ir_mapper.generate_cross_language_ir(all_asts)
        
        # Optimize unified IR
        print(f"  ⚡ Optimizing unified IR...")
        optimized_ir = self.ir_mapper.optimize_ir(unified_ir)
        
        print(f"✅ Cross-language compilation complete ({len(optimized_ir)} IR instructions)")
        return optimized_ir
    
    def analyze_language_coverage(self) -> Dict[str, Any]:
        """Analyze what languages are supported and their feature coverage"""
        
        coverage = {
            'total_languages': 0,
            'languages_with_ast': 0,
            'languages_with_ir': 0,
            'languages_with_parser': 0,
            'language_details': {},
            'missing_features': []
        }
        
        for language in self.language_registry.get_supported_languages():
            coverage['total_languages'] += 1
            
            details = {
                'has_ast': language in self.generated_asts,
                'has_ir': language in self.generated_irs,
                'has_parser': language in self.generated_parsers,
                'language_spec': bool(self.language_registry.get_language_spec(language))
            }
            
            if details['has_ast']:
                coverage['languages_with_ast'] += 1
            if details['has_ir']:
                coverage['languages_with_ir'] += 1
            if details['has_parser']:
                coverage['languages_with_parser'] += 1
            
            coverage['language_details'][language] = details
            
            # Check for missing features
            if not all(details.values()):
                missing = [k for k, v in details.items() if not v]
                coverage['missing_features'].append({
                    'language': language,
                    'missing': missing
                })
        
        return coverage
    
    def export_all_artifacts(self, output_dir: str = "generated_language_support"):
        """Export all generated ASTs, IR mappings, and parsers"""
        
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        print(f"💾 Exporting all artifacts to {output_dir}...")
        
        # Export AST specifications
        ast_dir = output_path / "ast_specs"
        ast_dir.mkdir(exist_ok=True)
        
        for language, ast_spec in self.generated_asts.items():
            ast_file = ast_dir / f"{language}_ast_spec.json"
            with open(ast_file, 'w') as f:
                # Convert AST specs to serializable format
                serializable_spec = {}
                for node_type, node_spec in ast_spec.items():
                    serializable_spec[node_type] = {
                        'node_type': node_spec.node_type,
                        'properties': node_spec.properties,
                        'children': node_spec.children,
                        'semantic_rules': node_spec.semantic_rules,
                        'ir_mapping': node_spec.ir_mapping
                    }
                json.dump(serializable_spec, f, indent=2)
        
        # Export IR mappings
        ir_dir = output_path / "ir_mappings"
        ir_dir.mkdir(exist_ok=True)
        self.ir_mapper.save_ir_mappings(str(ir_dir))
        
        # Export parsers
        parser_dir = output_path / "parsers"
        parser_dir.mkdir(exist_ok=True)
        
        for language, parser_code in self.generated_parsers.items():
            parser_file = parser_dir / f"{language}_parser.py"
            with open(parser_file, 'w') as f:
                f.write(parser_code)
        
        # Export language registry
        registry_file = output_path / "language_registry.json"
        self.language_registry.meta_engine.save_generated_specs(str(output_path))
        
        # Export coverage analysis
        coverage = self.analyze_language_coverage()
        coverage_file = output_path / "language_coverage.json"
        with open(coverage_file, 'w') as f:
            json.dump(coverage, f, indent=2)
        
        print(f"✅ All artifacts exported to {output_dir}")
        
        # Print summary
        print(f"\n📊 Export Summary:")
        print(f"  • AST specs: {len(self.generated_asts)} languages")
        print(f"  • IR mappings: {len(self.generated_irs)} languages")
        print(f"  • Parsers: {len(self.generated_parsers)} languages")
        print(f"  • Total coverage: {coverage['total_languages']} languages")
    
    def demo_system(self):
        """Demonstrate the system with sample code from multiple languages"""
        
        print("🎬 Running Meta-Prompting System Demo...")
        
        # Sample code in different languages
        samples = {
            'python': '''
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

result = fibonacci(10)
print(result)
''',
            'javascript': '''
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

const result = factorial(5);
console.log(result);
''',
            'rust': '''
fn main() {
    let numbers = vec![1, 2, 3, 4, 5];
    let sum: i32 = numbers.iter().sum();
    println!("Sum: {}", sum);
}
''',
            'c': '''
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main() {
    int result = add(5, 3);
    printf("Result: %d\\n", result);
    return 0;
}
'''
        }
        
        demo_results = {}
        
        for language, code in samples.items():
            print(f"\n🔄 Demo: {language.upper()}")
            print(f"📝 Source code:")
            print(f"```{language}")
            print(code[:100] + "..." if len(code) > 100 else code)
            print("```")
            
            try:
                # Generate complete language support
                support = self.generate_complete_language_support(language, code)
                
                if support['status'] == 'success':
                    # Compile to IR
                    ir = self.compile_source_to_ir(code, language)
                    
                    demo_results[language] = {
                        'ast_nodes': len(support['ast_spec']),
                        'ir_instructions': len(ir),
                        'status': 'success'
                    }
                    
                    print(f"  ✅ Generated {len(support['ast_spec'])} AST nodes")
                    print(f"  ✅ Generated {len(ir)} IR instructions")
                else:
                    demo_results[language] = {'status': 'failed', 'error': support.get('error')}
                    print(f"  ❌ Failed: {support.get('error')}")
                    
            except Exception as e:
                demo_results[language] = {'status': 'failed', 'error': str(e)}
                print(f"  ❌ Error: {e}")
        
        print(f"\n🎉 Demo Complete!")
        print(f"📊 Demo Results:")
        for lang, result in demo_results.items():
            status = "✅" if result['status'] == 'success' else "❌"
            print(f"  {status} {lang.upper()}: {result}")
        
        return demo_results

def integrate_with_existing_ide(ide_instance):
    """
    Integrate the complete meta-prompting system with existing IDE
    This is the main integration point that should be called from the IDE
    """
    
    print("🔧 Integrating Meta-Prompting AST/IR System with IDE...")
    
    # Create the main system
    meta_system = MetaASTIRSystem()
    
    # Attach to IDE instance
    ide_instance.meta_ast_ir_system = meta_system
    ide_instance.language_registry = meta_system.language_registry
    ide_instance.ast_generator = meta_system.ast_generator
    ide_instance.ir_mapper = meta_system.ir_mapper
    
    # Add convenience methods to IDE
    ide_instance.generate_language_support = meta_system.generate_complete_language_support
    ide_instance.compile_to_ir = meta_system.compile_source_to_ir
    ide_instance.cross_compile = meta_system.cross_language_compilation
    ide_instance.analyze_language_coverage = meta_system.analyze_language_coverage
    ide_instance.export_language_support = meta_system.export_all_artifacts
    
    print("✅ Meta-Prompting AST/IR System integrated with IDE")
    
    # Generate support for all languages in the background
    print("🚀 Auto-generating support for all languages...")
    try:
        results = meta_system.generate_all_language_support()
        success_count = sum(1 for r in results.values() if r['status'] == 'success')
        print(f"🎉 Auto-generated support for {success_count}/{len(results)} languages")
    except Exception as e:
        print(f"⚠️ Background generation encountered errors: {e}")
    
    return meta_system

if __name__ == "__main__":
    print("🚀 Meta-Prompting AST/IR Integration System")
    print("=" * 60)
    
    # Create and demo the system
    system = MetaASTIRSystem()
    
    # Run demo
    demo_results = system.demo_system()
    
    # Export artifacts
    system.export_all_artifacts()
    
    # Show coverage analysis
    coverage = system.analyze_language_coverage()
    print(f"\n📊 Language Coverage Analysis:")
    print(f"  • Total languages: {coverage['total_languages']}")
    print(f"  • With AST support: {coverage['languages_with_ast']}")
    print(f"  • With IR support: {coverage['languages_with_ir']}")
    print(f"  • With parser support: {coverage['languages_with_parser']}")
    
    if coverage['missing_features']:
        print(f"  • Languages needing attention: {len(coverage['missing_features'])}")
    
    print("\n✅ Meta-Prompting AST/IR System ready for integration!")
