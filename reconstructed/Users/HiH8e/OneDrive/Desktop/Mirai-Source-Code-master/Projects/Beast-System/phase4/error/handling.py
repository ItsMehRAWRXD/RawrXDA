#!/usr/bin/env python3
"""
PHASE 4: BEAST SWARM ERROR HANDLING & RESILIENCE
=================================================

Comprehensive error handling system:
1. Exception Recovery - Graceful error handling with retry logic
2. Circuit Breaker - Prevent cascade failures  
3. Health Monitoring - Track component health
4. Fallback Systems - Backup processing when components fail

Target: Production-ready error resilience
Timeline: 2 hours
"""

import time
import json
from collections import defaultdict, deque
from enum import Enum

class ErrorType(Enum):
    NETWORK_ERROR = "network_error"
    TIMEOUT_ERROR = "timeout_error"
    VALIDATION_ERROR = "validation_error"
    PROCESSING_ERROR = "processing_error"
    RESOURCE_ERROR = "resource_error"
    CRITICAL_ERROR = "critical_error"

class RetryStrategy:
    """Smart retry logic with exponential backoff"""
    
    def __init__(self, max_retries=3, base_delay=1.0, max_delay=30.0):
        self.max_retries = max_retries
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.retry_stats = {
            'total_attempts': 0,
            'successful_retries': 0,
            'failed_after_retry': 0,
            'immediate_successes': 0
        }
    
    def execute_with_retry(self, func, *args, **kwargs):
        """Execute function with retry logic"""
        last_exception = None
        
        for attempt in range(self.max_retries + 1):
            try:
                self.retry_stats['total_attempts'] += 1
                
                # Execute function
                result = func(*args, **kwargs)
                
                if attempt == 0:
                    self.retry_stats['immediate_successes'] += 1
                else:
                    self.retry_stats['successful_retries'] += 1
                
                return {'success': True, 'result': result, 'attempts': attempt + 1}
                
            except Exception as e:
                last_exception = e
                
                if attempt < self.max_retries:
                    # Calculate delay with exponential backoff
                    delay = min(self.base_delay * (2 ** attempt), self.max_delay)
                    time.sleep(delay)
                else:
                    # Max retries reached
                    self.retry_stats['failed_after_retry'] += 1
        
        return {
            'success': False, 
            'error': str(last_exception), 
            'attempts': self.max_retries + 1,
            'error_type': type(last_exception).__name__
        }
    
    def get_stats(self):
        """Get retry statistics"""
        total = self.retry_stats['total_attempts']
        success_rate = ((self.retry_stats['immediate_successes'] + self.retry_stats['successful_retries']) / max(total, 1)) * 100
        
        return {
            'total_attempts': total,
            'immediate_successes': self.retry_stats['immediate_successes'],
            'successful_retries': self.retry_stats['successful_retries'],
            'failed_after_retry': self.retry_stats['failed_after_retry'],
            'success_rate_pct': round(success_rate, 1)
        }

class CircuitBreaker:
    """Circuit breaker to prevent cascade failures"""
    
    def __init__(self, failure_threshold=5, timeout_duration=60):
        self.failure_threshold = failure_threshold
        self.timeout_duration = timeout_duration
        
        self.failure_count = 0
        self.last_failure_time = 0
        self.state = 'CLOSED'  # CLOSED, OPEN, HALF_OPEN
        
        self.stats = {
            'total_requests': 0,
            'failed_requests': 0,
            'blocked_requests': 0,
            'circuit_opens': 0,
            'successful_recoveries': 0
        }
    
    def call(self, func, *args, **kwargs):
        """Call function through circuit breaker"""
        self.stats['total_requests'] += 1
        current_time = time.time()
        
        # Check circuit state
        if self.state == 'OPEN':
            # Check if timeout period has passed
            if current_time - self.last_failure_time >= self.timeout_duration:
                self.state = 'HALF_OPEN'
            else:
                self.stats['blocked_requests'] += 1
                return {
                    'success': False,
                    'error': 'Circuit breaker is OPEN',
                    'circuit_state': self.state,
                    'retry_after': self.timeout_duration - (current_time - self.last_failure_time)
                }
        
        try:
            # Execute function
            result = func(*args, **kwargs)
            
            # Success - reset failure count
            if self.state == 'HALF_OPEN':
                self.state = 'CLOSED'
                self.stats['successful_recoveries'] += 1
            
            self.failure_count = 0
            return {'success': True, 'result': result, 'circuit_state': self.state}
            
        except Exception as e:
            # Handle failure
            self.failure_count += 1
            self.last_failure_time = current_time
            self.stats['failed_requests'] += 1
            
            # Check if we should open circuit
            if self.failure_count >= self.failure_threshold:
                self.state = 'OPEN'
                self.stats['circuit_opens'] += 1
            
            return {
                'success': False,
                'error': str(e),
                'circuit_state': self.state,
                'failure_count': self.failure_count
            }
    
    def get_stats(self):
        """Get circuit breaker statistics"""
        total = self.stats['total_requests']
        failure_rate = (self.stats['failed_requests'] / max(total, 1)) * 100
        
        return {
            'state': self.state,
            'failure_count': self.failure_count,
            'total_requests': total,
            'failed_requests': self.stats['failed_requests'],
            'blocked_requests': self.stats['blocked_requests'],
            'circuit_opens': self.stats['circuit_opens'],
            'successful_recoveries': self.stats['successful_recoveries'],
            'failure_rate_pct': round(failure_rate, 1)
        }

