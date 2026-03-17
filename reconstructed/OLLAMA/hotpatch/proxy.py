#!/usr/bin/env python3
"""
Ollama Hotpatching Proxy
Comprehensive system for constraint enforcement, intelligent routing, and agentic integration
"""

import asyncio
import json
import logging
import time
import uuid
from typing import Any, Dict, List, Optional, Tuple
from dataclasses import dataclass, asdict, field
from datetime import datetime
import hashlib
import re
from enum import Enum
from collections import defaultdict

import aiohttp
from aiohttp import web
import numpy as np

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class RequestType(Enum):
    """Classify request types for intelligent routing"""
    CODE_GENERATION = "code_gen"
    ANALYSIS = "analysis"
    REASONING = "reasoning"
    CREATIVE = "creative"
    FACTUAL = "factual"
    AGENTIC = "agentic"


class ModelProfile(Enum):
    """Model capability profiles"""
    PHI = "phi"  # Fast, prone to hallucinations
    NEURAL_CHAT = "neural-chat"  # Balanced
    MISTRAL = "mistral"  # Strong reasoning
    LLAMA2 = "llama2"  # General purpose


@dataclass
class Constraint:
    """Represents a constraint that must be enforced"""
    name: str
    description: str
    validation_fn: callable
    error_message: str
    severity: str = "warn"  # warn or fail
    applies_to: List[str] = field(default_factory=lambda: ["all"])


@dataclass
class RequestMetadata:
    """Metadata about a request for routing and A/B testing"""
    request_id: str
    timestamp: float
    request_type: RequestType
    model: str
    prompt_hash: str
    original_prompt: str
    rewritten_prompt: str
    constraints: List[str]
    routing_score: float = 0.0


@dataclass
class ResponseMetadata:
    """Metadata about a response for evaluation"""
    request_id: str
    model: str
    generation_time: float
    response_hash: str
    hallucination_score: float
    constraint_violations: List[str] = field(default_factory=list)
    ab_test_group: Optional[str] = None


