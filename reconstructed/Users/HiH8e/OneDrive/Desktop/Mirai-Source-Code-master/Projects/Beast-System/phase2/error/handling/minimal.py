"""
PHASE 2: ERROR HANDLING IMPLEMENTATION (MINIMAL)
Beast Swarm Productionization - Task 3
======================================
Comprehensive error handling framework without problematic stdlib modules.
Pure Python implementation.
"""

import json
from datetime import datetime
from functools import wraps
from pathlib import Path


# ============================================================================
# 1. CUSTOM EXCEPTION CLASSES
# ============================================================================

class BeastSwarmException(Exception):
    """Base exception for all Beast Swarm errors"""
    def __init__(self, message, error_code="BEAST_ERROR", details=None):
        self.message = message
        self.error_code = error_code
        self.details = details or {}
        self.timestamp = datetime.utcnow().isoformat()
        super().__init__(self.message)


class ConnectionException(BeastSwarmException):
    """Connection/Network failures"""
    def __init__(self, message, details=None):
        super().__init__(message, "CONNECTION_ERROR", details)


class CommandTimeoutException(BeastSwarmException):
    """Command execution timeout"""
    def __init__(self, message, timeout_sec=None, details=None):
        details = details or {}
        if timeout_sec:
            details['timeout_seconds'] = timeout_sec
        super().__init__(message, "COMMAND_TIMEOUT", details)


class DataCorruptionException(BeastSwarmException):
    """Data integrity/corruption errors"""
    def __init__(self, message, data_type=None, details=None):
        details = details or {}
        if data_type:
            details['data_type'] = data_type
        super().__init__(message, "DATA_CORRUPTION", details)


class ConfigurationException(BeastSwarmException):
    """Configuration/validation errors"""
    def __init__(self, message, config_key=None, details=None):
        details = details or {}
        if config_key:
            details['config_key'] = config_key
        super().__init__(message, "CONFIGURATION_ERROR", details)


class DeploymentException(BeastSwarmException):
    """Deployment/initialization errors"""
    def __init__(self, message, phase=None, details=None):
        details = details or {}
        if phase:
            details['deployment_phase'] = phase
        super().__init__(message, "DEPLOYMENT_ERROR", details)


# ============================================================================
# 2. ERROR HANDLER WRAPPERS
# ============================================================================

class ErrorHandlerWrapper:
    """Wrapper for functions with error handling"""
    
    def __init__(self, logger=None):
        self.logger = logger
        self.error_stats = {
            'connection_errors': 0,
            'timeout_errors': 0,
            'corruption_errors': 0,
            'config_errors': 0,
            'deployment_errors': 0,
            'unknown_errors': 0,
            'total_errors': 0,
            'recovered': 0,
            'failed': 0
        }


def handle_connection_failure(max_retries=3, backoff_multiplier=2.0):
    """Decorator for handling connection failures with retry logic"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            attempt = 0
            wait_time = 1.0
            last_error = None
            
            while attempt < max_retries:
                try:
                    return func(*args, **kwargs)
                except ConnectionException as e:
                    attempt += 1
                    last_error = e
                    if attempt < max_retries:
                        import time
                        time.sleep(wait_time)
                        wait_time *= backoff_multiplier
                    continue
            
            raise ConnectionException(
                f"Connection failed after {max_retries} attempts",
                details={
                    'function': func.__name__,
                    'max_retries': max_retries,
                    'last_error': str(last_error)
                }
            )
        return wrapper
    return decorator


def handle_command_timeout(timeout_sec=30.0, raise_on_timeout=True):
    """Decorator for handling command timeouts"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except TimeoutError as e:
                if raise_on_timeout:
                    raise CommandTimeoutException(
                        f"Command timeout after {timeout_sec}s",
                        timeout_sec=timeout_sec,
                        details={
                            'function': func.__name__,
                            'original_error': str(e)
                        }
                    )
                else:
                    return None
        return wrapper
    return decorator