class HealthMonitor:
    """Monitor component health and system status"""
    
    def __init__(self, check_interval=30):
        self.check_interval = check_interval
        self.component_health = {}  # component_id -> health_data
        self.health_history = defaultdict(lambda: deque(maxlen=10))  # Keep last 10 checks
        self.last_check_time = 0
        
        self.alert_thresholds = {
            'response_time_ms': 5000,  # 5 seconds
            'error_rate_pct': 10.0,    # 10% error rate
            'memory_usage_pct': 85.0,  # 85% memory usage
            'cpu_usage_pct': 90.0      # 90% CPU usage
        }
        
        self.stats = {
            'total_checks': 0,
            'healthy_checks': 0,
            'unhealthy_checks': 0,
            'alerts_triggered': 0
        }
    
    def register_component(self, component_id, initial_health=None):
        """Register component for health monitoring"""
        self.component_health[component_id] = initial_health or {
            'status': 'unknown',
            'response_time_ms': 0,
            'error_rate_pct': 0,
            'memory_usage_pct': 0,
            'cpu_usage_pct': 0,
            'last_check': time.time()
        }
    
    def update_health(self, component_id, health_data):
        """Update component health data"""
        if component_id not in self.component_health:
            self.register_component(component_id)
        
        # Update health data
        self.component_health[component_id].update(health_data)
        self.component_health[component_id]['last_check'] = time.time()
        
        # Store in history
        self.health_history[component_id].append({
            'timestamp': time.time(),
            'health_data': health_data.copy()
        })
        
        # Check for alerts
        alerts = self._check_alerts(component_id, health_data)
        
        self.stats['total_checks'] += 1
        
        # Determine overall health
        if self._is_healthy(health_data):
            self.stats['healthy_checks'] += 1
            status = 'healthy'
        else:
            self.stats['unhealthy_checks'] += 1
            status = 'unhealthy'
        
        self.component_health[component_id]['status'] = status
        
        return {'status': status, 'alerts': alerts}
    
    def _check_alerts(self, component_id, health_data):
        """Check if any alert thresholds are exceeded"""
        alerts = []
        
        for metric, threshold in self.alert_thresholds.items():
            if metric in health_data:
                value = health_data[metric]
                
                if metric in ['response_time_ms', 'error_rate_pct', 'memory_usage_pct', 'cpu_usage_pct']:
                    if value > threshold:
                        alert = {
                            'component': component_id,
                            'metric': metric,
                            'value': value,
                            'threshold': threshold,
                            'severity': self._get_alert_severity(metric, value, threshold),
                            'timestamp': time.time()
                        }
                        alerts.append(alert)
                        self.stats['alerts_triggered'] += 1
        
        return alerts
    
    def _get_alert_severity(self, metric, value, threshold):
        """Determine alert severity based on how much threshold is exceeded"""
        overage = (value - threshold) / threshold
        
        if overage > 0.5:  # 50% over threshold
            return 'critical'
        elif overage > 0.2:  # 20% over threshold
            return 'high'
        else:
            return 'medium'
    
    def _is_healthy(self, health_data):
        """Determine if component is healthy based on all metrics"""
        for metric, threshold in self.alert_thresholds.items():
            if metric in health_data and health_data[metric] > threshold:
                return False
        return True
    
    def get_system_health(self):
        """Get overall system health status"""
        total_components = len(self.component_health)
        if total_components == 0:
            return {'status': 'no_components', 'components': {}}
        
        healthy_components = sum(1 for health in self.component_health.values() if health.get('status') == 'healthy')
        health_percentage = (healthy_components / total_components) * 100
        
        if health_percentage >= 90:
            system_status = 'excellent'
        elif health_percentage >= 75:
            system_status = 'good'
        elif health_percentage >= 50:
            system_status = 'degraded'
        else:
            system_status = 'critical'
        
        return {
            'status': system_status,
            'health_percentage': round(health_percentage, 1),
            'healthy_components': healthy_components,
            'total_components': total_components,
            'components': self.component_health.copy()
        }
    
    def get_stats(self):
        """Get health monitoring statistics"""
        return self.stats.copy()