class ConstraintEnforcer:
    """Enforces constraints on prompts and responses"""
    
    def __init__(self):
        self.constraints: Dict[str, Constraint] = {}
        self._register_default_constraints()
    
    def _register_default_constraints(self):
        """Register default constraints for all models"""
        
        # Prevent prompt injection
        self.register_constraint(Constraint(
            name="no_prompt_injection",
            description="Prevent prompt injection attacks",
            validation_fn=self._check_prompt_injection,
            error_message="Potential prompt injection detected",
            severity="fail",
            applies_to=["all"]
        ))
        
        # Limit output length to prevent token exhaustion
        self.register_constraint(Constraint(
            name="max_output_length",
            description="Enforce maximum output length",
            validation_fn=self._check_output_length,
            error_message="Response exceeds maximum length",
            severity="warn"
        ))
        
        # Enforce factuality for factual requests
        self.register_constraint(Constraint(
            name="factuality_check",
            description="Verify factual accuracy where possible",
            validation_fn=self._check_factuality,
            error_message="Response contains unverified claims",
            severity="warn",
            applies_to=[RequestType.FACTUAL.value]
        ))
        
        # Prevent harmful content
        self.register_constraint(Constraint(
            name="safety_check",
            description="Block harmful or unsafe content",
            validation_fn=self._check_safety,
            error_message="Response contains unsafe content",
            severity="fail"
        ))
        
        # Code validity for code generation
        self.register_constraint(Constraint(
            name="code_validity",
            description="Validate generated code syntax",
            validation_fn=self._check_code_validity,
            error_message="Generated code has syntax errors",
            severity="warn",
            applies_to=[RequestType.CODE_GENERATION.value]
        ))
    
    def register_constraint(self, constraint: Constraint):
        """Register a new constraint"""
        self.constraints[constraint.name] = constraint
    
    def _check_prompt_injection(self, text: str, context: Dict = None) -> Tuple[bool, str]:
        """Detect prompt injection patterns"""
        injection_patterns = [
            r'(?i)ignore.*(?:previous|prior|above)',
            r'(?i)forget.*(?:previous|prior|above)',
            r'(?i)disregard.*(?:instructions|prompt)',
            r'(?i)\[SYSTEM\]',
            r'(?i)\[ADMIN\]',
            r'(?i)jailbreak',
            r'(?i)bypass',
        ]
        
        for pattern in injection_patterns:
            if re.search(pattern, text):
                return False, f"Injection pattern detected: {pattern}"
        
        return True, ""
    
    def _check_output_length(self, text: str, context: Dict = None) -> Tuple[bool, str]:
        """Check output doesn't exceed reasonable limits"""
        max_length = context.get('max_length', 10000) if context else 10000
        if len(text) > max_length:
            return False, f"Output exceeds {max_length} characters"
        return True, ""
    
    def _check_factuality(self, text: str, context: Dict = None) -> Tuple[bool, str]:
        """Check for unverified factual claims"""
        # Look for high-confidence claims without evidence markers
        unverified_patterns = [
            r'\b(?:definitely|certainly|absolutely|surely|undoubtedly)\s+\w+\s+(?:is|are|was|were)\s+[^.!?]*\.',
        ]
        
        suspicious_count = 0
        for pattern in unverified_patterns:
            suspicious_count += len(re.findall(pattern, text, re.IGNORECASE))
        
        if suspicious_count > 3:
            return False, "Multiple unverified factual claims detected"
        return True, ""
    
    def _check_safety(self, text: str, context: Dict = None) -> Tuple[bool, str]:
        """Check for harmful content"""
        unsafe_patterns = [
            r'(?i)(?:bomb|explosive|weapon|poison|kill|harm|abuse)',
            r'(?i)(?:hate|racism|sexism|discrimination)',
        ]
        
        for pattern in unsafe_patterns:
            if re.search(pattern, text):
                return False, "Unsafe content detected"
        
        return True, ""
    
    def _check_code_validity(self, text: str, context: Dict = None) -> Tuple[bool, str]:
        """Basic code syntax validation"""
        # Extract code blocks
        code_blocks = re.findall(r'```(?:\w+)?\n(.*?)\n```', text, re.DOTALL)
        
        for code in code_blocks:
            try:
                compile(code, '<string>', 'exec')
            except SyntaxError as e:
                return False, f"Syntax error in code: {str(e)}"
        
        return True, ""
    
    def validate_prompt(self, prompt: str, request_type: RequestType) -> Tuple[bool, List[str]]:
        """Validate prompt against applicable constraints"""
        violations = []
        
        for constraint in self.constraints.values():
            if "all" in constraint.applies_to or request_type.value in constraint.applies_to:
                is_valid, error = constraint.validation_fn(prompt)
                if not is_valid:
                    violations.append(f"{constraint.name}: {error}")
                    if constraint.severity == "fail":
                        return False, violations
        
        return True, violations
    
    def validate_response(self, response: str, request_type: RequestType,
                         context: Dict = None) -> Tuple[bool, List[str]]:
        """Validate response against applicable constraints"""
        violations = []
        
        for constraint in self.constraints.values():
            if "all" in constraint.applies_to or request_type.value in constraint.applies_to:
                is_valid, error = constraint.validation_fn(response, context or {})
                if not is_valid:
                    violations.append(f"{constraint.name}: {error}")
                    if constraint.severity == "fail":
                        return False, violations
        
        return True, violations


