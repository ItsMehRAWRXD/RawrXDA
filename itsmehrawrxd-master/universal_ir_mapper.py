#!/usr/bin/env python3
"""
Universal IR Mapper
Creates cross-language compatible Intermediate Representation mappings
Enables compilation from any source language to any target architecture
"""

import json
from typing import Dict, List, Any, Optional, Union, Tuple
from dataclasses import dataclass, asdict
from enum import Enum
from pathlib import Path

class IROpcode(Enum):
    """Universal IR opcodes"""
    # Memory operations
    LOAD = "load"
    STORE = "store"
    ALLOC = "alloc"
    DEALLOC = "dealloc"
    
    # Arithmetic operations
    ADD = "add"
    SUB = "sub"
    MUL = "mul"
    DIV = "div"
    MOD = "mod"
    NEG = "neg"
    
    # Bitwise operations
    AND = "and"
    OR = "or"
    XOR = "xor"
    NOT = "not"
    SHL = "shl"
    SHR = "shr"
    
    # Comparison operations
    EQ = "eq"
    NE = "ne"
    LT = "lt"
    LE = "le"
    GT = "gt"
    GE = "ge"
    
    # Control flow
    JUMP = "jump"
    BRANCH = "branch"
    CALL = "call"
    RETURN = "return"
    
    # Type operations
    CAST = "cast"
    TYPEOF = "typeof"
    
    # High-level constructs
    FUNCTION = "function"
    LABEL = "label"
    COMMENT = "comment"

@dataclass
class IRInstruction:
    """Universal IR instruction"""
    opcode: IROpcode
    operands: List[Any]
    metadata: Dict[str, Any]
    result: Optional[str] = None
    source_mapping: Optional[Tuple[int, int]] = None
    
    def __post_init__(self):
        if self.metadata is None:
            self.metadata = {}

@dataclass
class IRType:
    """IR type information"""
    name: str
    size: int  # Size in bytes
    alignment: int
    is_signed: bool = False
    is_pointer: bool = False
    element_type: Optional['IRType'] = None  # For arrays/pointers