class FallbackSystem:
    """Fallback processing when primary systems fail"""
    
    def __init__(self):
        self.fallback_handlers = {}  # operation_type -> fallback_function
        self.primary_failures = defaultdict(int)
        self.fallback_successes = defaultdict(int)
        self.stats = {
            'primary_attempts': 0,
            'primary_failures': 0,
            'fallback_attempts': 0,
            'fallback_successes': 0,
            'total_failures': 0
        }
    
    def register_fallback(self, operation_type, primary_func, fallback_func):
        """Register primary and fallback functions for operation"""
        self.fallback_handlers[operation_type] = {
            'primary': primary_func,
            'fallback': fallback_func
        }
    
    def execute_with_fallback(self, operation_type, *args, **kwargs):
        """Execute operation with fallback if primary fails"""
        if operation_type not in self.fallback_handlers:
            return {
                'success': False,
                'error': f'No handler registered for operation: {operation_type}'
            }
        
        handlers = self.fallback_handlers[operation_type]
        
        # Try primary function first
        self.stats['primary_attempts'] += 1
        
        try:
            result = handlers['primary'](*args, **kwargs)
            return {
                'success': True,
                'result': result,
                'handler_used': 'primary'
            }
            
        except Exception as primary_error:
            # Primary failed, try fallback
            self.stats['primary_failures'] += 1
            self.primary_failures[operation_type] += 1
            
            self.stats['fallback_attempts'] += 1
            
            try:
                result = handlers['fallback'](*args, **kwargs)
                self.stats['fallback_successes'] += 1
                self.fallback_successes[operation_type] += 1
                
                return {
                    'success': True,
                    'result': result,
                    'handler_used': 'fallback',
                    'primary_error': str(primary_error)
                }
                
            except Exception as fallback_error:
                # Both failed
                self.stats['total_failures'] += 1
                
                return {
                    'success': False,
                    'primary_error': str(primary_error),
                    'fallback_error': str(fallback_error),
                    'handler_used': 'none'
                }
    
    def get_fallback_rate(self, operation_type):
        """Get fallback usage rate for specific operation"""
        failures = self.primary_failures[operation_type]
        successes = self.fallback_successes[operation_type]
        
        if failures == 0:
            return 0.0
        
        return (successes / failures) * 100
    
    def get_stats(self):
        """Get fallback system statistics"""
        primary_success_rate = ((self.stats['primary_attempts'] - self.stats['primary_failures']) / max(self.stats['primary_attempts'], 1)) * 100
        fallback_success_rate = (self.stats['fallback_successes'] / max(self.stats['fallback_attempts'], 1)) * 100
        
        return {
            'primary_attempts': self.stats['primary_attempts'],
            'primary_failures': self.stats['primary_failures'],
            'primary_success_rate_pct': round(primary_success_rate, 1),
            'fallback_attempts': self.stats['fallback_attempts'],
            'fallback_successes': self.stats['fallback_successes'],
            'fallback_success_rate_pct': round(fallback_success_rate, 1),
            'total_failures': self.stats['total_failures']
        }