class PromptRewriter:
    """Rewrites prompts to enforce constraints and improve model behavior"""
    
    def __init__(self):
        self.system_prompts = self._build_system_prompts()
    
    def _build_system_prompts(self) -> Dict[str, str]:
        """Build context-aware system prompts for each model"""
        return {
            "phi": """You are a precise, helpful AI assistant. Follow these rules:
1. Be concise and clear
2. Acknowledge uncertainty: say "I'm not sure" when appropriate
3. Provide evidence for claims
4. Ask clarifying questions if needed
5. Do not speculate beyond your knowledge
6. Format code with proper syntax
7. Break complex tasks into steps""",
            
            "neural-chat": """You are a balanced, thoughtful AI assistant.
1. Consider multiple perspectives
2. Provide nuanced answers
3. Cite sources when making claims
4. Acknowledge limitations
5. Be respectful and helpful
6. Focus on clarity over brevity""",
            
            "mistral": """You are a logical, reasoning-focused AI assistant.
1. Use step-by-step reasoning
2. Identify assumptions
3. Consider edge cases
4. Provide complete analysis
5. Validate conclusions
6. Be thorough and precise""",
            
            "llama2": """You are a general-purpose, helpful AI assistant.
1. Be clear and organized
2. Provide complete answers
3. Use examples when helpful
4. Acknowledge what you don't know
5. Be conversational and friendly
6. Focus on being useful""",
        }
    
    def rewrite_prompt(self, prompt: str, model: str, request_type: RequestType,
                       constraints: List[str] = None) -> str:
        """Rewrite prompt with model-specific system prompt and constraints"""
        
        system_prompt = self.system_prompts.get(model, self.system_prompts["llama2"])
        
        # Add constraint reminders
        constraint_reminders = []
        if constraints:
            constraint_reminders = [f"- {c}" for c in constraints]
        
        # Build rewritten prompt with context
        rewritten = f"""{system_prompt}

Request Type: {request_type.value}

{"Additional constraints:" + chr(10) + chr(10).join(constraint_reminders) if constraint_reminders else ""}

User Request:
{prompt}

Please provide a thoughtful, accurate response following the guidelines above."""
        
        return rewritten
    
    def inject_hallucination_prevention(self, prompt: str) -> str:
        """Add special tokens/patterns to reduce hallucinations"""
        injection = """[ACCURATE_MODE]
If you are not certain about something:
- Say "I'm not certain, but..."
- Provide your best knowledge with caveats
- Suggest where to find authoritative information
[END_ACCURATE_MODE]

"""
        return injection + prompt


class HallucinationDetector:
    """Detects and scores hallucinations in responses"""
    
    def __init__(self):
        self.hallucination_patterns = self._build_patterns()
        self.fact_cache = {}
    
    def _build_patterns(self) -> List[Tuple[str, float]]:
        """Build hallucination detection patterns with scores"""
        return [
            # Overconfident false claims
            (r'\b(?:obviously|clearly|definitely|certainly)\s+(?:the|a)\s+\w+\s+(?:is|are)\s+\w+', 0.3),
            # Invented statistics without source
            (r'(?<![\d\s])(\d{1,3}%)\s+(?:of|in|among)', 0.2),
            # Made-up names/dates
            (r'[A-Z][a-z]+\s+(?:discovered|invented|created)\s+(?:in\s+)?(?:19|20)\d{2}', 0.2),
            # Contradictions
            (r'(?i)(?:however|but|actually|in fact).*(?:contradicts|disagrees)', 0.4),
            # Unverifiable claims
            (r'(?i)(?:my|your)\s+(?:personal|private)\s+(?:opinion|belief)', 0.1),
        ]
    
    def calculate_hallucination_score(self, text: str, prompt: str = "") -> float:
        """Calculate hallucination probability (0-1)"""
        score = 0.0
        matches = 0
        
        for pattern, weight in self.hallucination_patterns:
            found = re.findall(pattern, text, re.IGNORECASE)
            if found:
                matches += 1
                score += weight
        
        # Normalize and cap at 1.0
        if matches > 0:
            score = min(1.0, score / matches)
        
        # Boost score if response is very confident but vague
        if re.search(r'\b(?:definitely|certainly|absolutely)\b', text, re.IGNORECASE) and \
           len(text) < 100:
            score = min(1.0, score + 0.2)
        
        return score
    
    def flag_hallucination(self, text: str, threshold: float = 0.5) -> bool:
        """Check if response likely contains hallucinations"""
        return self.calculate_hallucination_score(text) > threshold