class UniversalIRMapper:
    """Universal IR mapping system for cross-language compatibility"""
    
    def __init__(self, language_registry):
        self.language_registry = language_registry
        self.ir_mappings = {}
        self.type_mappings = {}
        self.builtin_types = self._init_builtin_types()
        
        # Initialize mappings for all supported languages
        self._initialize_all_mappings()
        
        print("🔄 Universal IR Mapper initialized")
    
    def _init_builtin_types(self) -> Dict[str, IRType]:
        """Initialize builtin IR types"""
        
        return {
            'void': IRType('void', 0, 1),
            'bool': IRType('bool', 1, 1),
            'i8': IRType('i8', 1, 1, is_signed=True),
            'u8': IRType('u8', 1, 1, is_signed=False),
            'i16': IRType('i16', 2, 2, is_signed=True),
            'u16': IRType('u16', 2, 2, is_signed=False),
            'i32': IRType('i32', 4, 4, is_signed=True),
            'u32': IRType('u32', 4, 4, is_signed=False),
            'i64': IRType('i64', 8, 8, is_signed=True),
            'u64': IRType('u64', 8, 8, is_signed=False),
            'f32': IRType('f32', 4, 4),
            'f64': IRType('f64', 8, 8),
            'ptr': IRType('ptr', 8, 8, is_pointer=True),
            'string': IRType('string', 8, 8, is_pointer=True),
        }
    
    def _initialize_all_mappings(self):
        """Initialize IR mappings for all supported languages"""
        
        for language_name in self.language_registry.get_supported_languages():
            self._generate_ir_mapping(language_name)
    
    def _generate_ir_mapping(self, language_name: str):
        """Generate IR mapping for specific language"""
        
        language_spec = self.language_registry.get_language_spec(language_name)
        if not language_spec:
            return
        
        # Generate AST to IR mappings based on language characteristics
        mapping = {}
        
        # Common mappings for all languages
        mapping.update(self._get_common_ir_mappings())
        
        # Language-specific mappings
        if language_name == 'python':
            mapping.update(self._get_python_ir_mappings())
        elif language_name == 'javascript':
            mapping.update(self._get_javascript_ir_mappings())
        elif language_name in ['c', 'cpp']:
            mapping.update(self._get_c_cpp_ir_mappings())
        elif language_name == 'rust':
            mapping.update(self._get_rust_ir_mappings())
        elif language_name == 'java':
            mapping.update(self._get_java_ir_mappings())
        elif language_name == 'go':
            mapping.update(self._get_go_ir_mappings())
        elif language_name == 'solidity':
            mapping.update(self._get_solidity_ir_mappings())
        else:
            # Use generic mappings
            mapping.update(self._get_generic_ir_mappings())
        
        self.ir_mappings[language_name] = mapping
    
    def _get_common_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get common IR mappings for all languages"""
        
        return {
            'Literal': [
                IRInstruction(IROpcode.LOAD, ['%result', '{value}'], 
                            {'description': 'Load literal value'})
            ],
            'Identifier': [
                IRInstruction(IROpcode.LOAD, ['%result', '{name}'], 
                            {'description': 'Load variable value'})
            ],
            'BinaryExpression': [
                IRInstruction(IROpcode.COMMENT, ['Binary operation: {operator}'], {}),
                # Operands will be generated dynamically based on operator
            ],
            'UnaryExpression': [
                IRInstruction(IROpcode.COMMENT, ['Unary operation: {operator}'], {}),
            ],
            'Assignment': [
                IRInstruction(IROpcode.STORE, ['{target}', '{value}'], 
                            {'description': 'Store value to variable'})
            ],
            'Block': [
                IRInstruction(IROpcode.COMMENT, ['Begin block'], {}),
                IRInstruction(IROpcode.COMMENT, ['End block'], {})
            ],
            'Return': [
                IRInstruction(IROpcode.RETURN, ['{value}'], 
                            {'description': 'Return from function'})
            ]
        }
    
    def _get_python_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get Python-specific IR mappings"""
        
        return {
            'FunctionDef': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{args}'], 
                            {'language': 'python', 'dynamic_typing': True})
            ],
            'ClassDef': [
                IRInstruction(IROpcode.COMMENT, ['Class definition: {name}'], {}),
                IRInstruction(IROpcode.ALLOC, ['%class_obj', 'class_size'], 
                            {'type': 'object', 'python_class': True})
            ],
            'ListComp': [
                IRInstruction(IROpcode.COMMENT, ['List comprehension'], {}),
                IRInstruction(IROpcode.ALLOC, ['%list', 'list_size'], {'type': 'list'})
            ],
            'Lambda': [
                IRInstruction(IROpcode.COMMENT, ['Lambda expression'], {}),
                IRInstruction(IROpcode.FUNCTION, ['%lambda', '{args}'], 
                            {'anonymous': True, 'closure': True})
            ],
            'ImportFrom': [
                IRInstruction(IROpcode.CALL, ['__import__', ['{module}']], 
                            {'python_import': True})
            ]
        }
    
    def _get_javascript_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get JavaScript-specific IR mappings"""
        
        return {
            'FunctionExpression': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'language': 'javascript', 'hoisted': True})
            ],
            'ArrowFunctionExpression': [
                IRInstruction(IROpcode.FUNCTION, ['%arrow', '{params}'], 
                            {'arrow_function': True, 'lexical_this': True})
            ],
            'ObjectExpression': [
                IRInstruction(IROpcode.ALLOC, ['%obj', 'object_size'], {'type': 'object'}),
                # Property assignments would follow
            ],
            'CallExpression': [
                IRInstruction(IROpcode.CALL, ['{callee}', '{arguments}'], 
                            {'javascript_call': True, 'this_binding': True})
            ],
            'AwaitExpression': [
                IRInstruction(IROpcode.COMMENT, ['Await expression'], {}),
                IRInstruction(IROpcode.CALL, ['await_runtime', ['{argument}']], 
                            {'async': True})
            ],
            'TemplateLiteral': [
                IRInstruction(IROpcode.COMMENT, ['Template literal'], {}),
                IRInstruction(IROpcode.CALL, ['template_string', ['{expressions}']], {})
            ]
        }
    
    def _get_c_cpp_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get C/C++-specific IR mappings"""
        
        return {
            'FunctionDeclaration': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'language': 'c', 'calling_convention': 'cdecl'})
            ],
            'StructDeclaration': [
                IRInstruction(IROpcode.COMMENT, ['Struct definition: {name}'], {}),
                # Struct layout calculations would follow
            ],
            'PointerDeclaration': [
                IRInstruction(IROpcode.ALLOC, ['%ptr', 'pointer_size'], 
                            {'type': 'pointer', 'pointee_type': '{type}'})
            ],
            'ArrayDeclaration': [
                IRInstruction(IROpcode.ALLOC, ['%array', '{size} * {element_size}'], 
                            {'type': 'array', 'element_type': '{type}'})
            ],
            'Dereference': [
                IRInstruction(IROpcode.LOAD, ['%result', '[{pointer}]'], 
                            {'dereference': True})
            ],
            'AddressOf': [
                IRInstruction(IROpcode.COMMENT, ['Address of operator'], {}),
                # Address calculation would follow
            ],
            'SizeofExpression': [
                IRInstruction(IROpcode.LOAD, ['%result', 'sizeof({type})'], 
                            {'compile_time': True})
            ]
        }
    
    def _get_rust_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get Rust-specific IR mappings"""
        
        return {
            'FnDef': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'language': 'rust', 'memory_safe': True})
            ],
            'StructDef': [
                IRInstruction(IROpcode.COMMENT, ['Struct definition: {name}'], {}),
                # Ownership and borrowing checks would be here
            ],
            'BorrowExpression': [
                IRInstruction(IROpcode.COMMENT, ['Borrow: {mutability}'], {}),
                IRInstruction(IROpcode.LOAD, ['%borrow', '&{value}'], 
                            {'borrow_checker': True, 'lifetime': '{lifetime}'})
            ],
            'MatchExpression': [
                IRInstruction(IROpcode.COMMENT, ['Pattern match'], {}),
                IRInstruction(IROpcode.BRANCH, ['match_discriminant', '{arms}'], 
                            {'exhaustive': True})
            ],
            'OwnershipTransfer': [
                IRInstruction(IROpcode.COMMENT, ['Ownership transfer'], {}),
                # Move semantics would be implemented here
            ],
            'LifetimeAnnotation': [
                IRInstruction(IROpcode.COMMENT, ['Lifetime: {lifetime}'], 
                            {'compile_time': True})
            ]
        }
    
    def _get_java_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get Java-specific IR mappings"""
        
        return {
            'MethodDeclaration': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'language': 'java', 'virtual': True})
            ],
            'ClassDeclaration': [
                IRInstruction(IROpcode.COMMENT, ['Class: {name}'], {}),
                # Virtual method table setup would follow
            ],
            'InterfaceDeclaration': [
                IRInstruction(IROpcode.COMMENT, ['Interface: {name}'], {}),
                # Interface method declarations
            ],
            'NewExpression': [
                IRInstruction(IROpcode.ALLOC, ['%obj', 'object_size'], 
                            {'garbage_collected': True, 'class': '{type}'})
            ],
            'CastExpression': [
                IRInstruction(IROpcode.CAST, ['%result', '{expression}', '{type}'], 
                            {'runtime_check': True})
            ],
            'InstanceofExpression': [
                IRInstruction(IROpcode.CALL, ['instanceof', ['{object}', '{type}']], 
                            {'runtime_check': True})
            ]
        }
    
    def _get_go_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get Go-specific IR mappings"""
        
        return {
            'FuncDecl': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'language': 'go', 'goroutine_safe': True})
            ],
            'GoStatement': [
                IRInstruction(IROpcode.COMMENT, ['Start goroutine'], {}),
                IRInstruction(IROpcode.CALL, ['go_runtime_start', ['{function}']], 
                            {'concurrent': True})
            ],
            'ChannelSend': [
                IRInstruction(IROpcode.CALL, ['chan_send', ['{channel}', '{value}']], 
                            {'blocking': True})
            ],
            'ChannelReceive': [
                IRInstruction(IROpcode.CALL, ['chan_recv', ['{channel}']], 
                            {'blocking': True})
            ],
            'SelectStatement': [
                IRInstruction(IROpcode.CALL, ['select_runtime', ['{cases}']], 
                            {'non_blocking': True})
            ],
            'InterfaceAssertion': [
                IRInstruction(IROpcode.CAST, ['%result', '{expression}', '{type}'], 
                            {'runtime_check': True, 'go_interface': True})
            ]
        }
    
    def _get_solidity_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get Solidity-specific IR mappings for blockchain"""
        
        return {
            'ContractDefinition': [
                IRInstruction(IROpcode.COMMENT, ['Contract: {name}'], {}),
                # Contract bytecode generation would follow
            ],
            'FunctionDefinition': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'language': 'solidity', 'gas_metered': True})
            ],
            'ModifierInvocation': [
                IRInstruction(IROpcode.COMMENT, ['Modifier: {name}'], {}),
                # Modifier execution logic
            ],
            'EventDefinition': [
                IRInstruction(IROpcode.COMMENT, ['Event: {name}'], {}),
                # Event logging setup
            ],
            'StateVariableDeclaration': [
                IRInstruction(IROpcode.ALLOC, ['storage_slot', 'variable_size'], 
                            {'storage': True, 'persistent': True})
            ],
            'RequireStatement': [
                IRInstruction(IROpcode.BRANCH, ['condition', 'revert_label'], 
                            {'assertion': True, 'gas_refund': True})
            ],
            'EmitStatement': [
                IRInstruction(IROpcode.CALL, ['emit_event', ['{event}', '{args}']], 
                            {'blockchain_log': True})
            ]
        }
    
    def _get_generic_ir_mappings(self) -> Dict[str, List[IRInstruction]]:
        """Get generic IR mappings for unsupported languages"""
        
        return {
            'GenericFunction': [
                IRInstruction(IROpcode.FUNCTION, ['{name}', '{params}'], 
                            {'generic': True})
            ],
            'GenericCall': [
                IRInstruction(IROpcode.CALL, ['{function}', '{args}'], 
                            {'generic': True})
            ]
        }
    
    def map_ast_to_ir(self, ast_node, language_name: str) -> List[IRInstruction]:
        """Map AST node to IR instructions"""
        
        language_name = language_name.lower()
        
        if language_name not in self.ir_mappings:
            raise ValueError(f"No IR mappings for language: {language_name}")
        
        mappings = self.ir_mappings[language_name]
        node_type = ast_node.node_type
        
        if node_type not in mappings:
            # Try to find a generic mapping
            generic_mappings = ['GenericFunction', 'GenericCall']
            for generic in generic_mappings:
                if generic in mappings:
                    return self._instantiate_ir_template(mappings[generic], ast_node)
            
            # If no mapping found, create a comment
            return [IRInstruction(IROpcode.COMMENT, [f'Unmapped AST node: {node_type}'], {})]
        
        # Get the IR template and instantiate it
        ir_template = mappings[node_type]
        return self._instantiate_ir_template(ir_template, ast_node)
    
    def _instantiate_ir_template(self, template: List[IRInstruction], ast_node) -> List[IRInstruction]:
        """Instantiate IR template with actual values from AST node"""
        
        result = []
        
        for instr_template in template:
            # Create a copy of the instruction
            instr = IRInstruction(
                opcode=instr_template.opcode,
                operands=[],
                metadata=instr_template.metadata.copy(),
                result=instr_template.result,
                source_mapping=instr_template.source_mapping
            )
            
            # Substitute placeholders in operands
            for operand in instr_template.operands:
                if isinstance(operand, str):
                    instr.operands.append(self._substitute_placeholders(operand, ast_node))
                else:
                    instr.operands.append(operand)
            
            # Substitute placeholders in result
            if instr.result:
                instr.result = self._substitute_placeholders(instr.result, ast_node)
            
            result.append(instr)
        
        return result
    
    def _substitute_placeholders(self, template: str, ast_node) -> str:
        """Substitute placeholders in template with actual values"""
        
        result = template
        
        # Basic substitutions
        if '{name}' in result and hasattr(ast_node, 'value') and ast_node.value:
            result = result.replace('{name}', str(ast_node.value))
        
        if '{value}' in result and hasattr(ast_node, 'value') and ast_node.value:
            result = result.replace('{value}', str(ast_node.value))
        
        if '{operator}' in result and hasattr(ast_node, 'value') and ast_node.value:
            result = result.replace('{operator}', str(ast_node.value))
        
        # More complex substitutions would be implemented here
        # For parameters, arguments, types, etc.
        
        return result
    
    def generate_cross_language_ir(self, ast_list: List[Tuple[Any, str]]) -> List[IRInstruction]:
        """Generate unified IR from multiple language ASTs"""
        
        unified_ir = []
        
        # Add header comment
        unified_ir.append(IRInstruction(IROpcode.COMMENT, 
                                      ['Cross-language compilation unit'], 
                                      {'cross_language': True}))
        
        for ast, language in ast_list:
            # Add language marker
            unified_ir.append(IRInstruction(IROpcode.COMMENT, 
                                          [f'Begin {language} section'], 
                                          {'language_boundary': True}))
            
            # Convert AST to IR
            if hasattr(ast, 'children'):
                for child in ast.children:
                    ir_instructions = self.map_ast_to_ir(child, language)
                    unified_ir.extend(ir_instructions)
            else:
                ir_instructions = self.map_ast_to_ir(ast, language)
                unified_ir.extend(ir_instructions)
            
            # Add end marker
            unified_ir.append(IRInstruction(IROpcode.COMMENT, 
                                          [f'End {language} section'], 
                                          {'language_boundary': True}))
        
        return unified_ir
    
    def optimize_ir(self, ir_instructions: List[IRInstruction]) -> List[IRInstruction]:
        """Optimize IR instructions"""
        
        optimized = ir_instructions.copy()
        
        # Dead code elimination
        optimized = self._eliminate_dead_code(optimized)
        
        # Constant folding
        optimized = self._fold_constants(optimized)
        
        # Common subexpression elimination
        optimized = self._eliminate_common_subexpressions(optimized)
        
        return optimized
    
    def _eliminate_dead_code(self, instructions: List[IRInstruction]) -> List[IRInstruction]:
        """Remove dead code"""
        # Simplified implementation
        return [instr for instr in instructions if instr.opcode != IROpcode.COMMENT or 
                'dead_code' not in instr.metadata]
    
    def _fold_constants(self, instructions: List[IRInstruction]) -> List[IRInstruction]:
        """Fold constant expressions"""
        # Simplified implementation
        return instructions
    
    def _eliminate_common_subexpressions(self, instructions: List[IRInstruction]) -> List[IRInstruction]:
        """Eliminate common subexpressions"""
        # Simplified implementation
        return instructions
    
    def ir_to_json(self, ir_instructions: List[IRInstruction]) -> str:
        """Convert IR to JSON representation"""
        
        def instr_to_dict(instr):
            return {
                'opcode': instr.opcode.value,
                'operands': instr.operands,
                'metadata': instr.metadata,
                'result': instr.result,
                'source_mapping': instr.source_mapping
            }
        
        return json.dumps([instr_to_dict(instr) for instr in ir_instructions], indent=2)
    
    def save_ir_mappings(self, output_dir: str):
        """Save all IR mappings to files"""
        
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        for language, mappings in self.ir_mappings.items():
            mapping_file = output_path / f"{language}_ir_mapping.json"
            
            # Convert IRInstruction objects to dictionaries for JSON serialization
            serializable_mappings = {}
            for node_type, instructions in mappings.items():
                serializable_mappings[node_type] = [
                    {
                        'opcode': instr.opcode.value,
                        'operands': instr.operands,
                        'metadata': instr.metadata,
                        'result': instr.result,
                        'source_mapping': instr.source_mapping
                    }
                    for instr in instructions
                ]
            
            with open(mapping_file, 'w') as f:
                json.dump(serializable_mappings, f, indent=2)
        
        print(f"✅ IR mappings saved to {output_dir}")

# Integration function
def integrate_universal_ir_mapper(ide_instance):
    """Integrate universal IR mapper with IDE"""
    
    ide_instance.ir_mapper = UniversalIRMapper(ide_instance.language_registry)
    print("🔄 Universal IR Mapper integrated with IDE")

if __name__ == "__main__":
    print("🔄 Universal IR Mapper")
    print("=" * 50)
    print("✅ Universal IR Mapper ready!")