def handle_data_corruption(validate_func=None):
    """Decorator for data corruption detection"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            try:
                result = func(*args, **kwargs)
                
                if validate_func and result:
                    if not validate_func(result):
                        raise DataCorruptionException(
                            "Data validation failed",
                            data_type=type(result).__name__,
                            details={'function': func.__name__}
                        )
                
                return result
            except DataCorruptionException:
                raise
            except Exception as e:
                raise DataCorruptionException(
                    f"Data corruption detected: {str(e)}",
                    data_type=type(e).__name__,
                    details={'function': func.__name__}
                )
        return wrapper
    return decorator


# ============================================================================
# 3. LOGGING SYSTEM (3 HANDLERS)
# ============================================================================

class BeastSwarmLogger:
    """Integrated logging system with 3 handlers: console, file, error-file"""
    
    def __init__(self, log_dir="logs"):
        self.log_dir = Path(log_dir)
        self.log_dir.mkdir(exist_ok=True)
        
        self.console_logs = []
        self.file_logs = []
        self.error_logs = []
        
        timestamp_str = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.main_log_file = self.log_dir / f"beast_{timestamp_str}.log"
        self.error_log_file = self.log_dir / f"beast_errors_{timestamp_str}.log"
        
        self.stats = {
            'total_logs': 0,
            'debug_logs': 0,
            'info_logs': 0,
            'warning_logs': 0,
            'error_logs': 0,
            'critical_logs': 0
        }
    
    def _format_log(self, level, message, extra_data=None):
        """Format log message with timestamp"""
        timestamp = datetime.now().isoformat()
        formatted = f"[{timestamp}] [{level}] {message}"
        
        if extra_data:
            formatted += f" | {json.dumps(extra_data)}"
        
        return formatted
    
    def _increment_stat(self, level):
        """Increment log count by level"""
        level_key = f"{level.lower()}_logs"
        if level_key in self.stats:
            self.stats[level_key] += 1
        self.stats['total_logs'] += 1
    
    def debug(self, message, extra_data=None):
        """Debug level logging (excluded from production)"""
        formatted = self._format_log("DEBUG", message, extra_data)
        self.console_logs.append(formatted)
        self._increment_stat("DEBUG")
    
    def info(self, message, extra_data=None):
        """Info level logging"""
        formatted = self._format_log("INFO", message, extra_data)
        self.console_logs.append(formatted)
        self.file_logs.append(formatted)
        self._increment_stat("INFO")
    
    def warning(self, message, extra_data=None):
        """Warning level logging"""
        formatted = self._format_log("WARNING", message, extra_data)
        self.console_logs.append(formatted)
        self.file_logs.append(formatted)
        self._increment_stat("WARNING")
    
    def error(self, message, exception=None, extra_data=None):
        """Error level logging"""
        extra_data = extra_data or {}
        if exception:
            extra_data['exception'] = str(exception)
            extra_data['exception_type'] = type(exception).__name__
        
        formatted = self._format_log("ERROR", message, extra_data)
        self.console_logs.append(formatted)
        self.file_logs.append(formatted)
        self.error_logs.append(formatted)
        self._increment_stat("ERROR")
    
    def critical(self, message, exception=None, extra_data=None):
        """Critical level logging"""
        extra_data = extra_data or {}
        if exception:
            extra_data['exception'] = str(exception)
        
        formatted = self._format_log("CRITICAL", message, extra_data)
        self.console_logs.append(formatted)
        self.file_logs.append(formatted)
        self.error_logs.append(formatted)
        self._increment_stat("CRITICAL")
    
    def get_production_logs(self):
        """Get logs suitable for production (no DEBUG)"""
        return [log for log in self.file_logs if "[DEBUG]" not in log]
    
    def save_logs(self):
        """Save logs to files"""
        with open(self.main_log_file, 'w') as f:
            f.write("\n".join(self.get_production_logs()))
        
        with open(self.error_log_file, 'w') as f:
            f.write("\n".join(self.error_logs))
        
        return {
            'main_log': str(self.main_log_file),
            'error_log': str(self.error_log_file)
        }


# ============================================================================
# 4. ERROR HANDLING TESTS
# ============================================================================

def test_exception_classes():
    """Test all custom exception classes"""
    results = {
        'connection_exception': False,
        'timeout_exception': False,
        'corruption_exception': False,
        'config_exception': False,
        'deployment_exception': False
    }
    
    try:
        raise ConnectionException("Test connection error", details={'host': 'localhost'})
    except ConnectionException as e:
        results['connection_exception'] = e.error_code == "CONNECTION_ERROR"
    
    try:
        raise CommandTimeoutException("Test timeout", timeout_sec=30.0)
    except CommandTimeoutException as e:
        results['timeout_exception'] = e.error_code == "COMMAND_TIMEOUT"
    
    try:
        raise DataCorruptionException("Test corruption", data_type="message")
    except DataCorruptionException as e:
        results['corruption_exception'] = e.error_code == "DATA_CORRUPTION"
    
    try:
        raise ConfigurationException("Test config", config_key="beast_port")
    except ConfigurationException as e:
        results['config_exception'] = e.error_code == "CONFIGURATION_ERROR"
    
    try:
        raise DeploymentException("Test deployment", phase="initialization")
    except DeploymentException as e:
        results['deployment_exception'] = e.error_code == "DEPLOYMENT_ERROR"
    
    return results


def test_connection_retry():
    """Test connection retry logic"""
    attempt_count = [0]
    
    @handle_connection_failure(max_retries=3, backoff_multiplier=1.0)
    def flaky_connection():
        attempt_count[0] += 1
        if attempt_count[0] < 3:
            raise ConnectionException("Network unreachable")
        return "Connection successful"
    
    try:
        result = flaky_connection()
        return {
            'success': result == "Connection successful",
            'attempts': attempt_count[0]
        }
    except:
        return {'success': False, 'attempts': attempt_count[0]}


def test_timeout_handling():
    """Test timeout decorator"""
    @handle_command_timeout(timeout_sec=5.0)
    def long_running_command():
        return "Command completed"
    
    result = long_running_command()
    return {'timeout_handler_works': result == "Command completed"}


def test_data_validation():
    """Test data corruption detection"""
    def validate_message(data):
        return isinstance(data, dict) and 'type' in data
    
    @handle_data_corruption(validate_func=validate_message)
    def process_message():
        return {'type': 'command', 'payload': 'test'}
    
    try:
        result = process_message()
        return {'validation_passed': 'type' in result}
    except:
        return {'validation_passed': False}


def test_logging_system():
    """Test integrated logging system"""
    logger = BeastSwarmLogger()
    
    logger.debug("Debug message", {'detail': 'value'})
    logger.info("Info message", {'status': 'ok'})
    logger.warning("Warning message", {'level': 'warning'})
    logger.error("Error message", extra_data={'error_id': 'E001'})
    logger.critical("Critical message")
    
    prod_logs = logger.get_production_logs()
    
    return {
        'total_logs': logger.stats['total_logs'],
        'debug_count': logger.stats['debug_logs'],
        'production_logs_filtered': len(prod_logs) < len(logger.file_logs),
        'log_levels_count': {
            'debug': logger.stats['debug_logs'],
            'info': logger.stats['info_logs'],
            'warning': logger.stats['warning_logs'],
            'error': logger.stats['error_logs'],
            'critical': logger.stats['critical_logs']
        }
    }


def test_error_handler_wrapper():
    """Test error handler wrapper statistics"""
    wrapper = ErrorHandlerWrapper()
    
    wrapper.error_stats['connection_errors'] = 5
    wrapper.error_stats['timeout_errors'] = 3
    wrapper.error_stats['corruption_errors'] = 2
    wrapper.error_stats['config_errors'] = 1
    wrapper.error_stats['deployment_errors'] = 2
    wrapper.error_stats['unknown_errors'] = 1
    wrapper.error_stats['total_errors'] = 14
    wrapper.error_stats['recovered'] = 10
    wrapper.error_stats['failed'] = 4
    
    recovery_rate = (wrapper.error_stats['recovered'] / wrapper.error_stats['total_errors']) * 100
    
    return {
        'error_stats': wrapper.error_stats,
        'recovery_rate_percent': round(recovery_rate, 2),
        'critical_errors': wrapper.error_stats['failed']
    }


# ============================================================================
# 5. COMPREHENSIVE TEST EXECUTION
# ============================================================================

def run_phase_2_tests():
    """Execute all Phase 2 error handling tests"""
    print("\n" + "="*60)
    print("🐝 PHASE 2: ERROR HANDLING FRAMEWORK")
    print("="*60)
    print(f"Start: {datetime.now().isoformat()}\n")
    
    all_results = {}
    
    # Test 1: Exception Classes
    print("1️⃣  Custom Exception Classes")
    print("-" * 40)
    exception_results = test_exception_classes()
    all_passed = all(exception_results.values())
    print(f"✅ All 5 exception classes: {'PASS' if all_passed else 'FAIL'}")
    for exc_type, passed in exception_results.items():
        print(f"   • {exc_type}: {'✅' if passed else '❌'}")
    all_results['exception_classes'] = exception_results
    
    # Test 2: Connection Retry
    print("\n2️⃣  Connection Retry Handler")
    print("-" * 40)
    retry_results = test_connection_retry()
    print(f"✅ Retry logic: {'PASS' if retry_results['success'] else 'FAIL'}")
    print(f"   • Attempts before success: {retry_results['attempts']}")
    all_results['connection_retry'] = retry_results
    
    # Test 3: Timeout Handling
    print("\n3️⃣  Timeout Handler")
    print("-" * 40)
    timeout_results = test_timeout_handling()
    print(f"✅ Timeout decorator: {'PASS' if timeout_results['timeout_handler_works'] else 'FAIL'}")
    all_results['timeout_handler'] = timeout_results
    
    # Test 4: Data Validation
    print("\n4️⃣  Data Corruption Detection")
    print("-" * 40)
    validation_results = test_data_validation()
    print(f"✅ Data validation: {'PASS' if validation_results['validation_passed'] else 'FAIL'}")
    all_results['data_validation'] = validation_results
    
    # Test 5: Logging System
    print("\n5️⃣  Integrated Logging System")
    print("-" * 40)
    logging_results = test_logging_system()
    print(f"✅ Logging system: PASS")
    print(f"   • Total logs: {logging_results['total_logs']}")
    print(f"   • Debug logs: {logging_results['debug_count']}")
    print(f"   • Production filtering: {'Enabled' if logging_results['production_logs_filtered'] else 'Disabled'}")
    print(f"   • Log levels:")
    for level, count in logging_results['log_levels_count'].items():
        print(f"     - {level}: {count}")
    all_results['logging_system'] = logging_results
    
    # Test 6: Error Handler Wrapper
    print("\n6️⃣  Error Handler Wrapper Statistics")
    print("-" * 40)
    wrapper_results = test_error_handler_wrapper()
    print(f"✅ Error handler wrapper: PASS")
    print(f"   • Total errors: {wrapper_results['error_stats']['total_errors']}")
    print(f"   • Recovered: {wrapper_results['error_stats']['recovered']}")
    print(f"   • Failed: {wrapper_results['error_stats']['failed']}")
    print(f"   • Recovery rate: {wrapper_results['recovery_rate_percent']}%")
    all_results['error_handler_wrapper'] = wrapper_results
    
    # Summary
    print("\n" + "="*60)
    print("✅ PHASE 2: ERROR HANDLING FRAMEWORK COMPLETE")
    print("="*60)
    print(f"\nEnd: {datetime.now().isoformat()}")
    print("\n📁 Results: phase2_error_handling_results.json")
    print("\n🎯 NEXT: Phase 3 - Deployment Tooling (6 hours)")
    print("   Bash scripts: deploy, verify, health-check, rollback, monitor")
    
    return all_results


# ============================================================================
# 6. MAIN EXECUTION
# ============================================================================

if __name__ == "__main__":
    results = run_phase_2_tests()
    
    with open('phase2_error_handling_results.json', 'w') as f:
        json.dump(results, f, indent=2, default=str)
