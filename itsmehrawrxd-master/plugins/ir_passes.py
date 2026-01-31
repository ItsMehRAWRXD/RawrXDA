#!/usr/bin/env python3
"""
IR Passes for Extensible Compiler System
Implements optimization passes for AST/IR transformation
"""

from typing import List, Any, Dict
from main_compiler_system import BaseIRPass
from ast_nodes import *

IR_PASS_INFO = {
    'constant_folding': {
        'name': 'Constant Folding',
        'description': 'Fold constant expressions',
        'pass_class': 'ASTOptimizerPass',
        'dependencies': [],
        'optimization_level': 1
    },
    'dead_code_elimination': {
        'name': 'Dead Code Elimination',
        'description': 'Remove unreachable code',
        'pass_class': 'DeadCodeEliminationPass',
        'dependencies': ['constant_folding'],
        'optimization_level': 2
    },
    'unused_variable_removal': {
        'name': 'Unused Variable Removal',
        'description': 'Remove unused variable declarations',
        'pass_class': 'UnusedVariableRemovalPass',
        'dependencies': ['dead_code_elimination'],
        'optimization_level': 2
    }
}

class ASTOptimizerPass(BaseIRPass):
    """Constant folding optimization pass"""
    
    def __init__(self):
        self.optimizations_applied = 0
    
    def apply(self, ir: Any, optimization_level: int) -> Any:
        """Apply constant folding optimization"""
        self.optimizations_applied = 0
        
        if isinstance(ir, Program):
            optimized_statements = []
            for stmt in ir.statements:
                optimized_stmt = self._optimize_node(stmt)
                if optimized_stmt:
                    optimized_statements.append(optimized_stmt)
            ir.statements = optimized_statements
        else:
            ir = self._optimize_node(ir)
        
        return ir
    
    def _optimize_node(self, node):
        """Optimize a single AST node"""
        if isinstance(node, BinaryOperation):
            return self._optimize_binary_operation(node)
        elif isinstance(node, UnaryOperation):
            return self._optimize_unary_operation(node)
        elif isinstance(node, Block):
            return self._optimize_block(node)
        elif isinstance(node, IfStatement):
            return self._optimize_if_statement(node)
        elif isinstance(node, WhileStatement):
            return self._optimize_while_statement(node)
        elif isinstance(node, ForStatement):
            return self._optimize_for_statement(node)
        elif isinstance(node, FunctionDeclaration):
            return self._optimize_function_declaration(node)
        else:
            return node
    
    def _optimize_binary_operation(self, node: BinaryOperation):
        """Optimize binary operations"""
        left = self._optimize_node(node.left)
        right = self._optimize_node(node.right)
        
        # Constant folding
        if isinstance(left, Literal) and isinstance(right, Literal):
            try:
                result = self._evaluate_binary_operation(left.value, node.operator, right.value)
                if result is not None:
                    self.optimizations_applied += 1
                    return Literal(result, self._get_result_type(left.literal_type, right.literal_type, node.operator))
            except (ValueError, TypeError, ZeroDivisionError):
                pass
        
        return BinaryOperation(node.operator, left, right)
    
    def _optimize_unary_operation(self, node: UnaryOperation):
        """Optimize unary operations"""
        operand = self._optimize_node(node.operand)
        
        # Constant folding
        if isinstance(operand, Literal):
            try:
                result = self._evaluate_unary_operation(node.operator, operand.value)
                if result is not None:
                    self.optimizations_applied += 1
                    return Literal(result, operand.literal_type)
            except (ValueError, TypeError):
                pass
        
        return UnaryOperation(node.operator, operand)
    
    def _optimize_block(self, node: Block):
        """Optimize block statements"""
        optimized_statements = []
        for stmt in node.statements:
            optimized_stmt = self._optimize_node(stmt)
            if optimized_stmt:
                optimized_statements.append(optimized_stmt)
        return Block(optimized_statements)
    
    def _optimize_if_statement(self, node: IfStatement):
        """Optimize if statements"""
        condition = self._optimize_node(node.condition)
        then_branch = self._optimize_node(node.then_branch)
        else_branch = self._optimize_node(node.else_branch) if node.else_branch else None
        
        # Constant condition folding
        if isinstance(condition, Literal) and condition.literal_type == 'bool':
            self.optimizations_applied += 1
            if condition.value:
                return then_branch
            else:
                return else_branch or Block([])
        
        return IfStatement(condition, then_branch, else_branch)
    
    def _optimize_while_statement(self, node: WhileStatement):
        """Optimize while statements"""
        condition = self._optimize_node(node.condition)
        body = self._optimize_node(node.body)
        
        # Constant condition folding
        if isinstance(condition, Literal) and condition.literal_type == 'bool':
            if not condition.value:
                self.optimizations_applied += 1
                return Block([])  # Remove unreachable loop
        
        return WhileStatement(condition, body)
    
    def _optimize_for_statement(self, node: ForStatement):
        """Optimize for statements"""
        initializer = self._optimize_node(node.initializer) if node.initializer else None
        condition = self._optimize_node(node.condition) if node.condition else None
        increment = self._optimize_node(node.increment) if node.increment else None
        body = self._optimize_node(node.body)
        
        return ForStatement(initializer, condition, increment, body)
    
    def _optimize_function_declaration(self, node: FunctionDeclaration):
        """Optimize function declarations"""
        optimized_parameters = []
        for param in node.parameters:
            optimized_param = self._optimize_node(param)
            if optimized_param:
                optimized_parameters.append(optimized_param)
        
        optimized_body = self._optimize_node(node.body) if node.body else None
        
        return FunctionDeclaration(node.name, optimized_parameters, node.return_type, optimized_body, node.is_async)
    
    def _evaluate_binary_operation(self, left, operator, right):
        """Evaluate binary operation on constants"""
        if operator == '+':
            return left + right
        elif operator == '-':
            return left - right
        elif operator == '*':
            return left * right
        elif operator == '/':
            if right == 0:
                raise ZeroDivisionError("Division by zero")
            return left / right
        elif operator == '%':
            if right == 0:
                raise ZeroDivisionError("Modulo by zero")
            return left % right
        elif operator == '==':
            return left == right
        elif operator == '!=':
            return left != right
        elif operator == '<':
            return left < right
        elif operator == '>':
            return left > right
        elif operator == '<=':
            return left <= right
        elif operator == '>=':
            return left >= right
        elif operator == '&&':
            return left and right
        elif operator == '||':
            return left or right
        else:
            return None
    
    def _evaluate_unary_operation(self, operator, operand):
        """Evaluate unary operation on constants"""
        if operator == '-':
            return -operand
        elif operator == '+':
            return +operand
        elif operator == '!':
            return not operand
        else:
            return None
    
    def _get_result_type(self, left_type, right_type, operator):
        """Determine result type of binary operation"""
        if operator in ['==', '!=', '<', '>', '<=', '>=', '&&', '||']:
            return 'bool'
        elif left_type == 'float' or right_type == 'float':
            return 'float'
        else:
            return left_type
    
    def get_name(self) -> str:
        return 'constant_folding'
    
    def get_description(self) -> str:
        return f'Constant Folding (applied {self.optimizations_applied} optimizations)'