class RequestRouter:
    """Intelligently routes requests to appropriate models"""
    
    def __init__(self):
        self.model_capabilities = self._build_model_capabilities()
        self.routing_history = defaultdict(list)
    
    def _build_model_capabilities(self) -> Dict[str, Dict]:
        """Define model capabilities and strengths"""
        return {
            "phi": {
                "speed": 0.95,
                "code": 0.8,
                "reasoning": 0.6,
                "factuality": 0.7,
                "creative": 0.5,
                "hallucination_prone": True,
            },
            "neural-chat": {
                "speed": 0.7,
                "code": 0.75,
                "reasoning": 0.75,
                "factuality": 0.8,
                "creative": 0.8,
                "hallucination_prone": False,
            },
            "mistral": {
                "speed": 0.6,
                "code": 0.85,
                "reasoning": 0.9,
                "factuality": 0.85,
                "creative": 0.7,
                "hallucination_prone": False,
            },
            "llama2": {
                "speed": 0.65,
                "code": 0.7,
                "reasoning": 0.75,
                "factuality": 0.75,
                "creative": 0.85,
                "hallucination_prone": False,
            },
        }
    
    def classify_request(self, prompt: str) -> RequestType:
        """Classify request type based on content"""
        prompt_lower = prompt.lower()
        
        if any(word in prompt_lower for word in ['code', 'function', 'script', 'api', 'class']):
            return RequestType.CODE_GENERATION
        
        if any(word in prompt_lower for word in ['analyze', 'review', 'evaluate', 'examine']):
            return RequestType.ANALYSIS
        
        if any(word in prompt_lower for word in ['why', 'how', 'explain', 'reason', 'logic']):
            return RequestType.REASONING
        
        if any(word in prompt_lower for word in ['story', 'poem', 'create', 'imagine', 'write']):
            return RequestType.CREATIVE
        
        if any(word in prompt_lower for word in ['what', 'define', 'fact', 'true', 'real']):
            return RequestType.FACTUAL
        
        if any(word in prompt_lower for word in ['tool', 'action', 'agent', 'execute', 'run']):
            return RequestType.AGENTIC
        
        return RequestType.REASONING
    
    def select_best_model(self, request_type: RequestType,
                         available_models: List[str]) -> Tuple[str, float]:
        """Select best model for request type with scoring"""
        
        weights = {
            RequestType.CODE_GENERATION: {"code": 0.5, "factuality": 0.3, "speed": 0.2},
            RequestType.ANALYSIS: {"reasoning": 0.5, "factuality": 0.3, "code": 0.2},
            RequestType.REASONING: {"reasoning": 0.6, "factuality": 0.4},
            RequestType.CREATIVE: {"creative": 0.7, "speed": 0.3},
            RequestType.FACTUAL: {"factuality": 0.8, "reasoning": 0.2},
            RequestType.AGENTIC: {"reasoning": 0.5, "code": 0.3, "speed": 0.2},
        }
        
        request_weights = weights.get(request_type, weights[RequestType.REASONING])
        
        best_model = None
        best_score = -1.0
        
        for model in available_models:
            if model not in self.model_capabilities:
                continue
            
            capabilities = self.model_capabilities[model]
            score = 0.0
            
            for capability, weight in request_weights.items():
                if capability in capabilities:
                    score += capabilities[capability] * weight
            
            # Penalize hallucination-prone models for factual requests
            if request_type == RequestType.FACTUAL and capabilities.get('hallucination_prone'):
                score *= 0.7
            
            if score > best_score:
                best_score = score
                best_model = model
        
        return best_model or available_models[0], best_score
    
    def get_routing_score(self, model: str, request_type: RequestType) -> float:
        """Get confidence score for model routing decision"""
        if model not in self.model_capabilities:
            return 0.5
        
        weights = {
            RequestType.CODE_GENERATION: {"code": 0.5, "factuality": 0.3, "speed": 0.2},
            RequestType.ANALYSIS: {"reasoning": 0.5, "factuality": 0.3, "code": 0.2},
            RequestType.REASONING: {"reasoning": 0.6, "factuality": 0.4},
            RequestType.CREATIVE: {"creative": 0.7, "speed": 0.3},
            RequestType.FACTUAL: {"factuality": 0.8, "reasoning": 0.2},
            RequestType.AGENTIC: {"reasoning": 0.5, "code": 0.3, "speed": 0.2},
        }
        
        request_weights = weights.get(request_type, weights[RequestType.REASONING])
        capabilities = self.model_capabilities[model]
        
        score = 0.0
        for capability, weight in request_weights.items():
            if capability in capabilities:
                score += capabilities[capability] * weight
        
        return score


