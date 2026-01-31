#!/usr/bin/env python3
"""
Advanced Analytics Engine
Monitoring and analytics system
"""

import os
import sys
import json
import time
import threading
import psutil
from datetime import datetime, timedelta
from collections import defaultdict, deque
import sqlite3
import hashlib

class AdvancedAnalyticsEngine:
    """Advanced analytics engine for monitoring and analytics"""
    
    def __init__(self):
        self.initialized = False
        self.analytics_db = None
        self.metrics_collector = None
        self.analytics_thread = None
        self.monitoring_active = False
        
        # Analytics data
        self.system_metrics = defaultdict(list)
        self.user_metrics = defaultdict(list)
        self.performance_metrics = defaultdict(list)
        self.error_metrics = defaultdict(list)
        
        # Real-time data
        self.real_time_data = {
            'cpu_usage': deque(maxlen=100),
            'memory_usage': deque(maxlen=100),
            'disk_io': deque(maxlen=100),
            'network_io': deque(maxlen=100),
            'active_processes': deque(maxlen=50)
        }
        
        print("Advanced Analytics Engine created")
    
    def initialize(self):
        """Initialize the analytics engine"""
        try:
            # Initialize database
            self.initialize_database()
            
            # Start metrics collection
            self.start_metrics_collection()
            
            # Start analytics processing
            self.start_analytics_processing()
            
            self.initialized = True
            print("Advanced Analytics Engine initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Analytics Engine: {e}")
            return False
    
    def initialize_database(self):
        """Initialize analytics database"""
        try:
            # Create database directory
            db_dir = os.path.join(os.getcwd(), 'analytics_data')
            os.makedirs(db_dir, exist_ok=True)
            
            # Initialize SQLite database
            db_path = os.path.join(db_dir, 'analytics.db')
            self.analytics_db = sqlite3.connect(db_path, check_same_thread=False)
            
            # Create tables
            self.create_analytics_tables()
            
            print(f"Analytics database initialized: {db_path}")
            
        except Exception as e:
            print(f"Failed to initialize database: {e}")
            raise
    
    def create_analytics_tables(self):
        """Create analytics tables"""
        cursor = self.analytics_db.cursor()
        
        # System metrics table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS system_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                cpu_usage REAL,
                memory_usage REAL,
                disk_usage REAL,
                network_io REAL,
                active_processes INTEGER,
                load_average REAL
            )
        ''')
        
        # User metrics table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS user_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                user_id TEXT,
                action_type TEXT,
                action_data TEXT,
                session_id TEXT,
                duration REAL
            )
        ''')
        
        # Performance metrics table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS performance_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                operation_type TEXT,
                operation_name TEXT,
                duration REAL,
                memory_used REAL,
                cpu_used REAL,
                success BOOLEAN
            )
        ''')
        
        # Error metrics table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS error_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                error_type TEXT,
                error_message TEXT,
                error_location TEXT,
                severity TEXT,
                user_id TEXT,
                session_id TEXT
            )
        ''')
        
        # Analytics summary table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS analytics_summary (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                date DATE,
                total_operations INTEGER,
                successful_operations INTEGER,
                failed_operations INTEGER,
                average_response_time REAL,
                peak_cpu_usage REAL,
                peak_memory_usage REAL,
                total_errors INTEGER
            )
        ''')
        
        self.analytics_db.commit()
        print("Analytics tables created")
    
    def start_metrics_collection(self):
        """Start collecting system metrics"""
        if self.monitoring_active:
            return
        
        self.monitoring_active = True
        self.metrics_collector = threading.Thread(target=self.collect_metrics)
        self.metrics_collector.daemon = True
        self.metrics_collector.start()
        
        print("Metrics collection started")
    
    def stop_metrics_collection(self):
        """Stop collecting metrics"""
        self.monitoring_active = False
        if self.metrics_collector:
            self.metrics_collector.join(timeout=1)
        
        print("Metrics collection stopped")
    
    def collect_metrics(self):
        """Collect system metrics"""
        while self.monitoring_active:
            try:
                # Collect system metrics
                cpu_usage = psutil.cpu_percent(interval=1)
                memory = psutil.virtual_memory()
                disk = psutil.disk_usage('/')
                network = psutil.net_io_counters()
                processes = len(psutil.pids())
                
                # Store in real-time data
                self.real_time_data['cpu_usage'].append(cpu_usage)
                self.real_time_data['memory_usage'].append(memory.percent)
                self.real_time_data['disk_io'].append(disk.percent)
                self.real_time_data['network_io'].append(network.bytes_sent + network.bytes_recv)
                self.real_time_data['active_processes'].append(processes)
                
                # Store in database
                self.store_system_metrics({
                    'cpu_usage': cpu_usage,
                    'memory_usage': memory.percent,
                    'disk_usage': disk.percent,
                    'network_io': network.bytes_sent + network.bytes_recv,
                    'active_processes': processes,
                    'load_average': os.getloadavg()[0] if hasattr(os, 'getloadavg') else 0
                })
                
                time.sleep(5)  # Collect every 5 seconds
                
            except Exception as e:
                print(f"Metrics collection error: {e}")
                time.sleep(5)
    
    def store_system_metrics(self, metrics):
        """Store system metrics in database"""
        try:
            cursor = self.analytics_db.cursor()
            cursor.execute('''
                INSERT INTO system_metrics 
                (cpu_usage, memory_usage, disk_usage, network_io, active_processes, load_average)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (
                metrics['cpu_usage'],
                metrics['memory_usage'],
                metrics['disk_usage'],
                metrics['network_io'],
                metrics['active_processes'],
                metrics['load_average']
            ))
            self.analytics_db.commit()
            
        except Exception as e:
            print(f"Failed to store system metrics: {e}")
    
    def store_user_metrics(self, user_id, action_type, action_data, session_id, duration=0):
        """Store user metrics in database"""
        try:
            cursor = self.analytics_db.cursor()
            cursor.execute('''
                INSERT INTO user_metrics 
                (user_id, action_type, action_data, session_id, duration)
                VALUES (?, ?, ?, ?, ?)
            ''', (user_id, action_type, json.dumps(action_data), session_id, duration))
            self.analytics_db.commit()
            
        except Exception as e:
            print(f"Failed to store user metrics: {e}")
    
    def store_performance_metrics(self, operation_type, operation_name, duration, memory_used, cpu_used, success):
        """Store performance metrics in database"""
        try:
            cursor = self.analytics_db.cursor()
            cursor.execute('''
                INSERT INTO performance_metrics 
                (operation_type, operation_name, duration, memory_used, cpu_used, success)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (operation_type, operation_name, duration, memory_used, cpu_used, success))
            self.analytics_db.commit()
            
        except Exception as e:
            print(f"Failed to store performance metrics: {e}")
    
    def store_error_metrics(self, error_type, error_message, error_location, severity, user_id, session_id):
        """Store error metrics in database"""
        try:
            cursor = self.analytics_db.cursor()
            cursor.execute('''
                INSERT INTO error_metrics 
                (error_type, error_message, error_location, severity, user_id, session_id)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (error_type, error_message, error_location, severity, user_id, session_id))
            self.analytics_db.commit()
            
        except Exception as e:
            print(f"Failed to store error metrics: {e}")
    
    def start_analytics_processing(self):
        """Start analytics processing thread"""
        self.analytics_thread = threading.Thread(target=self.process_analytics)
        self.analytics_thread.daemon = True
        self.analytics_thread.start()
        
        print("Analytics processing started")
    
    def process_analytics(self):
        """Process analytics data"""
        while True:
            try:
                # Generate daily summary
                self.generate_daily_summary()
                
                # Clean up old data
                self.cleanup_old_data()
                
                # Process real-time analytics
                self.process_real_time_analytics()
                
                time.sleep(3600)  # Process every hour
                
            except Exception as e:
                print(f"Analytics processing error: {e}")
                time.sleep(3600)
    
    def generate_daily_summary(self):
        """Generate daily analytics summary"""
        try:
            cursor = self.analytics_db.cursor()
            
            # Get yesterday's date
            yesterday = (datetime.now() - timedelta(days=1)).date()
            
            # Check if summary already exists
            cursor.execute('SELECT id FROM analytics_summary WHERE date = ?', (yesterday,))
            if cursor.fetchone():
                return  # Summary already exists
            
            # Calculate summary metrics
            cursor.execute('''
                SELECT 
                    COUNT(*) as total_operations,
                    AVG(duration) as avg_duration,
                    MAX(cpu_usage) as peak_cpu,
                    MAX(memory_usage) as peak_memory
                FROM system_metrics 
                WHERE DATE(timestamp) = ?
            ''', (yesterday,))
            
            system_summary = cursor.fetchone()
            
            cursor.execute('''
                SELECT 
                    COUNT(*) as total_operations,
                    SUM(CASE WHEN success = 1 THEN 1 ELSE 0 END) as successful_operations,
                    SUM(CASE WHEN success = 0 THEN 1 ELSE 0 END) as failed_operations
                FROM performance_metrics 
                WHERE DATE(timestamp) = ?
            ''', (yesterday,))
            
            performance_summary = cursor.fetchone()
            
            cursor.execute('''
                SELECT COUNT(*) as total_errors
                FROM error_metrics 
                WHERE DATE(timestamp) = ?
            ''', (yesterday,))
            
            error_summary = cursor.fetchone()
            
            # Insert summary
            cursor.execute('''
                INSERT INTO analytics_summary 
                (date, total_operations, successful_operations, failed_operations, 
                 average_response_time, peak_cpu_usage, peak_memory_usage, total_errors)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                yesterday,
                performance_summary[0] if performance_summary else 0,
                performance_summary[1] if performance_summary else 0,
                performance_summary[2] if performance_summary else 0,
                system_summary[1] if system_summary else 0,
                system_summary[2] if system_summary else 0,
                system_summary[3] if system_summary else 0,
                error_summary[0] if error_summary else 0
            ))
            
            self.analytics_db.commit()
            print(f"Daily summary generated for {yesterday}")
            
        except Exception as e:
            print(f"Failed to generate daily summary: {e}")
    
    def cleanup_old_data(self):
        """Clean up old analytics data"""
        try:
            cursor = self.analytics_db.cursor()
            
            # Keep only last 30 days of detailed data
            cutoff_date = datetime.now() - timedelta(days=30)
            
            cursor.execute('DELETE FROM system_metrics WHERE timestamp < ?', (cutoff_date,))
            cursor.execute('DELETE FROM user_metrics WHERE timestamp < ?', (cutoff_date,))
            cursor.execute('DELETE FROM performance_metrics WHERE timestamp < ?', (cutoff_date,))
            cursor.execute('DELETE FROM error_metrics WHERE timestamp < ?', (cutoff_date,))
            
            self.analytics_db.commit()
            print("Old analytics data cleaned up")
            
        except Exception as e:
            print(f"Failed to cleanup old data: {e}")
    
    def process_real_time_analytics(self):
        """Process real-time analytics"""
        try:
            # Calculate real-time statistics
            if self.real_time_data['cpu_usage']:
                avg_cpu = sum(self.real_time_data['cpu_usage']) / len(self.real_time_data['cpu_usage'])
                max_cpu = max(self.real_time_data['cpu_usage'])
                
                if avg_cpu > 80 or max_cpu > 95:
                    self.store_error_metrics(
                        'high_cpu_usage',
                        f'High CPU usage detected: avg={avg_cpu:.1f}%, max={max_cpu:.1f}%',
                        'analytics_engine',
                        'warning',
                        'system',
                        'analytics'
                    )
            
            if self.real_time_data['memory_usage']:
                avg_memory = sum(self.real_time_data['memory_usage']) / len(self.real_time_data['memory_usage'])
                max_memory = max(self.real_time_data['memory_usage'])
                
                if avg_memory > 85 or max_memory > 95:
                    self.store_error_metrics(
                        'high_memory_usage',
                        f'High memory usage detected: avg={avg_memory:.1f}%, max={max_memory:.1f}%',
                        'analytics_engine',
                        'warning',
                        'system',
                        'analytics'
                    )
            
        except Exception as e:
            print(f"Real-time analytics processing error: {e}")
    
    def get_analytics_summary(self, days=7):
        """Get analytics summary for specified days"""
        try:
            cursor = self.analytics_db.cursor()
            
            cursor.execute('''
                SELECT 
                    date,
                    total_operations,
                    successful_operations,
                    failed_operations,
                    average_response_time,
                    peak_cpu_usage,
                    peak_memory_usage,
                    total_errors
                FROM analytics_summary 
                WHERE date >= DATE('now', '-{} days')
                ORDER BY date DESC
            '''.format(days))
            
            results = cursor.fetchall()
            
            summary = []
            for row in results:
                summary.append({
                    'date': row[0],
                    'total_operations': row[1],
                    'successful_operations': row[2],
                    'failed_operations': row[3],
                    'average_response_time': row[4],
                    'peak_cpu_usage': row[5],
                    'peak_memory_usage': row[6],
                    'total_errors': row[7]
                })
            
            return summary
            
        except Exception as e:
            print(f"Failed to get analytics summary: {e}")
            return []
    
    def get_real_time_metrics(self):
        """Get real-time metrics"""
        try:
            metrics = {}
            
            for metric_name, data in self.real_time_data.items():
                if data:
                    metrics[metric_name] = {
                        'current': data[-1],
                        'average': sum(data) / len(data),
                        'maximum': max(data),
                        'minimum': min(data),
                        'count': len(data)
                    }
                else:
                    metrics[metric_name] = {
                        'current': 0,
                        'average': 0,
                        'maximum': 0,
                        'minimum': 0,
                        'count': 0
                    }
            
            return metrics
            
        except Exception as e:
            print(f"Failed to get real-time metrics: {e}")
            return {}
    
    def get_performance_trends(self, hours=24):
        """Get performance trends for specified hours"""
        try:
            cursor = self.analytics_db.cursor()
            
            cursor.execute('''
                SELECT 
                    datetime(timestamp, 'localtime') as time,
                    cpu_usage,
                    memory_usage,
                    active_processes
                FROM system_metrics 
                WHERE timestamp >= datetime('now', '-{} hours')
                ORDER BY timestamp
            '''.format(hours))
            
            results = cursor.fetchall()
            
            trends = []
            for row in results:
                trends.append({
                    'time': row[0],
                    'cpu_usage': row[1],
                    'memory_usage': row[2],
                    'active_processes': row[3]
                })
            
            return trends
            
        except Exception as e:
            print(f"Failed to get performance trends: {e}")
            return []
    
    def get_error_analysis(self, days=7):
        """Get error analysis for specified days"""
        try:
            cursor = self.analytics_db.cursor()
            
            cursor.execute('''
                SELECT 
                    error_type,
                    severity,
                    COUNT(*) as count
                FROM error_metrics 
                WHERE timestamp >= datetime('now', '-{} days')
                GROUP BY error_type, severity
                ORDER BY count DESC
            '''.format(days))
            
            results = cursor.fetchall()
            
            error_analysis = []
            for row in results:
                error_analysis.append({
                    'error_type': row[0],
                    'severity': row[1],
                    'count': row[2]
                })
            
            return error_analysis
            
        except Exception as e:
            print(f"Failed to get error analysis: {e}")
            return []
    
    def export_analytics_data(self, export_path, days=30):
        """Export analytics data to JSON file"""
        try:
            cursor = self.analytics_db.cursor()
            
            # Get all data
            cursor.execute('''
                SELECT 
                    'system_metrics' as table_name,
                    timestamp,
                    cpu_usage,
                    memory_usage,
                    disk_usage,
                    network_io,
                    active_processes,
                    load_average
                FROM system_metrics 
                WHERE timestamp >= datetime('now', '-{} days')
                UNION ALL
                SELECT 
                    'performance_metrics' as table_name,
                    timestamp,
                    operation_type,
                    operation_name,
                    duration,
                    memory_used,
                    cpu_used,
                    success
                FROM performance_metrics 
                WHERE timestamp >= datetime('now', '-{} days')
                UNION ALL
                SELECT 
                    'error_metrics' as table_name,
                    timestamp,
                    error_type,
                    error_message,
                    error_location,
                    severity,
                    user_id,
                    session_id
                FROM error_metrics 
                WHERE timestamp >= datetime('now', '-{} days')
                ORDER BY timestamp
            '''.format(days, days, days))
            
            results = cursor.fetchall()
            
            # Convert to JSON format
            export_data = {
                'export_date': datetime.now().isoformat(),
                'days_exported': days,
                'total_records': len(results),
                'data': []
            }
            
            for row in results:
                export_data['data'].append({
                    'table': row[0],
                    'timestamp': row[1],
                    'data': row[2:]
                })
            
            # Write to file
            with open(export_path, 'w') as f:
                json.dump(export_data, f, indent=2)
            
            print(f"Analytics data exported to: {export_path}")
            return True
            
        except Exception as e:
            print(f"Failed to export analytics data: {e}")
            return False
    
    def shutdown(self):
        """Shutdown the analytics engine"""
        try:
            # Stop metrics collection
            self.stop_metrics_collection()
            
            # Close database connection
            if self.analytics_db:
                self.analytics_db.close()
            
            self.initialized = False
            print("Advanced Analytics Engine shutdown complete")
            
        except Exception as e:
            print(f"Error during shutdown: {e}")

def main():
    """Test the analytics engine"""
    print("Testing Advanced Analytics Engine...")
    
    analytics = AdvancedAnalyticsEngine()
    
    if analytics.initialize():
        print("✅ Analytics Engine initialized")
        
        # Simulate some metrics
        for i in range(10):
            analytics.store_user_metrics(f"user_{i}", "test_action", {"test": "data"}, f"session_{i}", 1.5)
            analytics.store_performance_metrics("test_operation", f"operation_{i}", 0.5, 100, 10, True)
            time.sleep(0.1)
        
        # Get analytics summary
        summary = analytics.get_analytics_summary(1)
        print(f"Analytics summary: {len(summary)} days")
        
        # Get real-time metrics
        real_time = analytics.get_real_time_metrics()
        print(f"Real-time metrics: {len(real_time)} metrics")
        
        # Get performance trends
        trends = analytics.get_performance_trends(1)
        print(f"Performance trends: {len(trends)} data points")
        
        # Get error analysis
        errors = analytics.get_error_analysis(1)
        print(f"Error analysis: {len(errors)} error types")
        
        # Export data
        export_path = "analytics_export.json"
        if analytics.export_analytics_data(export_path, 1):
            print(f"✅ Analytics data exported to {export_path}")
        
        # Shutdown
        analytics.shutdown()
        print("✅ Analytics Engine test complete")
    else:
        print("❌ Failed to initialize Analytics Engine")

if __name__ == "__main__":
    main()