class DeadCodeEliminationPass(BaseIRPass):
    """Dead code elimination pass"""
    
    def __init__(self):
        self.eliminations_applied = 0
    
    def apply(self, ir: Any, optimization_level: int) -> Any:
        """Apply dead code elimination"""
        self.eliminations_applied = 0
        
        if isinstance(ir, Program):
            ir.statements = self._eliminate_dead_statements(ir.statements)
        else:
            ir = self._eliminate_dead_code(ir)
        
        return ir
    
    def _eliminate_dead_code(self, node):
        """Eliminate dead code from a node"""
        if isinstance(node, Block):
            return self._eliminate_dead_block(node)
        elif isinstance(node, IfStatement):
            return self._eliminate_dead_if(node)
        elif isinstance(node, WhileStatement):
            return self._eliminate_dead_while(node)
        elif isinstance(node, ForStatement):
            return self._eliminate_dead_for(node)
        elif isinstance(node, FunctionDeclaration):
            return self._eliminate_dead_function(node)
        else:
            return node
    
    def _eliminate_dead_block(self, node: Block):
        """Eliminate dead code from block"""
        optimized_statements = []
        for stmt in node.statements:
            optimized_stmt = self._eliminate_dead_code(stmt)
            if optimized_stmt:
                optimized_statements.append(optimized_stmt)
        return Block(optimized_statements)
    
    def _eliminate_dead_if(self, node: IfStatement):
        """Eliminate dead code from if statement"""
        condition = self._eliminate_dead_code(node.condition)
        then_branch = self._eliminate_dead_code(node.then_branch)
        else_branch = self._eliminate_dead_code(node.else_branch) if node.else_branch else None
        
        # If condition is always false, eliminate then branch
        if isinstance(condition, Literal) and condition.literal_type == 'bool' and not condition.value:
            self.eliminations_applied += 1
            return else_branch or Block([])
        
        # If condition is always true, eliminate else branch
        if isinstance(condition, Literal) and condition.literal_type == 'bool' and condition.value:
            self.eliminations_applied += 1
            return then_branch
        
        return IfStatement(condition, then_branch, else_branch)
    
    def _eliminate_dead_while(self, node: WhileStatement):
        """Eliminate dead code from while statement"""
        condition = self._eliminate_dead_code(node.condition)
        body = self._eliminate_dead_code(node.body)
        
        # If condition is always false, eliminate the loop
        if isinstance(condition, Literal) and condition.literal_type == 'bool' and not condition.value:
            self.eliminations_applied += 1
            return Block([])
        
        return WhileStatement(condition, body)
    
    def _eliminate_dead_for(self, node: ForStatement):
        """Eliminate dead code from for statement"""
        initializer = self._eliminate_dead_code(node.initializer) if node.initializer else None
        condition = self._eliminate_dead_code(node.condition) if node.condition else None
        increment = self._eliminate_dead_code(node.increment) if node.increment else None
        body = self._eliminate_dead_code(node.body)
        
        # If condition is always false, eliminate the loop
        if isinstance(condition, Literal) and condition.literal_type == 'bool' and not condition.value:
            self.eliminations_applied += 1
            return Block([])
        
        return ForStatement(initializer, condition, increment, body)
    
    def _eliminate_dead_function(self, node: FunctionDeclaration):
        """Eliminate dead code from function"""
        optimized_parameters = []
        for param in node.parameters:
            optimized_param = self._eliminate_dead_code(param)
            if optimized_param:
                optimized_parameters.append(optimized_param)
        
        optimized_body = self._eliminate_dead_code(node.body) if node.body else None
        
        return FunctionDeclaration(node.name, optimized_parameters, node.return_type, optimized_body, node.is_async)
    
    def _eliminate_dead_statements(self, statements):
        """Eliminate dead statements from a list"""
        optimized_statements = []
        for stmt in statements:
            optimized_stmt = self._eliminate_dead_code(stmt)
            if optimized_stmt:
                optimized_statements.append(optimized_stmt)
        return optimized_statements
    
    def get_name(self) -> str:
        return 'dead_code_elimination'
    
    def get_description(self) -> str:
        return f'Dead Code Elimination (eliminated {self.eliminations_applied} dead code blocks)'