class ABTestingFramework:
    """Real-time A/B testing between models"""
    
    def __init__(self):
        self.active_tests: Dict[str, Dict] = {}
        self.results: Dict[str, List[Dict]] = defaultdict(list)
        self.test_id_counter = 0
    
    def create_test(self, model_a: str, model_b: str,
                   request_type: RequestType,
                   test_duration: float = 3600) -> str:
        """Create a new A/B test"""
        test_id = f"test_{self.test_id_counter}_{int(time.time())}"
        self.test_id_counter += 1
        
        self.active_tests[test_id] = {
            "model_a": model_a,
            "model_b": model_b,
            "request_type": request_type.value,
            "start_time": time.time(),
            "end_time": time.time() + test_duration,
            "group_a_count": 0,
            "group_b_count": 0,
            "group_a_metrics": [],
            "group_b_metrics": [],
        }
        
        logger.info(f"Created A/B test {test_id}: {model_a} vs {model_b}")
        return test_id
    
    def assign_group(self, test_id: str) -> Tuple[str, str]:
        """Assign request to test group (returns model and group)"""
        if test_id not in self.active_tests:
            return None, None
        
        test = self.active_tests[test_id]
        
        # Round-robin assignment
        if test["group_a_count"] <= test["group_b_count"]:
            test["group_a_count"] += 1
            return test["model_a"], "A"
        else:
            test["group_b_count"] += 1
            return test["model_b"], "B"
    
    def record_result(self, test_id: str, group: str, metrics: Dict):
        """Record test result"""
        if test_id in self.active_tests:
            test = self.active_tests[test_id]
            key = f"group_{group.lower()}_metrics"
            test[key].append(metrics)
            
            # Record for analysis
            self.results[test_id].append({
                "group": group,
                "timestamp": time.time(),
                **metrics
            })
    
    def get_test_results(self, test_id: str) -> Dict:
        """Get current test results"""
        if test_id not in self.active_tests:
            return {}
        
        test = self.active_tests[test_id]
        
        def calculate_stats(metrics_list):
            if not metrics_list:
                return {}
            
            hallucination_scores = [m.get('hallucination_score', 0) for m in metrics_list]
            response_times = [m.get('generation_time', 0) for m in metrics_list]
            
            return {
                "count": len(metrics_list),
                "avg_hallucination": np.mean(hallucination_scores) if hallucination_scores else 0,
                "avg_response_time": np.mean(response_times) if response_times else 0,
                "violations_total": sum(len(m.get('constraint_violations', [])) for m in metrics_list),
            }
        
        return {
            "test_id": test_id,
            "model_a": test["model_a"],
            "model_b": test["model_b"],
            "group_a": calculate_stats(test["group_a_metrics"]),
            "group_b": calculate_stats(test["group_b_metrics"]),
            "winner": self._determine_winner(test),
        }
    
    def _determine_winner(self, test: Dict) -> Optional[str]:
        """Determine which model is performing better"""
        a_metrics = test["group_a_metrics"]
        b_metrics = test["group_b_metrics"]
        
        if not a_metrics or not b_metrics:
            return None
        
        a_hallucination = np.mean([m.get('hallucination_score', 0) for m in a_metrics])
        b_hallucination = np.mean([m.get('hallucination_score', 0) for m in b_metrics])
        
        if a_hallucination < b_hallucination:
            return test["model_a"]
        elif b_hallucination < a_hallucination:
            return test["model_b"]
        
        return None