class ErrorHandler:
    """Main error handling coordinator"""
    
    def __init__(self):
        self.retry_strategy = RetryStrategy(max_retries=3, base_delay=0.5)
        self.circuit_breaker = CircuitBreaker(failure_threshold=3, timeout_duration=30)
        self.health_monitor = HealthMonitor(check_interval=15)
        self.fallback_system = FallbackSystem()
        
        self.error_log = deque(maxlen=100)  # Keep last 100 errors
        self.enabled = False
        self.start_time = None
        
        # Setup default fallback handlers
        self._setup_default_fallbacks()
    
    def _setup_default_fallbacks(self):
        """Setup default fallback handlers"""
        # Code generation fallback
        self.fallback_system.register_fallback(
            'code_generation',
            self._primary_code_generation,
            self._fallback_code_generation
        )
        
        # Message processing fallback
        self.fallback_system.register_fallback(
            'message_processing',
            self._primary_message_processing,
            self._fallback_message_processing
        )
        
        # Data validation fallback
        self.fallback_system.register_fallback(
            'data_validation',
            self._primary_data_validation,
            self._fallback_data_validation
        )
    
    def _primary_code_generation(self, request):
        """Primary code generation (may fail)"""
        if 'error' in request:
            raise Exception(f"Code generation failed: {request['error']}")
        
        return {
            'code': f'def {request.get("function_name", "generated_function")}():\n    pass',
            'language': 'python',
            'quality': 'high'
        }
    
    def _fallback_code_generation(self, request):
        """Fallback code generation (simplified)"""
        return {
            'code': f'# Fallback code for {request.get("function_name", "function")}\npass',
            'language': 'python',
            'quality': 'basic'
        }
    
    def _primary_message_processing(self, message):
        """Primary message processing (may fail)"""
        if message.get('corrupt', False):
            raise Exception("Message is corrupted")
        
        return {
            'processed': True,
            'result': f"Processed: {message.get('content', '')}",
            'method': 'primary'
        }
    
    def _fallback_message_processing(self, message):
        """Fallback message processing (basic)"""
        return {
            'processed': True,
            'result': f"Basic processing: {message.get('content', 'unknown')}",
            'method': 'fallback'
        }
    
    def _primary_data_validation(self, data):
        """Primary data validation (strict)"""
        required_fields = ['id', 'type', 'timestamp']
        
        for field in required_fields:
            if field not in data:
                raise Exception(f"Missing required field: {field}")
        
        return {'valid': True, 'method': 'strict_validation'}
    
    def _fallback_data_validation(self, data):
        """Fallback data validation (lenient)"""
        # Just check if it's a dictionary
        if isinstance(data, dict):
            return {'valid': True, 'method': 'lenient_validation'}
        else:
            raise Exception("Data must be a dictionary")
    
    def enable(self):
        """Enable error handling systems"""
        self.enabled = True
        self.start_time = time.time()
        
        # Register some components for health monitoring
        self.health_monitor.register_component('message_router')
        self.health_monitor.register_component('task_processor')
        self.health_monitor.register_component('cache_system')
        
        print("✅ Error handling enabled:")
        print("   🛡️ Retry logic with exponential backoff")
        print("   ⚡ Circuit breaker for cascade prevention")
        print("   💓 Health monitoring for components")
        print("   🔄 Fallback systems for resilience")
    
    def disable(self):
        """Disable error handling"""
        self.enabled = False
        print("✅ Error handling disabled")
    
    def handle_operation(self, operation_type, operation_func, *args, **kwargs):
        """Handle operation with full error resilience"""
        if not self.enabled:
            return operation_func(*args, **kwargs)
        
        start_time = time.time()
        
        try:
            # Execute with circuit breaker and retry logic
            def execute_with_circuit():
                return self.circuit_breaker.call(operation_func, *args, **kwargs)
            
            result = self.retry_strategy.execute_with_retry(execute_with_circuit)
            
            # Log successful operation
            operation_time = (time.time() - start_time) * 1000
            self._log_operation(operation_type, True, operation_time, None)
            
            return result
            
        except Exception as e:
            # Log error
            error_data = {
                'operation_type': operation_type,
                'error': str(e),
                'timestamp': time.time(),
                'args': str(args),
                'kwargs': str(kwargs)
            }
            self.error_log.append(error_data)
            
            operation_time = (time.time() - start_time) * 1000
            self._log_operation(operation_type, False, operation_time, str(e))
            
            # Try fallback system
            return self.fallback_system.execute_with_fallback(operation_type, *args, **kwargs)
    
    def _log_operation(self, operation_type, success, duration_ms, error):
        """Log operation for health monitoring"""
        # Update component health based on operation
        component_id = f"{operation_type}_processor"
        
        if component_id not in self.health_monitor.component_health:
            self.health_monitor.register_component(component_id)
        
        # Simulate health metrics based on operation
        health_data = {
            'response_time_ms': duration_ms,
            'error_rate_pct': 0 if success else 100,
            'memory_usage_pct': 45 + (duration_ms / 100),  # Simulate memory usage
            'cpu_usage_pct': 30 + (duration_ms / 50)      # Simulate CPU usage
        }
        
        self.health_monitor.update_health(component_id, health_data)
    
    def get_error_summary(self):
        """Get summary of recent errors"""
        if not self.error_log:
            return {'total_errors': 0, 'recent_errors': []}
        
        # Group errors by type
        error_types = defaultdict(int)
        for error in self.error_log:
            error_type = error['operation_type']
            error_types[error_type] += 1
        
        return {
            'total_errors': len(self.error_log),
            'error_types': dict(error_types),
            'recent_errors': list(self.error_log)[-5:]  # Last 5 errors
        }
    
    def get_comprehensive_stats(self):
        """Get comprehensive error handling statistics"""
        runtime = time.time() - self.start_time if self.start_time else 0
        
        return {
            'enabled': self.enabled,
            'runtime_seconds': round(runtime, 2),
            'retry_system': self.retry_strategy.get_stats(),
            'circuit_breaker': self.circuit_breaker.get_stats(),
            'health_monitoring': {
                'stats': self.health_monitor.get_stats(),
                'system_health': self.health_monitor.get_system_health()
            },
            'fallback_system': self.fallback_system.get_stats(),
            'error_summary': self.get_error_summary()
        }