class UnusedVariableRemovalPass(BaseIRPass):
    """Unused variable removal pass"""
    
    def __init__(self):
        self.variables_removed = 0
    
    def apply(self, ir: Any, optimization_level: int) -> Any:
        """Apply unused variable removal"""
        self.variables_removed = 0
        
        if isinstance(ir, Program):
            ir.statements = self._remove_unused_variables(ir.statements)
        else:
            ir = self._remove_unused_variables_from_node(ir)
        
        return ir
    
    def _remove_unused_variables(self, statements):
        """Remove unused variables from statements"""
        # First pass: collect all variable declarations and their usage
        variable_declarations = {}
        variable_usage = set()
        
        for stmt in statements:
            self._collect_variable_info(stmt, variable_declarations, variable_usage)
        
        # Second pass: remove unused variable declarations
        optimized_statements = []
        for stmt in statements:
            if isinstance(stmt, VariableDeclaration):
                if stmt.name not in variable_usage:
                    self.variables_removed += 1
                    continue  # Skip unused variable
            optimized_statements.append(stmt)
        
        return optimized_statements
    
    def _collect_variable_info(self, node, variable_declarations, variable_usage):
        """Collect variable declaration and usage information"""
        if isinstance(node, VariableDeclaration):
            variable_declarations[node.name] = node
        elif isinstance(node, Identifier):
            variable_usage.add(node.name)
        elif isinstance(node, Assignment):
            variable_usage.add(node.name)
        elif isinstance(node, Block):
            for stmt in node.statements:
                self._collect_variable_info(stmt, variable_declarations, variable_usage)
        elif isinstance(node, IfStatement):
            self._collect_variable_info(node.condition, variable_declarations, variable_usage)
            self._collect_variable_info(node.then_branch, variable_declarations, variable_usage)
            if node.else_branch:
                self._collect_variable_info(node.else_branch, variable_declarations, variable_usage)
        elif isinstance(node, WhileStatement):
            self._collect_variable_info(node.condition, variable_declarations, variable_usage)
            self._collect_variable_info(node.body, variable_declarations, variable_usage)
        elif isinstance(node, ForStatement):
            if node.initializer:
                self._collect_variable_info(node.initializer, variable_declarations, variable_usage)
            if node.condition:
                self._collect_variable_info(node.condition, variable_declarations, variable_usage)
            if node.increment:
                self._collect_variable_info(node.increment, variable_declarations, variable_usage)
            self._collect_variable_info(node.body, variable_declarations, variable_usage)
        elif isinstance(node, FunctionDeclaration):
            for param in node.parameters:
                self._collect_variable_info(param, variable_declarations, variable_usage)
            if node.body:
                self._collect_variable_info(node.body, variable_declarations, variable_usage)
        elif isinstance(node, BinaryOperation):
            self._collect_variable_info(node.left, variable_declarations, variable_usage)
            self._collect_variable_info(node.right, variable_declarations, variable_usage)
        elif isinstance(node, UnaryOperation):
            self._collect_variable_info(node.operand, variable_declarations, variable_usage)
        elif isinstance(node, FunctionCall):
            for arg in node.arguments:
                self._collect_variable_info(arg, variable_declarations, variable_usage)
        elif isinstance(node, ReturnStatement):
            if node.value:
                self._collect_variable_info(node.value, variable_declarations, variable_usage)
    
    def _remove_unused_variables_from_node(self, node):
        """Remove unused variables from a single node"""
        if isinstance(node, Block):
            return Block(self._remove_unused_variables(node.statements))
        else:
            return node
    
    def get_name(self) -> str:
        return 'unused_variable_removal'
    
    def get_description(self) -> str:
        return f'Unused Variable Removal (removed {self.variables_removed} unused variables)'