class AgenticIDEIntegration:
    """Integration layer for agentic capabilities within IDE"""
    
    def __init__(self):
        self.available_tools = self._build_tool_registry()
        self.execution_history = []
    
    def _build_tool_registry(self) -> Dict[str, Dict]:
        """Build registry of available IDE tools for agents"""
        return {
            "read_file": {
                "description": "Read contents of a file",
                "params": {"path": "str", "start_line": "int", "end_line": "int"},
                "executable": True,
            },
            "write_file": {
                "description": "Write or append to a file",
                "params": {"path": "str", "content": "str", "mode": "str"},
                "executable": True,
            },
            "search_code": {
                "description": "Search for code patterns or symbols",
                "params": {"pattern": "str", "file_path": "str"},
                "executable": True,
            },
            "compile": {
                "description": "Compile code in specified language",
                "params": {"code": "str", "language": "str"},
                "executable": True,
            },
            "execute_command": {
                "description": "Execute shell/terminal command",
                "params": {"command": "str", "cwd": "str"},
                "executable": True,
            },
            "list_files": {
                "description": "List files in directory",
                "params": {"path": "str", "recursive": "bool"},
                "executable": True,
            },
            "analyze_error": {
                "description": "Analyze compilation or runtime error",
                "params": {"error_message": "str", "context": "str"},
                "executable": False,
            },
        }
    
    def inject_agentic_prompt(self, prompt: str) -> str:
        """Inject agentic capabilities into prompt"""
        tools_description = self._format_tools_description()
        
        agentic_wrapper = f"""You are now operating as an Agentic IDE Assistant.
You have access to the following tools:

{tools_description}

Instructions:
1. For complex tasks, break them into steps
2. Use tools to inspect, modify, and test code
3. Provide tool calls in this format: <TOOL_CALL>tool_name(param1, param2)</TOOL_CALL>
4. After each tool call, include the result and next step
5. Validate changes and provide error handling
6. Document your reasoning for each action

Task:
{prompt}

Begin by analyzing the task and determining which tools you need."""
        
        return agentic_wrapper
    
    def _format_tools_description(self) -> str:
        """Format tools as readable description"""
        tools_text = []
        for tool_name, tool_info in self.available_tools.items():
            params = ", ".join([f"{k}: {v}" for k, v in tool_info["params"].items()])
            tools_text.append(f"- {tool_name}({params}): {tool_info['description']}")
        return "\n".join(tools_text)
    
    def parse_tool_calls(self, response: str) -> List[Tuple[str, Dict]]:
        """Parse tool calls from model response"""
        pattern = r'<TOOL_CALL>(\w+)\((.*?)\)</TOOL_CALL>'
        matches = re.findall(pattern, response)
        
        tool_calls = []
        for tool_name, args_str in matches:
            # Simple argument parsing (could be enhanced)
            args = {}
            if args_str:
                for arg in args_str.split(','):
                    if '=' in arg:
                        k, v = arg.split('=', 1)
                        args[k.strip()] = v.strip().strip('"\'')
            tool_calls.append((tool_name, args))
        
        return tool_calls