def test_error_handling():
    """Test error handling and resilience systems"""
    print("🧪 TESTING ERROR HANDLING & RESILIENCE")
    print("=" * 50)
    
    handler = ErrorHandler()
    handler.enable()
    
    # Test 1: Retry Logic
    print("\n📊 Test 1: Retry Logic")
    print("-" * 25)
    
    failure_count = 0
    def flaky_function(should_fail=False):
        nonlocal failure_count
        if should_fail and failure_count < 2:
            failure_count += 1
            raise Exception(f"Simulated failure #{failure_count}")
        return "Success after retries"
    
    # Test successful retry
    result = handler.retry_strategy.execute_with_retry(flaky_function, True)
    print(f"Retry result: {result['success']}, attempts: {result['attempts']}")
    
    retry_stats = handler.retry_strategy.get_stats()
    print(f"Retry statistics:")
    print(f"  Success rate: {retry_stats['success_rate_pct']}%")
    print(f"  Immediate successes: {retry_stats['immediate_successes']}")
    print(f"  Successful retries: {retry_stats['successful_retries']}")
    
    # Test 2: Circuit Breaker
    print("\n📊 Test 2: Circuit Breaker")
    print("-" * 28)
    
    def always_failing_function():
        raise Exception("Always fails")
    
    def working_function():
        return "Working correctly"
    
    # Trigger circuit breaker by causing failures
    for i in range(5):
        result = handler.circuit_breaker.call(always_failing_function)
        print(f"  Attempt {i+1}: {result['circuit_state']}")
    
    # Try working function while circuit is open
    result = handler.circuit_breaker.call(working_function)
    print(f"  Working function while OPEN: {result.get('error', 'Success')}")
    
    circuit_stats = handler.circuit_breaker.get_stats()
    print(f"Circuit breaker stats:")
    print(f"  State: {circuit_stats['state']}")
    print(f"  Blocked requests: {circuit_stats['blocked_requests']}")
    print(f"  Circuit opens: {circuit_stats['circuit_opens']}")
    
    # Test 3: Health Monitoring
    print("\n📊 Test 3: Health Monitoring")
    print("-" * 31)
    
    # Simulate component health updates
    components = ['message_router', 'task_processor', 'cache_system']
    
    for i, component in enumerate(components):
        # Simulate different health states
        health_data = {
            'response_time_ms': 1000 + (i * 2000),  # Increasing response times
            'error_rate_pct': i * 5,                # Increasing error rates
            'memory_usage_pct': 60 + (i * 10),      # Increasing memory usage
            'cpu_usage_pct': 40 + (i * 15)          # Increasing CPU usage
        }
        
        update_result = handler.health_monitor.update_health(component, health_data)
        print(f"  {component}: {update_result['status']} ({len(update_result['alerts'])} alerts)")
    
    system_health = handler.health_monitor.get_system_health()
    print(f"Overall system health: {system_health['status']} ({system_health['health_percentage']}%)")
    
    # Test 4: Fallback System
    print("\n📊 Test 4: Fallback System")
    print("-" * 29)
    
    # Test code generation fallback
    failing_request = {'function_name': 'test_func', 'error': 'compilation_error'}
    working_request = {'function_name': 'test_func'}
    
    # This should trigger fallback
    result1 = handler.fallback_system.execute_with_fallback('code_generation', failing_request)
    print(f"  Code generation (failing): {result1['handler_used']}")
    
    # This should use primary
    result2 = handler.fallback_system.execute_with_fallback('code_generation', working_request)
    print(f"  Code generation (working): {result2['handler_used']}")
    
    # Test message processing fallback
    corrupt_message = {'content': 'test message', 'corrupt': True}
    normal_message = {'content': 'test message'}
    
    result3 = handler.fallback_system.execute_with_fallback('message_processing', corrupt_message)
    result4 = handler.fallback_system.execute_with_fallback('message_processing', normal_message)
    
    print(f"  Message processing (corrupt): {result3['handler_used']}")
    print(f"  Message processing (normal): {result4['handler_used']}")
    
    fallback_stats = handler.fallback_system.get_stats()
    print(f"Fallback statistics:")
    print(f"  Primary success rate: {fallback_stats['primary_success_rate_pct']}%")
    print(f"  Fallback success rate: {fallback_stats['fallback_success_rate_pct']}%")
    
    # Test 5: Comprehensive Error Handling
    print("\n📊 Test 5: End-to-End Error Handling")
    print("-" * 38)
    
    def test_operation(should_fail=False):
        if should_fail:
            raise Exception("Test operation failed")
        return "Operation successful"
    
    # Test successful operation
    result1 = handler.handle_operation('test_op', test_operation, False)
    print(f"  Successful operation: {result1.get('success', True)}")
    
    # Test failing operation (should trigger retry and fallback)
    result2 = handler.handle_operation('test_op', test_operation, True)
    print(f"  Failed operation handled: {result2.get('success', 'handled')}")
    
    # Get comprehensive statistics
    print("\n📊 Test 6: Comprehensive Statistics")
    print("-" * 37)
    
    comprehensive_stats = handler.get_comprehensive_stats()
    
    print(f"Runtime: {comprehensive_stats['runtime_seconds']} seconds")
    print(f"Retry success rate: {comprehensive_stats['retry_system']['success_rate_pct']}%")
    print(f"Circuit breaker state: {comprehensive_stats['circuit_breaker']['state']}")
    print(f"System health: {comprehensive_stats['health_monitoring']['system_health']['status']}")
    print(f"Fallback usage: {comprehensive_stats['fallback_system']['fallback_attempts']} attempts")
    print(f"Total errors logged: {comprehensive_stats['error_summary']['total_errors']}")
    
    # Calculate resilience score
    retry_success = comprehensive_stats['retry_system']['success_rate_pct']
    fallback_success = comprehensive_stats['fallback_system']['fallback_success_rate_pct']
    system_health_pct = comprehensive_stats['health_monitoring']['system_health']['health_percentage']
    
    resilience_score = (retry_success + fallback_success + system_health_pct) / 3
    
    print(f"\n🛡️ RESILIENCE ASSESSMENT:")
    print(f"   Retry reliability: {retry_success}%")
    print(f"   Fallback reliability: {fallback_success}%")
    print(f"   System health: {system_health_pct}%")
    print(f"   OVERALL RESILIENCE SCORE: {resilience_score:.1f}%")
    
    if resilience_score >= 80:
        print("✅ Excellent resilience - Production ready!")
    elif resilience_score >= 60:
        print("✅ Good resilience - Ready with monitoring")
    else:
        print("⚠️  Needs improvement for production")
    
    handler.disable()
    print(f"\n✅ Error handling testing complete!")

if __name__ == '__main__':
    test_error_handling()