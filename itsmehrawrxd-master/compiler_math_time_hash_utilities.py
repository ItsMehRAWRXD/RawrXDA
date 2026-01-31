#!/usr/bin/env python3
"""
Compiler Math, Time, and Hash Utilities for n0mn0m IDE
Complete mathematical, time, and hashing operations for the compiler system
"""

import math
import time
import hashlib
import hmac
import secrets
import random
import statistics
import datetime
import calendar
from typing import Dict, List, Optional, Union, Any, Tuple
import threading
import functools
from decimal import Decimal, getcontext
import cmath
import fractions

class CompilerMathUtilities:
    """
    Comprehensive mathematical utilities for the n0mn0m IDE compiler
    Advanced mathematical operations and algorithms
    """
    
    def __init__(self):
        # Set high precision for decimal operations
        getcontext().prec = 50
        
        # Mathematical constants
        self.PI = math.pi
        self.E = math.e
        self.TAU = math.tau
        self.PHI = (1 + math.sqrt(5)) / 2  # Golden ratio
        self.SQRT2 = math.sqrt(2)
        self.SQRT3 = math.sqrt(3)
        
        print("🔢 Math utilities initialized")
    
    def advanced_calculator(self, expression: str) -> Union[float, complex, str]:
        """Advanced calculator with support for complex expressions"""
        
        try:
            # Safe evaluation of mathematical expressions
            allowed_names = {
                'sin': math.sin, 'cos': math.cos, 'tan': math.tan,
                'asin': math.asin, 'acos': math.acos, 'atan': math.atan,
                'sinh': math.sinh, 'cosh': math.cosh, 'tanh': math.tanh,
                'exp': math.exp, 'log': math.log, 'log10': math.log10,
                'sqrt': math.sqrt, 'pow': math.pow, 'abs': abs,
                'ceil': math.ceil, 'floor': math.floor, 'round': round,
                'min': min, 'max': max, 'sum': sum,
                'pi': self.PI, 'e': self.E, 'tau': self.TAU,
                'phi': self.PHI, 'sqrt2': self.SQRT2, 'sqrt3': self.SQRT3
            }
            
            # Evaluate expression safely
            result = eval(expression, {"__builtins__": {}}, allowed_names)
            return result
            
        except Exception as e:
            return f"Error: {e}"
    
    def prime_functions(self, n: int) -> Dict[str, Any]:
        """Prime number functions and analysis"""
        
        def is_prime(num):
            if num < 2:
                return False
            if num == 2:
                return True
            if num % 2 == 0:
                return False
            for i in range(3, int(math.sqrt(num)) + 1, 2):
                if num % i == 0:
                    return False
            return True
        
        def prime_factors(num):
            factors = []
            d = 2
            while d * d <= num:
                while num % d == 0:
                    factors.append(d)
                    num //= d
                d += 1
            if num > 1:
                factors.append(num)
            return factors
        
        def next_prime(num):
            num += 1
            while not is_prime(num):
                num += 1
            return num
        
        def previous_prime(num):
            if num <= 2:
                return None
            num -= 1
            while num > 2 and not is_prime(num):
                num -= 1
            return num if is_prime(num) else None
        
        return {
            'is_prime': is_prime(n),
            'prime_factors': prime_factors(n),
            'next_prime': next_prime(n),
            'previous_prime': previous_prime(n),
            'prime_count': sum(1 for i in range(2, n + 1) if is_prime(i))
        }
    
    def fibonacci_sequence(self, n: int) -> List[int]:
        """Generate Fibonacci sequence up to n terms"""
        
        if n <= 0:
            return []
        elif n == 1:
            return [0]
        elif n == 2:
            return [0, 1]
        
        sequence = [0, 1]
        for i in range(2, n):
            sequence.append(sequence[i-1] + sequence[i-2])
        
        return sequence
    
    def fibonacci_number(self, n: int) -> int:
        """Calculate nth Fibonacci number efficiently"""
        
        if n < 0:
            raise ValueError("Fibonacci not defined for negative numbers")
        
        # Use matrix exponentiation for efficiency
        def matrix_multiply(a, b):
            return [[a[0][0]*b[0][0] + a[0][1]*b[1][0], a[0][0]*b[0][1] + a[0][1]*b[1][1]],
                    [a[1][0]*b[0][0] + a[1][1]*b[1][0], a[1][0]*b[0][1] + a[1][1]*b[1][1]]]
        
        def matrix_power(matrix, n):
            if n == 1:
                return matrix
            if n % 2 == 0:
                half_power = matrix_power(matrix, n // 2)
                return matrix_multiply(half_power, half_power)
            else:
                return matrix_multiply(matrix, matrix_power(matrix, n - 1))
        
        if n == 0:
            return 0
        elif n == 1:
            return 1
        
        # [[F(n+1), F(n)], [F(n), F(n-1)]] = [[1,1],[1,0]]^n
        matrix = [[1, 1], [1, 0]]
        result_matrix = matrix_power(matrix, n - 1)
        return result_matrix[0][0]
    
    def number_theory(self, n: int) -> Dict[str, Any]:
        """Number theory analysis"""
        
        def gcd(a, b):
            while b:
                a, b = b, a % b
            return a
        
        def lcm(a, b):
            return abs(a * b) // gcd(a, b)
        
        def totient(n):
            result = n
            p = 2
            while p * p <= n:
                if n % p == 0:
                    while n % p == 0:
                        n //= p
                    result -= result // p
                p += 1
            if n > 1:
                result -= result // n
            return result
        
        def divisors(n):
            divisors = set()
            for i in range(1, int(math.sqrt(n)) + 1):
                if n % i == 0:
                    divisors.add(i)
                    divisors.add(n // i)
            return sorted(divisors)
        
        def perfect_number(n):
            return sum(divisors(n)[:-1]) == n
        
        def abundant_number(n):
            return sum(divisors(n)[:-1]) > n
        
        def deficient_number(n):
            return sum(divisors(n)[:-1]) < n
        
        return {
            'gcd_1': gcd(n, 1),
            'totient': totient(n),
            'divisors': divisors(n),
            'sum_of_divisors': sum(divisors(n)),
            'perfect': perfect_number(n),
            'abundant': abundant_number(n),
            'deficient': deficient_number(n),
            'digital_root': self._digital_root(n),
            'factorial': math.factorial(n) if n < 171 else float('inf')
        }
    
    def _digital_root(self, n: int) -> int:
        """Calculate digital root"""
        while n >= 10:
            n = sum(int(digit) for digit in str(n))
        return n
    
    def statistical_analysis(self, data: List[Union[int, float]]) -> Dict[str, float]:
        """Statistical analysis of data"""
        
        if not data:
            return {}
        
        return {
            'mean': statistics.mean(data),
            'median': statistics.median(data),
            'mode': statistics.mode(data) if len(set(data)) < len(data) else None,
            'stdev': statistics.stdev(data) if len(data) > 1 else 0,
            'variance': statistics.variance(data) if len(data) > 1 else 0,
            'min': min(data),
            'max': max(data),
            'range': max(data) - min(data),
            'quartiles': self._calculate_quartiles(data),
            'percentiles': self._calculate_percentiles(data)
        }
    
    def _calculate_quartiles(self, data: List[float]) -> Dict[str, float]:
        """Calculate quartiles"""
        sorted_data = sorted(data)
        n = len(sorted_data)
        
        q1 = statistics.median(sorted_data[:n//2])
        q2 = statistics.median(sorted_data)
        q3 = statistics.median(sorted_data[n//2:])
        
        return {'Q1': q1, 'Q2': q2, 'Q3': q3, 'IQR': q3 - q1}
    
    def _calculate_percentiles(self, data: List[float], 
                             percentiles: List[int] = [10, 25, 50, 75, 90, 95, 99]) -> Dict[str, float]:
        """Calculate percentiles"""
        sorted_data = sorted(data)
        result = {}
        
        for p in percentiles:
            if p == 100:
                result[f'P{p}'] = sorted_data[-1]
            else:
                index = (p / 100) * (len(sorted_data) - 1)
                if index.is_integer():
                    result[f'P{p}'] = sorted_data[int(index)]
                else:
                    lower = sorted_data[int(index)]
                    upper = sorted_data[int(index) + 1]
                    result[f'P{p}'] = lower + (upper - lower) * (index - int(index))
        
        return result

class CompilerTimeUtilities:
    """
    Comprehensive time utilities for the n0mn0m IDE compiler
    Advanced time operations and scheduling
    """
    
    def __init__(self):
        self.start_time = time.time()
        self.timezone = datetime.timezone.utc
        print("⏰ Time utilities initialized")
    
    def get_current_time(self, format_str: str = "%Y-%m-%d %H:%M:%S", 
                        timezone: Optional[datetime.timezone] = None) -> str:
        """Get current time in specified format"""
        
        if timezone is None:
            timezone = self.timezone
        
        now = datetime.datetime.now(timezone)
        return now.strftime(format_str)
    
    def get_timestamp(self, precision: str = 'seconds') -> Union[int, float]:
        """Get timestamp with specified precision"""
        
        current_time = time.time()
        
        if precision == 'seconds':
            return int(current_time)
        elif precision == 'milliseconds':
            return int(current_time * 1000)
        elif precision == 'microseconds':
            return int(current_time * 1000000)
        elif precision == 'nanoseconds':
            return int(current_time * 1000000000)
        else:
            return current_time
    
    def format_duration(self, seconds: float) -> str:
        """Format duration in human readable format"""
        
        if seconds < 60:
            return f"{seconds:.2f} seconds"
        elif seconds < 3600:
            minutes = seconds / 60
            return f"{minutes:.2f} minutes"
        elif seconds < 86400:
            hours = seconds / 3600
            return f"{hours:.2f} hours"
        else:
            days = seconds / 86400
            return f"{days:.2f} days"
    
    def time_operations(self, start_time: float, end_time: float) -> Dict[str, Any]:
        """Analyze time operations"""
        
        duration = end_time - start_time
        
        return {
            'start_time': datetime.datetime.fromtimestamp(start_time),
            'end_time': datetime.datetime.fromtimestamp(end_time),
            'duration_seconds': duration,
            'duration_formatted': self.format_duration(duration),
            'duration_milliseconds': duration * 1000,
            'duration_microseconds': duration * 1000000,
            'operations_per_second': 1 / duration if duration > 0 else float('inf'),
            'efficiency_score': min(100, max(0, 100 - (duration * 10)))
        }
    
    def create_timer(self, duration: float, callback: callable, 
                    repeat: bool = False) -> str:
        """Create a timer with callback"""
        
        timer_id = f"timer_{int(time.time())}_{secrets.token_hex(4)}"
        
        def timer_thread():
            while True:
                time.sleep(duration)
                try:
                    callback(timer_id)
                except Exception as e:
                    print(f"Timer callback error: {e}")
                
                if not repeat:
                    break
        
        timer_thread = threading.Thread(target=timer_thread, daemon=True)
        timer_thread.start()
        
        return timer_id
    
    def schedule_task(self, delay: float, task: callable, *args, **kwargs) -> str:
        """Schedule a task to run after delay"""
        
        task_id = f"task_{int(time.time())}_{secrets.token_hex(4)}"
        
        def delayed_task():
            time.sleep(delay)
            try:
                task(*args, **kwargs)
            except Exception as e:
                print(f"Scheduled task error: {e}")
        
        task_thread = threading.Thread(target=delayed_task, daemon=True)
        task_thread.start()
        
        return task_id
    
    def benchmark_function(self, func: callable, *args, **kwargs) -> Dict[str, Any]:
        """Benchmark a function execution"""
        
        # Warm up
        try:
            func(*args, **kwargs)
        except:
            pass
        
        # Benchmark
        start_time = time.perf_counter()
        try:
            result = func(*args, **kwargs)
            success = True
            error = None
        except Exception as e:
            result = None
            success = False
            error = str(e)
        
        end_time = time.perf_counter()
        duration = end_time - start_time
        
        return {
            'success': success,
            'result': result,
            'error': error,
            'duration': duration,
            'duration_formatted': self.format_duration(duration),
            'start_time': start_time,
            'end_time': end_time
        }
    
    def timezone_conversion(self, dt: datetime.datetime, 
                          from_tz: str, to_tz: str) -> datetime.datetime:
        """Convert datetime between timezones"""
        
        try:
            from_zone = datetime.timezone(datetime.timedelta(hours=int(from_tz)))
            to_zone = datetime.timezone(datetime.timedelta(hours=int(to_tz)))
            
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=from_zone)
            
            return dt.astimezone(to_zone)
        except Exception as e:
            raise ValueError(f"Invalid timezone conversion: {e}")

class CompilerHashUtilities:
    """
    Comprehensive hashing utilities for the n0mn0m IDE compiler
    Advanced hashing algorithms and security features
    """
    
    def __init__(self):
        self.supported_algorithms = [
            'md5', 'sha1', 'sha224', 'sha256', 'sha384', 'sha512',
            'sha3_224', 'sha3_256', 'sha3_384', 'sha3_512',
            'blake2b', 'blake2s', 'shake_128', 'shake_256'
        ]
        print("🔐 Hash utilities initialized")
    
    def calculate_hash(self, data: Union[str, bytes], 
                      algorithm: str = 'sha256') -> str:
        """Calculate hash of data using specified algorithm"""
        
        if algorithm not in self.supported_algorithms:
            raise ValueError(f"Unsupported algorithm: {algorithm}")
        
        if isinstance(data, str):
            data = data.encode('utf-8')
        
        hash_obj = hashlib.new(algorithm)
        hash_obj.update(data)
        return hash_obj.hexdigest()
    
    def calculate_file_hash(self, file_path: str, 
                           algorithm: str = 'sha256',
                           chunk_size: int = 8192) -> str:
        """Calculate hash of file using specified algorithm"""
        
        if algorithm not in self.supported_algorithms:
            raise ValueError(f"Unsupported algorithm: {algorithm}")
        
        hash_obj = hashlib.new(algorithm)
        
        with open(file_path, 'rb') as f:
            while chunk := f.read(chunk_size):
                hash_obj.update(chunk)
        
        return hash_obj.hexdigest()
    
    def calculate_hmac(self, data: Union[str, bytes], 
                      key: Union[str, bytes],
                      algorithm: str = 'sha256') -> str:
        """Calculate HMAC of data with key"""
        
        if isinstance(data, str):
            data = data.encode('utf-8')
        if isinstance(key, str):
            key = key.encode('utf-8')
        
        hmac_obj = hmac.new(key, data, algorithm)
        return hmac_obj.hexdigest()
    
    def verify_hash(self, data: Union[str, bytes], 
                   expected_hash: str,
                   algorithm: str = 'sha256') -> bool:
        """Verify data against expected hash"""
        
        calculated_hash = self.calculate_hash(data, algorithm)
        return hmac.compare_digest(calculated_hash, expected_hash)
    
    def verify_file_hash(self, file_path: str, 
                        expected_hash: str,
                        algorithm: str = 'sha256') -> bool:
        """Verify file against expected hash"""
        
        calculated_hash = self.calculate_file_hash(file_path, algorithm)
        return hmac.compare_digest(calculated_hash, expected_hash)
    
    def generate_password_hash(self, password: str, 
                              salt: Optional[str] = None,
                              algorithm: str = 'sha256') -> Dict[str, str]:
        """Generate secure password hash with salt"""
        
        if salt is None:
            salt = secrets.token_hex(32)
        
        # Use PBKDF2 for password hashing
        import hashlib
        import binascii
        
        salt_bytes = salt.encode('utf-8')
        password_bytes = password.encode('utf-8')
        
        # Use PBKDF2 with 100,000 iterations
        key = hashlib.pbkdf2_hmac(algorithm, password_bytes, salt_bytes, 100000)
        
        return {
            'hash': binascii.hexlify(key).decode('utf-8'),
            'salt': salt,
            'algorithm': f'pbkdf2_{algorithm}',
            'iterations': 100000
        }
    
    def verify_password(self, password: str, 
                       stored_hash: str,
                       salt: str,
                       algorithm: str = 'sha256') -> bool:
        """Verify password against stored hash"""
        
        password_hash = self.generate_password_hash(password, salt, algorithm)
        return hmac.compare_digest(password_hash['hash'], stored_hash)
    
    def generate_api_key(self, length: int = 32) -> str:
        """Generate secure API key"""
        
        return secrets.token_urlsafe(length)
    
    def generate_session_id(self) -> str:
        """Generate secure session ID"""
        
        return secrets.token_hex(32)
    
    def hash_collision_test(self, algorithm: str = 'md5', 
                           max_attempts: int = 1000000) -> Dict[str, Any]:
        """Test for hash collisions (educational purposes)"""
        
        hash_dict = {}
        collision_found = False
        collision_data = None
        
        for i in range(max_attempts):
            data = f"test_data_{i}_{secrets.token_hex(8)}"
            hash_value = self.calculate_hash(data, algorithm)
            
            if hash_value in hash_dict:
                collision_found = True
                collision_data = {
                    'data1': hash_dict[hash_value],
                    'data2': data,
                    'hash': hash_value,
                    'attempt': i
                }
                break
            
            hash_dict[hash_value] = data
        
        return {
            'collision_found': collision_found,
            'collision_data': collision_data,
            'attempts': len(hash_dict),
            'algorithm': algorithm
        }
    
    def hash_analysis(self, data: Union[str, bytes], 
                     algorithms: Optional[List[str]] = None) -> Dict[str, str]:
        """Analyze data with multiple hash algorithms"""
        
        if algorithms is None:
            algorithms = ['md5', 'sha1', 'sha256', 'sha512']
        
        results = {}
        for algorithm in algorithms:
            if algorithm in self.supported_algorithms:
                results[algorithm] = self.calculate_hash(data, algorithm)
        
        return results

# Integration function
def integrate_math_time_hash_utilities(ide_instance):
    """Integrate math, time, and hash utilities with IDE"""
    
    ide_instance.math_utils = CompilerMathUtilities()
    ide_instance.time_utils = CompilerTimeUtilities()
    ide_instance.hash_utils = CompilerHashUtilities()
    print("🔢⏰🔐 Math, Time, and Hash utilities integrated with IDE")

if __name__ == "__main__":
    print("🔢⏰🔐 Compiler Math, Time, and Hash Utilities")
    print("=" * 60)
    
    # Test math utilities
    math_utils = CompilerMathUtilities()
    print(f"✅ Fibonacci(10): {math_utils.fibonacci_sequence(10)}")
    print(f"✅ Prime analysis of 17: {math_utils.prime_functions(17)}")
    print(f"✅ Calculator: 2^10 + sqrt(144) = {math_utils.advanced_calculator('pow(2,10) + sqrt(144)')}")
    
    # Test time utilities
    time_utils = CompilerTimeUtilities()
    print(f"✅ Current time: {time_utils.get_current_time()}")
    print(f"✅ Timestamp: {time_utils.get_timestamp('milliseconds')}")
    
    # Test hash utilities
    hash_utils = CompilerHashUtilities()
    print(f"✅ SHA256 of 'hello': {hash_utils.calculate_hash('hello', 'sha256')}")
    print(f"✅ MD5 of 'world': {hash_utils.calculate_hash('world', 'md5')}")
    
    print("✅ All utilities ready!")