class OllamaHotpatchProxy:
    """Main proxy server that orchestrates all components"""
    
    def __init__(self, ollama_base_url: str = "http://localhost:11434"):
        self.ollama_base_url = ollama_base_url
        self.constraint_enforcer = ConstraintEnforcer()
        self.prompt_rewriter = PromptRewriter()
        self.hallucination_detector = HallucinationDetector()
        self.router = RequestRouter()
        self.ab_tester = ABTestingFramework()
        self.agentic_integration = AgenticIDEIntegration()
        self.app = web.Application()
        self._setup_routes()
    
    def _setup_routes(self):
        """Setup API routes"""
        self.app.router.add_post('/api/generate', self.handle_generate)
        self.app.router.add_post('/api/chat', self.handle_chat)
        self.app.router.add_get('/api/models', self.handle_models)
        self.app.router.add_get('/api/health', self.handle_health)
        self.app.router.add_get('/api/ab-test/{test_id}', self.handle_ab_test_results)
        self.app.router.add_post('/api/ab-test/create', self.handle_create_ab_test)
    
    async def handle_generate(self, request: web.Request) -> web.Response:
        """Handle generation requests with hotpatching"""
        try:
            data = await request.json()
            request_id = str(uuid.uuid4())
            
            prompt = data.get('prompt', '')
            model = data.get('model', 'phi')
            use_agentic = data.get('agentic', False)
            ab_test_id = data.get('ab_test_id')
            constraints = data.get('constraints', [])
            
            # Start timing
            start_time = time.time()
            
            logger.info(f"[{request_id}] Received request for model {model}")
            
            # Classify request
            request_type = self.router.classify_request(prompt)
            logger.info(f"[{request_id}] Classified as {request_type.value}")
            
            # Validate prompt constraints
            is_valid, violations = self.constraint_enforcer.validate_prompt(prompt, request_type)
            if not is_valid:
                logger.warning(f"[{request_id}] Prompt validation failed: {violations}")
                return web.json_response({
                    "error": "Prompt validation failed",
                    "violations": violations,
                }, status=400)
            
            # Route to best model if not specified
            if model == "auto":
                available = await self._get_available_models()
                model, score = self.router.select_best_model(request_type, available)
                logger.info(f"[{request_id}] Auto-routed to {model} (score: {score:.2f})")
            
            # Handle A/B testing
            if ab_test_id:
                ab_model, ab_group = self.ab_tester.assign_group(ab_test_id)
                if ab_model:
                    model = ab_model
                    logger.info(f"[{request_id}] A/B test {ab_test_id}, group {ab_group}")
            
            # Rewrite prompt
            original_prompt = prompt
            if use_agentic:
                prompt = self.agentic_integration.inject_agentic_prompt(prompt)
            
            prompt = self.prompt_rewriter.rewrite_prompt(
                prompt, model, request_type, constraints
            )
            
            # Check hallucination prevention
            prompt = self.prompt_rewriter.inject_hallucination_prevention(prompt)
            
            # Call Ollama
            logger.info(f"[{request_id}] Calling {model}")
            response_text = await self._call_ollama(model, prompt, data)
            
            generation_time = time.time() - start_time
            
            # Detect hallucinations
            hallucination_score = self.hallucination_detector.calculate_hallucination_score(
                response_text, original_prompt
            )
            
            # Validate response constraints
            is_valid, violations = self.constraint_enforcer.validate_response(
                response_text, request_type
            )
            
            if not is_valid and self.constraint_enforcer.constraints['max_output_length'].severity == 'fail':
                logger.warning(f"[{request_id}] Response validation failed: {violations}")
            
            # Record A/B test result
            if ab_test_id:
                self.ab_tester.record_result(ab_test_id, ab_group, {
                    "generation_time": generation_time,
                    "hallucination_score": hallucination_score,
                    "constraint_violations": violations,
                })
            
            logger.info(f"[{request_id}] Completed in {generation_time:.2f}s, "
                       f"hallucination: {hallucination_score:.2f}")
            
            return web.json_response({
                "response": response_text,
                "model": model,
                "request_id": request_id,
                "generation_time": generation_time,
                "hallucination_score": hallucination_score,
                "constraint_violations": violations,
                "request_type": request_type.value,
            })
        
        except Exception as e:
            logger.exception(f"Error handling request: {e}")
            return web.json_response({"error": str(e)}, status=500)
    
    async def handle_chat(self, request: web.Request) -> web.Response:
        """Handle chat requests with hotpatching"""
        try:
            data = await request.json()
            request_id = str(uuid.uuid4())
            
            messages = data.get('messages', [])
            model = data.get('model', 'phi')
            use_agentic = data.get('agentic', False)
            
            if not messages:
                return web.json_response({"error": "No messages provided"}, status=400)
            
            # Extract latest user message for classification
            user_message = next((m['content'] for m in reversed(messages) if m['role'] == 'user'), '')
            
            request_type = self.router.classify_request(user_message)
            
            # Rewrite messages with constraint reminders
            modified_messages = []
            for msg in messages:
                modified_messages.append(msg)
            
            if use_agentic:
                modified_messages[-1]['content'] = self.agentic_integration.inject_agentic_prompt(
                    modified_messages[-1]['content']
                )
            
            # Call Ollama
            response_text = await self._call_ollama_chat(model, modified_messages, data)
            
            return web.json_response({
                "response": response_text,
                "model": model,
                "request_id": request_id,
                "request_type": request_type.value,
            })
        
        except Exception as e:
            logger.exception(f"Error handling chat: {e}")
            return web.json_response({"error": str(e)}, status=500)
    
    async def handle_models(self, request: web.Request) -> web.Response:
        """List available models"""
        try:
            models = await self._get_available_models()
            return web.json_response({"models": models})
        except Exception as e:
            logger.exception(f"Error listing models: {e}")
            return web.json_response({"error": str(e)}, status=500)
    
    async def handle_health(self, request: web.Request) -> web.Response:
        """Health check"""
        return web.json_response({
            "status": "healthy",
            "components": {
                "constraint_enforcer": True,
                "prompt_rewriter": True,
                "hallucination_detector": True,
                "router": True,
                "ab_tester": True,
                "agentic_integration": True,
            }
        })
    
    async def handle_ab_test_results(self, request: web.Request) -> web.Response:
        """Get A/B test results"""
        test_id = request.match_info['test_id']
        results = self.ab_tester.get_test_results(test_id)
        return web.json_response(results)
    
    async def handle_create_ab_test(self, request: web.Request) -> web.Response:
        """Create new A/B test"""
        try:
            data = await request.json()
            model_a = data.get('model_a')
            model_b = data.get('model_b')
            request_type_str = data.get('request_type', 'reasoning')
            
            request_type = RequestType[request_type_str.upper()]
            test_id = self.ab_tester.create_test(model_a, model_b, request_type)
            
            return web.json_response({
                "test_id": test_id,
                "model_a": model_a,
                "model_b": model_b,
                "request_type": request_type_str,
            })
        except Exception as e:
            logger.exception(f"Error creating A/B test: {e}")
            return web.json_response({"error": str(e)}, status=500)
    
    async def _call_ollama(self, model: str, prompt: str, params: Dict) -> str:
        """Call Ollama generate endpoint"""
        async with aiohttp.ClientSession() as session:
            payload = {
                "model": model,
                "prompt": prompt,
                "stream": False,
                **{k: v for k, v in params.items() if k not in ['model', 'prompt', 'agentic', 'constraints', 'ab_test_id']}
            }
            
            async with session.post(
                f"{self.ollama_base_url}/api/generate",
                json=payload
            ) as resp:
                result = await resp.json()
                return result.get('response', '')
    
    async def _call_ollama_chat(self, model: str, messages: List, params: Dict) -> str:
        """Call Ollama chat endpoint"""
        async with aiohttp.ClientSession() as session:
            payload = {
                "model": model,
                "messages": messages,
                "stream": False,
                **{k: v for k, v in params.items() if k not in ['model', 'messages', 'agentic']}
            }
            
            async with session.post(
                f"{self.ollama_base_url}/api/chat",
                json=payload
            ) as resp:
                result = await resp.json()
                return result.get('message', {}).get('content', '')
    
    async def _get_available_models(self) -> List[str]:
        """Get list of available models from Ollama"""
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(f"{self.ollama_base_url}/api/tags") as resp:
                    result = await resp.json()
                    return [model['name'].split(':')[0] for model in result.get('models', [])]
        except Exception as e:
            logger.warning(f"Could not fetch model list: {e}")
            return ["phi", "neural-chat", "mistral", "llama2"]
    
    def run(self, host: str = "localhost", port: int = 8080):
        """Run the proxy server"""
        logger.info(f"Starting Ollama hotpatch proxy on {host}:{port}")
        web.run_app(self.app, host=host, port=port)


if __name__ == "__main__":
    import sys
    
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    ollama_url = sys.argv[3] if len(sys.argv) > 3 else "http://localhost:11434"
    
    proxy = OllamaHotpatchProxy(ollama_url)
    proxy.run(host, port)
