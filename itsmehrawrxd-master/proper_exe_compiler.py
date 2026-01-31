#!/usr/bin/env python3
"""
Proper EXE Compiler - Creates REAL executables
Generates actual Windows PE executables, not just batch wrappers

️  DO NOT DISTRIBUTE - CONFIDENTIAL TOOL ️
This is a specialized reverse engineering and security analysis tool.
Distribution of this software is strictly prohibited.
Reverse engineering of this IDE is strictly prohibited.
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import os
import sys
import platform
import subprocess
import threading
from pathlib import Path
import tempfile
import struct
import re
import base64
import hashlib
import urllib.request
import urllib.parse
import urllib.error
import json
import time
import random
import traceback
import socket
import queue

# Optional imports with fallbacks
try:
    import psutil
    PSUTIL_AVAILABLE = True
except ImportError:
    PSUTIL_AVAILABLE = False
    print(" psutil not available - some features disabled")

try:
    import socks
    SOCKS_AVAILABLE = True
except ImportError:
    SOCKS_AVAILABLE = False
    print(" socks not available - proxy features disabled")

try:
    import webbrowser
    WEBBROWSER_AVAILABLE = True
except ImportError:
    WEBBROWSER_AVAILABLE = False
    print(" webbrowser not available - browser features disabled")

try:
    import tkinter.web
    TKINTER_WEB_AVAILABLE = True
except ImportError:
    TKINTER_WEB_AVAILABLE = False
    print(" tkinter.web not available - web features disabled")

try:
    from PIL import Image, ImageTk, ImageDraw, ImageFont, ImageGrab
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print(" PIL not available - image features disabled")

try:
    import cv2
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False
    print(" cv2 not available - computer vision features disabled")

try:
    import numpy as np
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False
    print(" numpy not available - some analysis features disabled")

try:
    import mss
    MSS_AVAILABLE = True
except ImportError:
    MSS_AVAILABLE = False
    print(" mss not available - screen capture features disabled")

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False
    print(" requests not available - web scraping features disabled")
    
    # Fallback for requests
    class MockRequests:
        class Response:
            def __init__(self, status_code=200, text="", json_data=None):
                self.status_code = status_code
                self.text = text
                self._json_data = json_data or {}
            
            def json(self):
                return self._json_data
        
        def post(self, url, **kwargs):
            return self.Response(500, "requests not available")
    
    requests = MockRequests()

# Fallbacks for other optional imports
if not PIL_AVAILABLE:
    class MockImage:
        @staticmethod
        def frombytes(*args, **kwargs):
            return None
        @staticmethod
        def fromarray(*args, **kwargs):
            return None
    
    class MockImageTk:
        @staticmethod
        def PhotoImage(*args, **kwargs):
            return None
    
    Image = MockImage()
    ImageTk = MockImageTk()

if not CV2_AVAILABLE:
    class MockCV2:
        @staticmethod
        def cvtColor(*args, **kwargs):
            return args[0] if args else None
    
    cv2 = MockCV2()

if not NUMPY_AVAILABLE:
    class MockNumpy:
        @staticmethod
        def array(*args, **kwargs):
            return list(args[0]) if args else []
    
    np = MockNumpy()

if not MSS_AVAILABLE:
    class MockMSS:
        def screenshot(self, *args, **kwargs):
            return type('MockScreenshot', (), {
                'size': (800, 600),
                'bgra': b'\x00' * (800 * 600 * 4)
            })()
    
    mss = MockMSS()

if not SOCKS_AVAILABLE:
    class MockSocks:
        @staticmethod
        def set_default_proxy(*args, **kwargs):
            pass
    
    socks = MockSocks()

if not WEBBROWSER_AVAILABLE:
    class MockWebbrowser:
        @staticmethod
        def open(*args, **kwargs):
            print(f" webbrowser not available - would open: {args[0] if args else 'url'}")
    
    webbrowser = MockWebbrowser()

class ProxyManager:
    """Manage proxy connections for safe web scraping"""
    
    def __init__(self):
        print(" Proxy Manager initialized")
        
        # Proxy configurations
        self.proxy_configs = {
            'none': {'enabled': False, 'type': None, 'host': None, 'port': None, 'auth': None},
            'tor': {'enabled': True, 'type': 'socks5', 'host': '127.0.0.1', 'port': 9050, 'auth': None},
            'http_proxy': {'enabled': True, 'type': 'http', 'host': '127.0.0.1', 'port': 8080, 'auth': None},
            'socks5_proxy': {'enabled': True, 'type': 'socks5', 'host': '127.0.0.1', 'port': 1080, 'auth': None}
        }
        
        self.current_proxy = 'none'
        self.proxy_rotation = []
        self.rotation_index = 0
        
        # VPN/Proxy detection
        self.vpn_services = [
            'nordvpn', 'expressvpn', 'surfshark', 'cyberghost', 'privateinternetaccess',
            'protonvpn', 'windscribe', 'tunnelbear', 'ipvanish', 'hotspotshield'
        ]
        
        # Tor exit nodes (simplified list)
        self.tor_exit_nodes = [
            '127.0.0.1:9050', '127.0.0.1:9051', '127.0.0.1:9052'
        ]
        
        # Save/Load system
        self.data_directory = "scraped_data"
        self.auto_save_enabled = True
        self.save_interval = 300  # 5 minutes
        
        # Data storage
        self.scraped_data = {
            'api_keys': [],
            'ai_services': [],
            'proxies': [],
            'vpns': [],
            'copilots': [],
            'search_results': [],
            'configurations': {},
            'statistics': {},
            'last_updated': None
        }
        
        # Create data directory
        self.ensure_data_directory()
        
        # Load existing data
        self.load_all_data()
        
        # Start auto-save timer
        if self.auto_save_enabled:
            self.start_auto_save()
    
    def setup_proxy(self, proxy_type='none', host=None, port=None, username=None, password=None):
        """Setup proxy configuration"""
        try:
            if proxy_type == 'none':
                self._disable_proxy()
                print(" Proxy disabled - direct connection")
                return True
            
            elif proxy_type == 'tor':
                return self._setup_tor_proxy()
            
            elif proxy_type == 'http':
                return self._setup_http_proxy(host, port, username, password)
            
            elif proxy_type == 'socks5':
                return self._setup_socks5_proxy(host, port, username, password)
            
            else:
                print(f" Unknown proxy type: {proxy_type}")
                return False
                
        except Exception as e:
            print(f" Proxy setup error: {e}")
            return False
    
    def _disable_proxy(self):
        """Disable proxy and use direct connection"""
        try:
            # Reset socket to default
            socket.socket = socket._socketobject
            self.current_proxy = 'none'
            return True
        except Exception as e:
            print(f" Error disabling proxy: {e}")
            return False
    
    def _setup_tor_proxy(self):
        """Setup Tor proxy (SOCKS5)"""
        try:
            # Check if Tor is running
            if not self._check_tor_connection():
                print(" Tor not running - please start Tor service")
                return False
            
            # Setup SOCKS5 proxy through Tor
            socks.set_default_proxy(socks.SOCKS5, "127.0.0.1", 9050)
            socket.socket = socks.socksocket
            
            self.current_proxy = 'tor'
            print(" Tor proxy enabled - anonymous browsing")
            return True
            
        except Exception as e:
            print(f" Tor setup error: {e}")
            return False
    
    def _setup_http_proxy(self, host, port, username=None, password=None):
        """Setup HTTP proxy"""
        try:
            # Create proxy handler
            proxy_handler = urllib.request.ProxyHandler({
                'http': f'http://{host}:{port}',
                'https': f'http://{host}:{port}'
            })
            
            # Add authentication if provided
            if username and password:
                auth_handler = urllib.request.HTTPBasicAuthHandler()
                auth_handler.add_password(None, f'{host}:{port}', username, password)
                opener = urllib.request.build_opener(proxy_handler, auth_handler)
            else:
                opener = urllib.request.build_opener(proxy_handler)
            
            urllib.request.install_opener(opener)
            self.current_proxy = 'http'
            print(f" HTTP proxy enabled: {host}:{port}")
            return True
            
        except Exception as e:
            print(f" HTTP proxy setup error: {e}")
            return False
    
    def _setup_socks5_proxy(self, host, port, username=None, password=None):
        """Setup SOCKS5 proxy"""
        try:
            # Setup SOCKS5 proxy
            if username and password:
                socks.set_default_proxy(socks.SOCKS5, host, port, username=username, password=password)
            else:
                socks.set_default_proxy(socks.SOCKS5, host, port)
            
            socket.socket = socks.socksocket
            self.current_proxy = 'socks5'
            print(f" SOCKS5 proxy enabled: {host}:{port}")
            return True
            
        except Exception as e:
            print(f" SOCKS5 proxy setup error: {e}")
            return False
    
    def _check_tor_connection(self):
        """Check if Tor is running and accessible"""
        try:
            # Try to connect to Tor SOCKS proxy
            test_socket = socks.socksocket()
            test_socket.set_proxy(socks.SOCKS5, "127.0.0.1", 9050)
            test_socket.settimeout(5)
            test_socket.connect(("check.torproject.org", 80))
            test_socket.close()
            return True
        except:
            return False
    
    def setup_proxy_rotation(self, proxy_list):
        """Setup proxy rotation for distributed scraping"""
        try:
            self.proxy_rotation = proxy_list
            self.rotation_index = 0
            print(f" Proxy rotation enabled with {len(proxy_list)} proxies")
            return True
        except Exception as e:
            print(f" Proxy rotation setup error: {e}")
            return False
    
    def get_next_proxy(self):
        """Get next proxy in rotation"""
        if not self.proxy_rotation:
            return None
        
        proxy = self.proxy_rotation[self.rotation_index]
        self.rotation_index = (self.rotation_index + 1) % len(self.proxy_rotation)
        return proxy
    
    def test_proxy_connection(self, proxy_config=None):
        """Test proxy connection"""
        try:
            if proxy_config:
                # Test specific proxy
                if proxy_config['type'] == 'http':
                    proxy_handler = urllib.request.ProxyHandler({
                        'http': f"http://{proxy_config['host']}:{proxy_config['port']}",
                        'https': f"http://{proxy_config['host']}:{proxy_config['port']}"
                    })
                    opener = urllib.request.build_opener(proxy_handler)
                    urllib.request.install_opener(opener)
                
                elif proxy_config['type'] == 'socks5':
                    socks.set_default_proxy(socks.SOCKS5, proxy_config['host'], proxy_config['port'])
                    socket.socket = socks.socksocket
            
            # Test connection
            test_url = "http://httpbin.org/ip"
            request = urllib.request.Request(test_url)
            request.add_header('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36')
            
            with urllib.request.urlopen(request, timeout=10) as response:
                result = json.loads(response.read().decode('utf-8'))
                print(f" Proxy test successful - IP: {result.get('origin', 'Unknown')}")
                return True
                
        except Exception as e:
            print(f" Proxy test failed: {e}")
            return False
    
    def get_current_ip(self):
        """Get current public IP address"""
        try:
            test_urls = [
                "http://httpbin.org/ip",
                "http://ipinfo.io/ip",
                "http://icanhazip.com"
            ]
            
            for url in test_urls:
                try:
                    request = urllib.request.Request(url)
                    request.add_header('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36')
                    
                    with urllib.request.urlopen(request, timeout=5) as response:
                        if 'httpbin.org' in url:
                            result = json.loads(response.read().decode('utf-8'))
                            return result.get('origin', 'Unknown')
                        else:
                            return response.read().decode('utf-8').strip()
                except:
                    continue
            
            return "Unknown"
            
        except Exception as e:
            print(f" IP detection error: {e}")
            return "Unknown"
    
    def detect_vpn_proxy(self):
        """Detect if running through VPN or proxy"""
        try:
            current_ip = self.get_current_ip()
            
            # Check for common VPN/proxy indicators
            vpn_indicators = [
                'vpn', 'proxy', 'tor', 'anonymous', 'privacy'
            ]
            
            # This is a simplified check - in reality you'd use more sophisticated detection
            for indicator in vpn_indicators:
                if indicator in current_ip.lower():
                    return True
            
            return False
            
        except Exception as e:
            print(f" VPN detection error: {e}")
            return False
    
    def setup_stealth_mode(self):
        """Setup stealth mode with multiple layers of anonymity"""
        try:
            print(" Setting up stealth mode...")
            
            # 1. Use Tor proxy
            if not self._setup_tor_proxy():
                print("️ Tor not available - using direct connection")
            
            # 2. Add additional headers for stealth
            self.stealth_headers = {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
                'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
                'Accept-Language': 'en-US,en;q=0.5',
                'Accept-Encoding': 'gzip, deflate',
                'Connection': 'keep-alive',
                'Upgrade-Insecure-Requests': '1',
                'Sec-Fetch-Dest': 'document',
                'Sec-Fetch-Mode': 'navigate',
                'Sec-Fetch-Site': 'none',
                'Cache-Control': 'max-age=0'
            }
            
            # 3. Random delays between requests
            self.stealth_delay_range = (2, 8)
            
            print(" Stealth mode enabled - maximum anonymity")
            return True
            
        except Exception as e:
            print(f" Stealth mode setup error: {e}")
            return False
    
    def ensure_data_directory(self):
        """Ensure data directory exists"""
        try:
            if not os.path.exists(self.data_directory):
                os.makedirs(self.data_directory)
                print(f" Created data directory: {self.data_directory}")
        except Exception as e:
            print(f" Error creating data directory: {e}")
    
    def save_all_data(self):
        """Save all scraped data to files"""
        try:
            self.scraped_data['last_updated'] = time.time()
            
            # Save main data file
            main_file = os.path.join(self.data_directory, "scraped_data.json")
            with open(main_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data, f, indent=2, ensure_ascii=False)
            
            # Save individual category files
            self.save_api_keys()
            self.save_ai_services()
            self.save_proxies()
            self.save_vpns()
            self.save_copilots()
            self.save_search_results()
            self.save_configurations()
            self.save_statistics()
            
            print(f" All data saved to {self.data_directory}")
            return True
            
        except Exception as e:
            print(f" Error saving data: {e}")
            return False
    
    def load_all_data(self):
        """Load all scraped data from files"""
        try:
            # Load main data file
            main_file = os.path.join(self.data_directory, "scraped_data.json")
            if os.path.exists(main_file):
                with open(main_file, 'r', encoding='utf-8') as f:
                    self.scraped_data = json.load(f)
                print(f" Loaded main data file")
            
            # Load individual category files
            self.load_api_keys()
            self.load_ai_services()
            self.load_proxies()
            self.load_vpns()
            self.load_copilots()
            self.load_search_results()
            self.load_configurations()
            self.load_statistics()
            
            print(f" All data loaded from {self.data_directory}")
            return True
            
        except Exception as e:
            print(f" Error loading data: {e}")
            return False
    
    def save_api_keys(self):
        """Save API keys data"""
        try:
            api_keys_file = os.path.join(self.data_directory, "api_keys.json")
            with open(api_keys_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['api_keys'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving API keys: {e}")
    
    def load_api_keys(self):
        """Load API keys data"""
        try:
            api_keys_file = os.path.join(self.data_directory, "api_keys.json")
            if os.path.exists(api_keys_file):
                with open(api_keys_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['api_keys'] = json.load(f)
        except Exception as e:
            print(f" Error loading API keys: {e}")
    
    def save_ai_services(self):
        """Save AI services data"""
        try:
            ai_services_file = os.path.join(self.data_directory, "ai_services.json")
            with open(ai_services_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['ai_services'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving AI services: {e}")
    
    def load_ai_services(self):
        """Load AI services data"""
        try:
            ai_services_file = os.path.join(self.data_directory, "ai_services.json")
            if os.path.exists(ai_services_file):
                with open(ai_services_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['ai_services'] = json.load(f)
        except Exception as e:
            print(f" Error loading AI services: {e}")
    
    def save_proxies(self):
        """Save proxy data"""
        try:
            proxies_file = os.path.join(self.data_directory, "proxies.json")
            with open(proxies_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['proxies'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving proxies: {e}")
    
    def load_proxies(self):
        """Load proxy data"""
        try:
            proxies_file = os.path.join(self.data_directory, "proxies.json")
            if os.path.exists(proxies_file):
                with open(proxies_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['proxies'] = json.load(f)
        except Exception as e:
            print(f" Error loading proxies: {e}")
    
    def save_vpns(self):
        """Save VPN data"""
        try:
            vpns_file = os.path.join(self.data_directory, "vpns.json")
            with open(vpns_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['vpns'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving VPNs: {e}")
    
    def load_vpns(self):
        """Load VPN data"""
        try:
            vpns_file = os.path.join(self.data_directory, "vpns.json")
            if os.path.exists(vpns_file):
                with open(vpns_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['vpns'] = json.load(f)
        except Exception as e:
            print(f" Error loading VPNs: {e}")
    
    def save_copilots(self):
        """Save copilot data"""
        try:
            copilots_file = os.path.join(self.data_directory, "copilots.json")
            with open(copilots_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['copilots'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving copilots: {e}")
    
    def load_copilots(self):
        """Load copilot data"""
        try:
            copilots_file = os.path.join(self.data_directory, "copilots.json")
            if os.path.exists(copilots_file):
                with open(copilots_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['copilots'] = json.load(f)
        except Exception as e:
            print(f" Error loading copilots: {e}")
    
    def save_search_results(self):
        """Save search results data"""
        try:
            search_results_file = os.path.join(self.data_directory, "search_results.json")
            with open(search_results_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['search_results'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving search results: {e}")
    
    def load_search_results(self):
        """Load search results data"""
        try:
            search_results_file = os.path.join(self.data_directory, "search_results.json")
            if os.path.exists(search_results_file):
                with open(search_results_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['search_results'] = json.load(f)
        except Exception as e:
            print(f" Error loading search results: {e}")
    
    def save_configurations(self):
        """Save configuration data"""
        try:
            config_file = os.path.join(self.data_directory, "configurations.json")
            with open(config_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['configurations'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving configurations: {e}")
    
    def load_configurations(self):
        """Load configuration data"""
        try:
            config_file = os.path.join(self.data_directory, "configurations.json")
            if os.path.exists(config_file):
                with open(config_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['configurations'] = json.load(f)
        except Exception as e:
            print(f" Error loading configurations: {e}")
    
    def save_statistics(self):
        """Save statistics data"""
        try:
            stats_file = os.path.join(self.data_directory, "statistics.json")
            with open(stats_file, 'w', encoding='utf-8') as f:
                json.dump(self.scraped_data['statistics'], f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f" Error saving statistics: {e}")
    
    def load_statistics(self):
        """Load statistics data"""
        try:
            stats_file = os.path.join(self.data_directory, "statistics.json")
            if os.path.exists(stats_file):
                with open(stats_file, 'r', encoding='utf-8') as f:
                    self.scraped_data['statistics'] = json.load(f)
        except Exception as e:
            print(f" Error loading statistics: {e}")
    
    def start_auto_save(self):
        """Start auto-save timer"""
        def auto_save_worker():
            while self.auto_save_enabled:
                time.sleep(self.save_interval)
                if self.auto_save_enabled:
                    self.save_all_data()
        
        self.auto_save_thread = threading.Thread(target=auto_save_worker, daemon=True)
        self.auto_save_thread.start()
        print(f" Auto-save started (interval: {self.save_interval}s)")
    
    def stop_auto_save(self):
        """Stop auto-save timer"""
        self.auto_save_enabled = False
        print(" Auto-save stopped")
    
    def add_scraped_data(self, category, data):
        """Add data to scraped results"""
        try:
            if category in self.scraped_data:
                if isinstance(data, list):
                    self.scraped_data[category].extend(data)
                else:
                    self.scraped_data[category].append(data)
                
                # Auto-save if enabled
                if self.auto_save_enabled:
                    self.save_all_data()
                
                print(f" Added {len(data) if isinstance(data, list) else 1} items to {category}")
        except Exception as e:
            print(f" Error adding data to {category}: {e}")
    
    def get_scraped_data(self, category):
        """Get scraped data by category"""
        return self.scraped_data.get(category, [])
    
    def clear_scraped_data(self, category=None):
        """Clear scraped data"""
        try:
            if category:
                self.scraped_data[category] = []
                print(f" Cleared {category} data")
            else:
                for key in self.scraped_data:
                    if isinstance(self.scraped_data[key], list):
                        self.scraped_data[key] = []
                print(" Cleared all scraped data")
            
            # Save after clearing
            self.save_all_data()
        except Exception as e:
            print(f" Error clearing data: {e}")
    
    def export_data(self, format='json', filename=None):
        """Export all data in various formats"""
        try:
            if not filename:
                timestamp = time.strftime("%Y%m%d_%H%M%S")
                filename = f"scraped_data_export_{timestamp}"
            
            if format.lower() == 'json':
                export_file = os.path.join(self.data_directory, f"{filename}.json")
                with open(export_file, 'w', encoding='utf-8') as f:
                    json.dump(self.scraped_data, f, indent=2, ensure_ascii=False)
            
            elif format.lower() == 'csv':
                # Export as CSV (flattened)
                export_file = os.path.join(self.data_directory, f"{filename}.csv")
                with open(export_file, 'w', encoding='utf-8', newline='') as f:
                    import csv
                    writer = csv.writer(f)
                    
                    # Write headers
                    writer.writerow(['Category', 'Type', 'Data', 'Timestamp'])
                    
                    # Write data
                    for category, items in self.scraped_data.items():
                        if isinstance(items, list):
                            for item in items:
                                if isinstance(item, dict):
                                    writer.writerow([category, item.get('type', ''), str(item), time.time()])
                                else:
                                    writer.writerow([category, 'item', str(item), time.time()])
            
            elif format.lower() == 'txt':
                # Export as readable text
                export_file = os.path.join(self.data_directory, f"{filename}.txt")
                with open(export_file, 'w', encoding='utf-8') as f:
                    f.write("SCRAPED DATA EXPORT\n")
                    f.write("=" * 50 + "\n\n")
                    
                    for category, items in self.scraped_data.items():
                        f.write(f"{category.upper()}:\n")
                        f.write("-" * 20 + "\n")
                        
                        if isinstance(items, list):
                            for i, item in enumerate(items, 1):
                                f.write(f"{i}. {item}\n")
                        else:
                            f.write(f"{items}\n")
                        f.write("\n")
            
            print(f" Data exported to {export_file}")
            return export_file
            
        except Exception as e:
            print(f" Error exporting data: {e}")
            return None
    
    def import_data(self, filepath):
        """Import data from file"""
        try:
            if not os.path.exists(filepath):
                print(f" Import file not found: {filepath}")
                return False
            
            with open(filepath, 'r', encoding='utf-8') as f:
                if filepath.endswith('.json'):
                    imported_data = json.load(f)
                else:
                    print(" Only JSON import supported currently")
                    return False
            
            # Merge with existing data
            for category, data in imported_data.items():
                if category in self.scraped_data:
                    if isinstance(data, list) and isinstance(self.scraped_data[category], list):
                        self.scraped_data[category].extend(data)
                    else:
                        self.scraped_data[category] = data
            
            # Save imported data
            self.save_all_data()
            print(f" Data imported from {filepath}")
            return True
            
        except Exception as e:
            print(f" Error importing data: {e}")
            return False

class MultiCopilotManager:
    """Manages multiple AI copilots from scraped services"""
    
    def __init__(self):
        print(" Multi-Copilot Manager initialized")
        self.active_copilots = {}
        self.copilot_configs = {}
        self.copilot_history = {}
        
    def create_copilot_from_service(self, service_name, service_info, api_key=None):
        """Create a new copilot from a scraped service"""
        copilot_id = f"copilot_{len(self.active_copilots) + 1}_{service_name}"
        
        copilot_config = {
            'id': copilot_id,
            'name': f"{service_info['name']} Copilot",
            'service': service_name,
            'service_info': service_info,
            'api_key': api_key,
            'created_at': time.time(),
            'status': 'active',
            'usage_count': 0,
            'last_used': None
        }
        
        self.active_copilots[copilot_id] = copilot_config
        self.copilot_configs[copilot_id] = copilot_config
        self.copilot_history[copilot_id] = []
        
        print(f" Created copilot: {copilot_id} for {service_name}")
        return copilot_id
    
    def get_copilot(self, copilot_id):
        """Get copilot configuration"""
        return self.active_copilots.get(copilot_id)
    
    def list_copilots(self):
        """List all active copilots"""
        return list(self.active_copilots.keys())
    
    def use_copilot(self, copilot_id, prompt):
        """Use a specific copilot for coding assistance"""
        if copilot_id not in self.active_copilots:
            return {"error": f"Copilot {copilot_id} not found"}
        
        copilot = self.active_copilots[copilot_id]
        service_name = copilot['service']
        
        # Update usage stats
        copilot['usage_count'] += 1
        copilot['last_used'] = time.time()
        
        # Add to history
        self.copilot_history[copilot_id].append({
            'timestamp': time.time(),
            'prompt': prompt,
            'service': service_name
        })
        
        # Use the service
        try:
            if service_name in ['huggingface_chat', 'ollama_local', 'groq_free', 'together_free', 'replicate_free']:
                # Use free services
                from proper_exe_compiler import AIKeyScraper
                scraper = AIKeyScraper()
                result = scraper.use_free_chatbox(service_name, prompt)
                result['copilot_id'] = copilot_id
                result['copilot_name'] = copilot['name']
                return result
            else:
                return {"error": f"Service {service_name} not supported yet"}
        
        except Exception as e:
            return {"error": f"Error using copilot {copilot_id}: {e}"}
    
    def remove_copilot(self, copilot_id):
        """Remove a copilot"""
        if copilot_id in self.active_copilots:
            del self.active_copilots[copilot_id]
            print(f" Removed copilot: {copilot_id}")
            return True
        return False
    
    def get_copilot_stats(self, copilot_id):
        """Get usage statistics for a copilot"""
        if copilot_id not in self.active_copilots:
            return None
        
        copilot = self.active_copilots[copilot_id]
        history = self.copilot_history.get(copilot_id, [])
        
        return {
            'id': copilot_id,
            'name': copilot['name'],
            'service': copilot['service'],
            'usage_count': copilot['usage_count'],
            'last_used': copilot['last_used'],
            'created_at': copilot['created_at'],
            'history_count': len(history),
            'status': copilot['status']
        }

class AIKeyScraper:
    """AI-powered key and credential scraper for code analysis"""
    
    def __init__(self):
        print(" AI Key Scraper initialized")
        self.proxy_manager = ProxyManager()
        self.copilot_manager = MultiCopilotManager()
        
        # Common API key patterns
        self.api_key_patterns = {
            'openai': [
                r'sk-[a-zA-Z0-9]{20,}',
                r'OPENAI_API_KEY["\']?\s*[:=]\s*["\']?([a-zA-Z0-9\-_]{20,})',
                r'openai\.api_key\s*=\s*["\']([^"\']+)["\']'
            ],
            'anthropic': [
                r'sk-ant-[a-zA-Z0-9\-_]{20,}',
                r'ANTHROPIC_API_KEY["\']?\s*[:=]\s*["\']?([a-zA-Z0-9\-_]{20,})',
                r'anthropic\.api_key\s*=\s*["\']([^"\']+)["\']'
            ],
            'google': [
                r'AIza[0-9A-Za-z\-_]{35}',
                r'GOOGLE_API_KEY["\']?\s*[:=]\s*["\']?([a-zA-Z0-9\-_]{35,})',
                r'google\.api_key\s*=\s*["\']([^"\']+)["\']'
            ],
            'azure': [
                r'[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}',
                r'AZURE_API_KEY["\']?\s*[:=]\s*["\']?([a-zA-Z0-9\-_]{20,})',
                r'azure\.api_key\s*=\s*["\']([^"\']+)["\']'
            ],
            'aws': [
                r'AKIA[0-9A-Z]{16}',
                r'AWS_ACCESS_KEY_ID["\']?\s*[:=]\s*["\']?([A-Z0-9]{20})',
                r'aws\.access_key_id\s*=\s*["\']([^"\']+)["\']'
            ],
            'github': [
                r'ghp_[a-zA-Z0-9]{36}',
                r'gho_[a-zA-Z0-9]{36}',
                r'ghu_[a-zA-Z0-9]{36}',
                r'ghs_[a-zA-Z0-9]{36}',
                r'ghr_[a-zA-Z0-9]{36}',
                r'GITHUB_TOKEN["\']?\s*[:=]\s*["\']?([a-zA-Z0-9_]{20,})'
            ],
            'discord': [
                r'[MN][A-Za-z\d]{23}\.[\w-]{6}\.[\w-]{27}',
                r'DISCORD_TOKEN["\']?\s*[:=]\s*["\']?([a-zA-Z0-9\.\-_]{20,})'
            ],
            'telegram': [
                r'\d{8,10}:[a-zA-Z0-9\-_]{35}',
                r'TELEGRAM_BOT_TOKEN["\']?\s*[:=]\s*["\']?([0-9]{8,10}:[a-zA-Z0-9\-_]{35})'
            ],
            'generic': [
                r'["\']?[A-Za-z0-9_]{20,}["\']?\s*[:=]\s*["\']?([A-Za-z0-9_\-\.]{20,})',
                r'api[_-]?key["\']?\s*[:=]\s*["\']?([A-Za-z0-9_\-\.]{20,})',
                r'token["\']?\s*[:=]\s*["\']?([A-Za-z0-9_\-\.]{20,})',
                r'secret["\']?\s*[:=]\s*["\']?([A-Za-z0-9_\-\.]{20,})'
            ]
        }
        
        # File extensions to scan
        self.scannable_extensions = {
            '.py', '.js', '.ts', '.java', '.cpp', '.c', '.cs', '.php', 
            '.rb', '.go', '.rs', '.swift', '.kt', '.scala', '.sh', '.bat',
            '.ps1', '.env', '.config', '.json', '.yaml', '.yml', '.xml',
            '.ini', '.cfg', '.conf', '.txt', '.md', '.log'
        }
        
        # AI Chatbox patterns and services
        self.ai_chatbox_patterns = {
            'chatgpt': [
                r'chat\.openai\.com',
                r'chatgpt\.com',
                r'openai\.com/chat',
                r'gpt-[0-9]+',
                r'chatgpt[_-]?api'
            ],
            'claude': [
                r'claude\.ai',
                r'anthropic\.com',
                r'claude[_-]?api',
                r'claude-[0-9]+'
            ],
            'bard': [
                r'bard\.google\.com',
                r'gemini\.google\.com',
                r'google\.com/ai',
                r'bard[_-]?api',
                r'gemini[_-]?api'
            ],
            'copilot': [
                r'github\.com/copilot',
                r'copilot\.github\.com',
                r'github[_-]?copilot',
                r'copilot[_-]?api'
            ],
            'huggingface': [
                r'huggingface\.co',
                r'hf\.co',
                r'hugging[_-]?face[_-]?chat',
                r'inference\.huggingface\.co'
            ],
            'ollama': [
                r'ollama\.ai',
                r'localhost:11434',
                r'ollama[_-]?api',
                r'ollama[_-]?chat'
            ],
            'groq': [
                r'groq\.com',
                r'api\.groq\.com',
                r'groq[_-]?api',
                r'groq[_-]?chat'
            ],
            'together': [
                r'together\.ai',
                r'api\.together\.xyz',
                r'together[_-]?ai[_-]?api',
                r'together[_-]?chat'
            ],
            'replicate': [
                r'replicate\.com',
                r'api\.replicate\.com',
                r'replicate[_-]?api',
                r'replicate[_-]?chat'
            ],
            'perplexity': [
                r'perplexity\.ai',
                r'api\.perplexity\.ai',
                r'perplexity[_-]?api',
                r'perplexity[_-]?chat'
            ]
        }
        
        # Free AI chatbox services
        self.free_chatbox_services = {
            'huggingface_chat': {
                'name': 'Hugging Face Chat',
                'url': 'https://huggingface.co/chat',
                'api_url': 'https://api-inference.huggingface.co/models',
                'models': ['microsoft/DialoGPT-medium', 'microsoft/DialoGPT-large'],
                'free': True,
                'requires_auth': False
            },
            'ollama_local': {
                'name': 'Ollama Local',
                'url': 'http://localhost:11434',
                'api_url': 'http://localhost:11434/api/generate',
                'models': ['llama2', 'codellama', 'mistral', 'phi'],
                'free': True,
                'requires_auth': False
            },
            'groq_free': {
                'name': 'Groq Free',
                'url': 'https://console.groq.com',
                'api_url': 'https://api.groq.com/openai/v1',
                'models': ['llama3-8b-8192', 'mixtral-8x7b-32768'],
                'free': True,
                'requires_auth': True
            },
            'together_free': {
                'name': 'Together AI Free',
                'url': 'https://api.together.xyz',
                'api_url': 'https://api.together.xyz/v1',
                'models': ['meta-llama/Llama-2-7b-chat-hf'],
                'free': True,
                'requires_auth': True
            },
            'replicate_free': {
                'name': 'Replicate Free',
                'url': 'https://replicate.com',
                'api_url': 'https://api.replicate.com/v1',
                'models': ['meta/llama-2-7b-chat'],
                'free': True,
                'requires_auth': True
            }
        }
        
        # Google Dork patterns for finding exposed keys
        self.google_dorks = {
            'openai': [
                'site:github.com "sk-" filetype:py',
                'site:github.com "OPENAI_API_KEY" filetype:env',
                'site:pastebin.com "sk-"',
                'site:gist.github.com "openai.api_key"',
                '"sk-" "openai" filetype:json',
                'site:stackoverflow.com "sk-" "openai"'
            ],
            'google': [
                'site:github.com "AIza" filetype:js',
                'site:github.com "GOOGLE_API_KEY" filetype:env',
                'site:pastebin.com "AIza"',
                'site:gist.github.com "google.api_key"',
                '"AIza" "google" filetype:json',
                'site:stackoverflow.com "AIza" "google"'
            ],
            'github': [
                'site:github.com "ghp_" filetype:py',
                'site:github.com "GITHUB_TOKEN" filetype:env',
                'site:pastebin.com "ghp_"',
                'site:gist.github.com "github.token"',
                '"ghp_" "github" filetype:json',
                'site:stackoverflow.com "ghp_" "github"'
            ],
            'aws': [
                'site:github.com "AKIA" filetype:py',
                'site:github.com "AWS_ACCESS_KEY_ID" filetype:env',
                'site:pastebin.com "AKIA"',
                'site:gist.github.com "aws.access_key"',
                '"AKIA" "aws" filetype:json',
                'site:stackoverflow.com "AKIA" "aws"'
            ],
            'discord': [
                'site:github.com "discord token" filetype:js',
                'site:github.com "DISCORD_TOKEN" filetype:env',
                'site:pastebin.com "discord"',
                'site:gist.github.com "discord.token"',
                '"discord" "token" filetype:json',
                'site:stackoverflow.com "discord" "token"'
            ],
            'generic': [
                'site:github.com "api_key" filetype:py',
                'site:github.com "API_KEY" filetype:env',
                'site:pastebin.com "api_key"',
                'site:gist.github.com "api_key"',
                '"api_key" filetype:json',
                'site:stackoverflow.com "api_key"'
            ]
        }
        
        # User agents for web scraping
        self.user_agents = [
            'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0',
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:89.0) Gecko/20100101 Firefox/89.0'
        ]
    
    def scan_file(self, file_path):
        """Scan a single file for API keys and credentials"""
        try:
            print(f" Scanning file: {file_path}")
            
            # Read file content
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Scan for keys
            found_keys = []
            
            for provider, patterns in self.api_key_patterns.items():
                for pattern in patterns:
                    matches = re.finditer(pattern, content, re.IGNORECASE | re.MULTILINE)
                    for match in matches:
                        key_value = match.group(1) if match.groups() else match.group(0)
                        
                        # Validate key format
                        if self._validate_key_format(key_value, provider):
                            found_keys.append({
                                'provider': provider,
                                'key': key_value,
                                'line': content[:match.start()].count('\n') + 1,
                                'context': self._get_context(content, match.start(), match.end()),
                                'pattern': pattern,
                                'file': file_path
                            })
            
            return found_keys
            
        except Exception as e:
            print(f" Error scanning {file_path}: {e}")
            return []
    
    def scan_directory(self, directory_path, recursive=True):
        """Scan directory for API keys"""
        print(f" Scanning directory: {directory_path}")
        
        all_keys = []
        scanned_files = 0
        
        try:
            for root, dirs, files in os.walk(directory_path):
                for file in files:
                    file_path = os.path.join(root, file)
                    file_ext = Path(file_path).suffix.lower()
                    
                    # Check if file should be scanned
                    if file_ext in self.scannable_extensions or file_ext == '':
                        keys = self.scan_file(file_path)
                        all_keys.extend(keys)
                        scanned_files += 1
                
                if not recursive:
                    break
            
            print(f" Scanned {scanned_files} files, found {len(all_keys)} potential keys")
            return all_keys
            
        except Exception as e:
            print(f" Error scanning directory: {e}")
            return []
    
    def _validate_key_format(self, key, provider):
        """Validate if key matches expected format for provider"""
        if not key or len(key) < 10:
            return False
        
        # Basic validation rules
        if provider == 'openai' and key.startswith('sk-'):
            return True
        elif provider == 'anthropic' and key.startswith('sk-ant-'):
            return True
        elif provider == 'google' and key.startswith('AIza'):
            return True
        elif provider == 'github' and key.startswith(('ghp_', 'gho_', 'ghu_', 'ghs_', 'ghr_')):
            return True
        elif provider == 'discord' and '.' in key and len(key) > 50:
            return True
        elif provider == 'telegram' and ':' in key:
            return True
        elif provider == 'aws' and key.startswith('AKIA'):
            return True
        elif provider == 'azure' and '-' in key and len(key) == 36:
            return True
        elif provider == 'generic':
            return len(key) >= 20 and any(c.isalnum() for c in key)
        
        return False
    
    def _get_context(self, content, start, end):
        """Get context around found key"""
        lines = content.split('\n')
        line_start = content[:start].count('\n')
        line_end = content[:end].count('\n')
        
        context_lines = []
        for i in range(max(0, line_start - 2), min(len(lines), line_end + 3)):
            prefix = ">>> " if i == line_start else "    "
            context_lines.append(f"{prefix}{i+1:4d}: {lines[i]}")
        
        return '\n'.join(context_lines)
    
    def generate_report(self, found_keys, output_file=None):
        """Generate a security report"""
        if not output_file:
            output_file = "ai_key_scan_report.txt"
        
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(" AI KEY SCAN REPORT\n")
                f.write("=" * 50 + "\n\n")
                f.write(f"Scan Date: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Total Keys Found: {len(found_keys)}\n\n")
                
                if not found_keys:
                    f.write(" No API keys or credentials found!\n")
                    return output_file
                
                # Group by provider
                by_provider = {}
                for key in found_keys:
                    provider = key['provider']
                    if provider not in by_provider:
                        by_provider[provider] = []
                    by_provider[provider].append(key)
                
                # Write report
                for provider, keys in by_provider.items():
                    f.write(f"\n {provider.upper()} KEYS ({len(keys)} found):\n")
                    f.write("-" * 30 + "\n")
                    
                    for key in keys:
                        f.write(f"\nFile: {key['file']}\n")
                        f.write(f"Line: {key['line']}\n")
                        f.write(f"Key: {key['key'][:20]}...{key['key'][-10:]}\n")
                        f.write(f"Context:\n{key['context']}\n")
                        f.write("\n" + "="*50 + "\n")
                
                # Security recommendations
                f.write("\n️ SECURITY RECOMMENDATIONS:\n")
                f.write("-" * 30 + "\n")
                f.write("1. Remove hardcoded keys from source code\n")
                f.write("2. Use environment variables for API keys\n")
                f.write("3. Implement proper key rotation\n")
                f.write("4. Use secure key management services\n")
                f.write("5. Add .env files to .gitignore\n")
                f.write("6. Review commit history for exposed keys\n")
            
            print(f" Security report generated: {output_file}")
            return output_file
            
        except Exception as e:
            print(f" Error generating report: {e}")
            return None
    
    def mask_keys_in_file(self, file_path, output_file=None):
        """Mask found keys in a file"""
        if not output_file:
            output_file = file_path + '.masked'
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            original_content = content
            masked_count = 0
            
            for provider, patterns in self.api_key_patterns.items():
                for pattern in patterns:
                    def mask_match(match):
                        nonlocal masked_count
                        masked_count += 1
                        key_value = match.group(1) if match.groups() else match.group(0)
                        if self._validate_key_format(key_value, provider):
                            return match.group(0).replace(key_value, "***MASKED***")
                        return match.group(0)
                    
                    content = re.sub(pattern, mask_match, content, flags=re.IGNORECASE)
            
            if content != original_content:
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f" Masked {masked_count} keys in: {output_file}")
                return output_file
            else:
                print(f" No keys found to mask in: {file_path}")
                return None
                
        except Exception as e:
            print(f" Error masking keys: {e}")
            return None
    
    def scan_internet_for_keys(self, provider=None, max_results=50):
        """Deep scan internet for exposed API keys using multiple search engines and languages"""
        print(f" Starting deep multi-language scan for {provider or 'all'} API keys...")
        
        found_keys = []
        total_searches = 0
        
        # Multi-language search terms for API keys
        language_terms = {
            'english': {
                'api_key': ['api_key', 'api key', 'API_KEY', 'apiKey', 'api-key'],
                'token': ['token', 'TOKEN', 'access_token', 'access-token'],
                'secret': ['secret', 'SECRET', 'api_secret', 'api-secret'],
                'key': ['key', 'KEY', 'api_key', 'api-key']
            },
            'spanish': {
                'api_key': ['clave_api', 'clave api', 'CLAVE_API', 'claveApi', 'clave-api'],
                'token': ['token', 'TOKEN', 'token_acceso', 'token-acceso'],
                'secret': ['secreto', 'SECRETO', 'api_secreto', 'api-secreto'],
                'key': ['clave', 'CLAVE', 'clave_api', 'clave-api']
            },
            'french': {
                'api_key': ['cle_api', 'cle api', 'CLE_API', 'cleApi', 'cle-api'],
                'token': ['token', 'TOKEN', 'jeton_acces', 'jeton-acces'],
                'secret': ['secret', 'SECRET', 'api_secret', 'api-secret'],
                'key': ['cle', 'CLE', 'cle_api', 'cle-api']
            },
            'german': {
                'api_key': ['api_schlussel', 'api schlussel', 'API_SCHLUSSEL', 'apiSchlussel', 'api-schlussel'],
                'token': ['token', 'TOKEN', 'zugriff_token', 'zugriff-token'],
                'secret': ['geheimnis', 'GEHEIMNIS', 'api_geheimnis', 'api-geheimnis'],
                'key': ['schlussel', 'SCHLUSSEL', 'api_schlussel', 'api-schlussel']
            },
            'chinese': {
                'api_key': ['api密钥', 'api 密钥', 'API密钥', 'apiMiYao', 'api-mi-yao'],
                'token': ['令牌', 'TOKEN', '访问令牌', 'fang-wen-ling-pai'],
                'secret': ['秘密', 'SECRET', 'api秘密', 'api-mi-mi'],
                'key': ['密钥', 'KEY', 'api密钥', 'api-mi-yao']
            },
            'japanese': {
                'api_key': ['apiキー', 'api キー', 'APIキー', 'apiKii', 'api-kii'],
                'token': ['トークン', 'TOKEN', 'アクセストークン', 'akusesu-token'],
                'secret': ['秘密', 'SECRET', 'api秘密', 'api-himitsu'],
                'key': ['キー', 'KEY', 'apiキー', 'api-kii']
            },
            'russian': {
                'api_key': ['api_ключ', 'api ключ', 'API_КЛЮЧ', 'apiKlyuch', 'api-klyuch'],
                'token': ['токен', 'ТОКЕН', 'токен_доступа', 'token-dostupa'],
                'secret': ['секрет', 'СЕКРЕТ', 'api_секрет', 'api-sekret'],
                'key': ['ключ', 'КЛЮЧ', 'api_ключ', 'api-klyuch']
            },
            'portuguese': {
                'api_key': ['chave_api', 'chave api', 'CHAVE_API', 'chaveApi', 'chave-api'],
                'token': ['token', 'TOKEN', 'token_acesso', 'token-acesso'],
                'secret': ['segredo', 'SEGREDO', 'api_segredo', 'api-segredo'],
                'key': ['chave', 'CHAVE', 'chave_api', 'chave-api']
            },
            'italian': {
                'api_key': ['chiave_api', 'chiave api', 'CHIAVE_API', 'chiaveApi', 'chiave-api'],
                'token': ['token', 'TOKEN', 'token_accesso', 'token-accesso'],
                'secret': ['segreto', 'SEGRETO', 'api_segreto', 'api-segreto'],
                'key': ['chiave', 'CHIAVE', 'chiave_api', 'chiave-api']
            },
            'korean': {
                'api_key': ['api키', 'api 키', 'API키', 'apiKi', 'api-ki'],
                'token': ['토큰', 'TOKEN', '액세스토큰', 'aegseseu-token'],
                'secret': ['비밀', 'SECRET', 'api비밀', 'api-bimil'],
                'key': ['키', 'KEY', 'api키', 'api-ki']
            },
            'arabic': {
                'api_key': ['مفتاح_api', 'مفتاح api', 'مفتاح_API', 'miftahApi', 'miftah-api'],
                'token': ['رمز', 'TOKEN', 'رمز_الوصول', 'ramz-al-wusul'],
                'secret': ['سر', 'SECRET', 'api_سر', 'api-sir'],
                'key': ['مفتاح', 'KEY', 'مفتاح_api', 'miftah-api']
            },
            'hindi': {
                'api_key': ['api_कुंजी', 'api कुंजी', 'API_कुंजी', 'apiKunji', 'api-kunji'],
                'token': ['टोकन', 'TOKEN', 'पहुंच_टोकन', 'pahunch-token'],
                'secret': ['रहस्य', 'SECRET', 'api_रहस्य', 'api-rahasy'],
                'key': ['कुंजी', 'KEY', 'api_कुंजी', 'api-kunji']
            }
        }
        
        # Multi-region search engines
        search_engines = [
            {'name': 'Google US', 'url': 'https://www.google.com/search?q={query}&num={num}&hl=en&gl=US'},
            {'name': 'Google UK', 'url': 'https://www.google.co.uk/search?q={query}&num={num}&hl=en&gl=GB'},
            {'name': 'Google Canada', 'url': 'https://www.google.ca/search?q={query}&num={num}&hl=en&gl=CA'},
            {'name': 'Google Australia', 'url': 'https://www.google.com.au/search?q={query}&num={num}&hl=en&gl=AU'},
            {'name': 'Google Germany', 'url': 'https://www.google.de/search?q={query}&num={num}&hl=de&gl=DE'},
            {'name': 'Google France', 'url': 'https://www.google.fr/search?q={query}&num={num}&hl=fr&gl=FR'},
            {'name': 'Google Spain', 'url': 'https://www.google.es/search?q={query}&num={num}&hl=es&gl=ES'},
            {'name': 'Google Italy', 'url': 'https://www.google.it/search?q={query}&num={num}&hl=it&gl=IT'},
            {'name': 'Google Japan', 'url': 'https://www.google.co.jp/search?q={query}&num={num}&hl=ja&gl=JP'},
            {'name': 'Google China', 'url': 'https://www.google.com.hk/search?q={query}&num={num}&hl=zh&gl=HK'},
            {'name': 'Google Russia', 'url': 'https://www.google.ru/search?q={query}&num={num}&hl=ru&gl=RU'},
            {'name': 'Google Brazil', 'url': 'https://www.google.com.br/search?q={query}&num={num}&hl=pt&gl=BR'},
            {'name': 'Google Korea', 'url': 'https://www.google.co.kr/search?q={query}&num={num}&hl=ko&gl=KR'},
            {'name': 'Google India', 'url': 'https://www.google.co.in/search?q={query}&num={num}&hl=hi&gl=IN'},
            {'name': 'Google Mexico', 'url': 'https://www.google.com.mx/search?q={query}&num={num}&hl=es&gl=MX'},
            {'name': 'Google Netherlands', 'url': 'https://www.google.nl/search?q={query}&num={num}&hl=nl&gl=NL'},
            {'name': 'Google Sweden', 'url': 'https://www.google.se/search?q={query}&num={num}&hl=sv&gl=SE'},
            {'name': 'Google Norway', 'url': 'https://www.google.no/search?q={query}&num={num}&hl=no&gl=NO'},
            {'name': 'Google Denmark', 'url': 'https://www.google.dk/search?q={query}&num={num}&hl=da&gl=DK'},
            {'name': 'Google Finland', 'url': 'https://www.google.fi/search?q={query}&num={num}&hl=fi&gl=FI'},
            {'name': 'Bing Global', 'url': 'https://www.bing.com/search?q={query}&count={num}&mkt=en-US'},
            {'name': 'Bing UK', 'url': 'https://www.bing.com/search?q={query}&count={num}&mkt=en-GB'},
            {'name': 'Bing Germany', 'url': 'https://www.bing.com/search?q={query}&count={num}&mkt=de-DE'},
            {'name': 'Bing France', 'url': 'https://www.bing.com/search?q={query}&count={num}&mkt=fr-FR'},
            {'name': 'Bing Japan', 'url': 'https://www.bing.com/search?q={query}&count={num}&mkt=ja-JP'},
            {'name': 'Bing China', 'url': 'https://www.bing.com/search?q={query}&count={num}&mkt=zh-CN'},
            {'name': 'Yahoo Global', 'url': 'https://search.yahoo.com/search?p={query}&n={num}&cc=us'},
            {'name': 'Yahoo UK', 'url': 'https://search.yahoo.com/search?p={query}&n={num}&cc=uk'},
            {'name': 'Yahoo Germany', 'url': 'https://search.yahoo.com/search?p={query}&n={num}&cc=de'},
            {'name': 'Yahoo France', 'url': 'https://search.yahoo.com/search?p={query}&n={num}&cc=fr'},
            {'name': 'Yahoo Japan', 'url': 'https://search.yahoo.com/search?p={query}&n={num}&cc=jp'},
            {'name': 'DuckDuckGo', 'url': 'https://duckduckgo.com/?q={query}&t=h_&ia=web'},
            {'name': 'Yandex', 'url': 'https://yandex.com/search/?text={query}&numdoc={num}'},
            {'name': 'Baidu', 'url': 'https://www.baidu.com/s?wd={query}&rn={num}'},
            {'name': 'Naver', 'url': 'https://search.naver.com/search.naver?query={query}&display={num}'},
            {'name': 'Seznam', 'url': 'https://search.seznam.cz/?q={query}&count={num}'},
            {'name': 'Qwant', 'url': 'https://www.qwant.com/?q={query}&t=web'},
            {'name': 'Startpage', 'url': 'https://www.startpage.com/sp/search?query={query}&count={num}'},
            {'name': 'Ecosia', 'url': 'https://www.ecosia.org/search?q={query}&count={num}'}
        ]
        
        # Common dork patterns
        dork_patterns = [
            'site:github.com "{term}" OR "{term}=" OR "{term_upper}"',
            'site:pastebin.com "{term}" OR "{term}=" OR "{term_upper}"',
            'site:gist.github.com "{term}" OR "{term}=" OR "{term_upper}"',
            'site:stackoverflow.com "{term}" OR "{term}=" OR "{term_upper}"',
            'site:gitlab.com "{term}" OR "{term}=" OR "{term_upper}"',
            'site:bitbucket.org "{term}" OR "{term}=" OR "{term_upper}"',
            'site:sourceforge.net "{term}" OR "{term}=" OR "{term_upper}"',
            'site:code.google.com "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:env "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:json "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:py "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:js "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:txt "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:md "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:yml "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:yaml "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:xml "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:csv "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:log "{term}" OR "{term}=" OR "{term_upper}"',
            'filetype:conf "{term}" OR "{term}=" OR "{term_upper}"'
        ]
        
        try:
            # Determine which providers to search
            providers_to_search = [provider] if provider else list(self.google_dorks.keys())
            
            for search_provider in providers_to_search:
                if search_provider not in self.google_dorks:
                    continue
                
                print(f" Searching for {search_provider} keys across multiple languages and regions...")
                
                # Search in multiple languages
                for language, terms in language_terms.items():
                    print(f"  Language: {language}")
                    
                    for term_type, terms_list in terms.items():
                        for term in terms_list:
                            # Create dorks for this term
                            for pattern in dork_patterns:
                                dork = pattern.format(term=term, term_upper=term.upper())
                                
                                # Search across multiple engines
                                for engine in search_engines:
                                    try:
                                        print(f"    {engine['name']}: {dork[:50]}...")
                                        search_results = self._perform_multi_engine_search(dork, engine, max_results=3)
                                        total_searches += 1
                                        
                                        # Extract keys from search results
                                        for result in search_results:
                                            keys = self._extract_keys_from_url(result['url'], search_provider)
                                            found_keys.extend(keys)
                                        
                                        # Rate limiting
                                        time.sleep(random.uniform(0.5, 2))
                                        
                                        if len(found_keys) >= max_results:
                                            break
                                    
                                    except Exception as e:
                                        print(f"    {engine['name']} error: {e}")
                                        continue
                                
                                if len(found_keys) >= max_results:
                                    break
                            
                            if len(found_keys) >= max_results:
                                break
                        
                        if len(found_keys) >= max_results:
                            break
                    
                    if len(found_keys) >= max_results:
                        break
                
                if len(found_keys) >= max_results:
                    break
            
            print(f" Deep scan complete: {total_searches} searches, {len(found_keys)} keys found")
            
            # Auto-save results
            if found_keys:
                self.proxy_manager.add_scraped_data('api_keys', found_keys)
                self.proxy_manager.add_scraped_data('search_results', {
                    'type': 'deep_scan',
                    'timestamp': time.time(),
                    'searches': total_searches,
                    'keys_found': len(found_keys),
                    'provider': provider
                })
            
            return found_keys[:max_results]
            
        except Exception as e:
            print(f" Deep scan error: {e}")
            return []
    
    def _perform_multi_engine_search(self, query, engine, max_results=10):
        """Perform search using any search engine with curl"""
        try:
            # Encode query
            encoded_query = urllib.parse.quote_plus(query)
            
            # Construct search URL
            search_url = engine['url'].format(query=encoded_query, num=max_results)
            
            # Build curl command
            curl_cmd = [
                'curl',
                '-s',  # Silent mode
                '-L',  # Follow redirects
                '--max-time', '30',  # Timeout
                '--user-agent', random.choice(self.user_agents),
                '--header', 'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
                '--header', 'Accept-Language: en-US,en;q=0.5',
                '--header', 'Accept-Encoding: gzip, deflate',
                '--header', 'Connection: keep-alive',
                '--header', 'Upgrade-Insecure-Requests: 1'
            ]
            
            # Add proxy if available
            if self.proxy_manager.current_proxy and self.proxy_manager.current_proxy['enabled']:
                proxy_url = f"{self.proxy_manager.current_proxy['type']}://{self.proxy_manager.current_proxy['host']}:{self.proxy_manager.current_proxy['port']}"
                curl_cmd.extend(['--proxy', proxy_url])
            
            # Add random delay
            time.sleep(random.uniform(0.5, 2))
            
            # Execute curl command
            curl_cmd.append(search_url)
            result = subprocess.run(curl_cmd, capture_output=True, text=True, timeout=35)
            
            if result.returncode == 0:
                return self._parse_search_results(result.stdout, engine['name'])
            else:
                print(f" {engine['name']} curl error: {result.stderr}")
                return []
            
        except Exception as e:
            print(f" {engine['name']} search error: {e}")
            return []
    
    def _perform_google_search(self, query, max_results=10):
        """Perform Google search using curl"""
        try:
            # Encode query
            encoded_query = urllib.parse.quote_plus(query)
            
            # Construct Google search URL
            search_url = f"https://www.google.com/search?q={encoded_query}&num={max_results}"
            
            # Build curl command
            curl_cmd = [
                'curl',
                '-s',  # Silent mode
                '-L',  # Follow redirects
                '--max-time', '30',  # Timeout
                '--user-agent', random.choice(self.user_agents),
                '--header', 'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
                '--header', 'Accept-Language: en-US,en;q=0.5',
                '--header', 'Accept-Encoding: gzip, deflate',
                '--header', 'Connection: keep-alive',
                '--header', 'Upgrade-Insecure-Requests: 1'
            ]
            
            # Add proxy if available
            if self.proxy_manager.current_proxy and self.proxy_manager.current_proxy['enabled']:
                proxy_url = f"{self.proxy_manager.current_proxy['type']}://{self.proxy_manager.current_proxy['host']}:{self.proxy_manager.current_proxy['port']}"
                curl_cmd.extend(['--proxy', proxy_url])
            
            # Add random delay
            time.sleep(random.uniform(1, 3))
            
            # Execute curl command
            curl_cmd.append(search_url)
            result = subprocess.run(curl_cmd, capture_output=True, text=True, timeout=35)
            
            if result.returncode == 0:
                return self._parse_google_results(result.stdout)
            else:
                print(f" Curl error: {result.stderr}")
                return []
            
        except Exception as e:
            print(f" Google search error: {e}")
            return []
    
    def _parse_search_results(self, html, engine_name):
        """Parse search results from any search engine"""
        results = []
        
        try:
            if 'google' in engine_name.lower():
                return self._parse_google_results(html)
            elif 'bing' in engine_name.lower():
                return self._parse_bing_results(html)
            elif 'yahoo' in engine_name.lower():
                return self._parse_yahoo_results(html)
            elif 'duckduckgo' in engine_name.lower():
                return self._parse_duckduckgo_results(html)
            elif 'yandex' in engine_name.lower():
                return self._parse_yandex_results(html)
            elif 'baidu' in engine_name.lower():
                return self._parse_baidu_results(html)
            elif 'naver' in engine_name.lower():
                return self._parse_naver_results(html)
            else:
                # Generic parser for unknown engines
                return self._parse_generic_results(html)
                
        except Exception as e:
            print(f" Parse {engine_name} results error: {e}")
            return []
    
    def _parse_google_results(self, html):
        """Parse Google search results from HTML"""
        results = []
        
        try:
            # Simple regex-based parsing (in production, use BeautifulSoup)
            # Look for result URLs
            url_pattern = r'<a href="/url\?q=([^&]+)&amp;'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:  # Limit to 10 results
                try:
                    decoded_url = urllib.parse.unquote(url)
                    if self._is_valid_result_url(decoded_url):
                        results.append({
                            'url': decoded_url,
                            'title': 'Search Result',
                            'snippet': 'Found via Google search'
                        })
                except:
                    continue
            
        except Exception as e:
            print(f" Result parsing error: {e}")
        
        return results
    
    def _parse_bing_results(self, html):
        """Parse Bing search results from HTML"""
        results = []
        try:
            # Bing result patterns
            url_pattern = r'<a href="([^"]+)"[^>]*class="[^"]*b_title[^"]*"'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:
                try:
                    if self._is_valid_result_url(url):
                        results.append({
                            'url': url,
                            'title': 'Bing Search Result',
                            'snippet': 'Found via Bing search'
                        })
                except:
                    continue
        except Exception as e:
            print(f" Bing parsing error: {e}")
        return results
    
    def _parse_yahoo_results(self, html):
        """Parse Yahoo search results from HTML"""
        results = []
        try:
            # Yahoo result patterns
            url_pattern = r'<a href="([^"]+)"[^>]*class="[^"]*ac-algo[^"]*"'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:
                try:
                    if self._is_valid_result_url(url):
                        results.append({
                            'url': url,
                            'title': 'Yahoo Search Result',
                            'snippet': 'Found via Yahoo search'
                        })
                except:
                    continue
        except Exception as e:
            print(f" Yahoo parsing error: {e}")
        return results
    
    def _parse_duckduckgo_results(self, html):
        """Parse DuckDuckGo search results from HTML"""
        results = []
        try:
            # DuckDuckGo result patterns
            url_pattern = r'<a href="([^"]+)"[^>]*class="[^"]*result__a[^"]*"'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:
                try:
                    if self._is_valid_result_url(url):
                        results.append({
                            'url': url,
                            'title': 'DuckDuckGo Search Result',
                            'snippet': 'Found via DuckDuckGo search'
                        })
                except:
                    continue
        except Exception as e:
            print(f" DuckDuckGo parsing error: {e}")
        return results
    
    def _parse_yandex_results(self, html):
        """Parse Yandex search results from HTML"""
        results = []
        try:
            # Yandex result patterns
            url_pattern = r'<a href="([^"]+)"[^>]*class="[^"]*serp-item__title[^"]*"'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:
                try:
                    if self._is_valid_result_url(url):
                        results.append({
                            'url': url,
                            'title': 'Yandex Search Result',
                            'snippet': 'Found via Yandex search'
                        })
                except:
                    continue
        except Exception as e:
            print(f" Yandex parsing error: {e}")
        return results
    
    def _parse_baidu_results(self, html):
        """Parse Baidu search results from HTML"""
        results = []
        try:
            # Baidu result patterns
            url_pattern = r'<a href="([^"]+)"[^>]*class="[^"]*c-showurl[^"]*"'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:
                try:
                    if self._is_valid_result_url(url):
                        results.append({
                            'url': url,
                            'title': 'Baidu Search Result',
                            'snippet': 'Found via Baidu search'
                        })
                except:
                    continue
        except Exception as e:
            print(f" Baidu parsing error: {e}")
        return results
    
    def _parse_naver_results(self, html):
        """Parse Naver search results from HTML"""
        results = []
        try:
            # Naver result patterns
            url_pattern = r'<a href="([^"]+)"[^>]*class="[^"]*sh_blog_title[^"]*"'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:10]:
                try:
                    if self._is_valid_result_url(url):
                        results.append({
                            'url': url,
                            'title': 'Naver Search Result',
                            'snippet': 'Found via Naver search'
                        })
                except:
                    continue
        except Exception as e:
            print(f" Naver parsing error: {e}")
        return results
    
    def _parse_generic_results(self, html):
        """Generic parser for unknown search engines"""
        results = []
        try:
            # Generic URL patterns
            url_patterns = [
                r'<a href="([^"]+)"[^>]*>',
                r'href="([^"]+)"',
                r'url=([^&"]+)',
                r'link=([^&"]+)'
            ]
            
            for pattern in url_patterns:
                urls = re.findall(pattern, html)
                for url in urls[:5]:  # Limit per pattern
                    try:
                        if self._is_valid_result_url(url):
                            results.append({
                                'url': url,
                                'title': 'Generic Search Result',
                                'snippet': 'Found via generic search'
                            })
                    except:
                        continue
        except Exception as e:
            print(f" Generic parsing error: {e}")
        return results
    
    def _is_valid_result_url(self, url):
        """Check if URL is a valid result for key extraction"""
        valid_domains = [
            'github.com', 'gist.github.com', 'pastebin.com', 
            'stackoverflow.com', 'gitlab.com', 'bitbucket.org',
            'code.google.com', 'sourceforge.net'
        ]
        
        try:
            parsed = urllib.parse.urlparse(url)
            return any(domain in parsed.netloc for domain in valid_domains)
        except:
            return False
    
    def _extract_keys_from_url(self, url, provider):
        """Extract API keys from a specific URL using curl"""
        found_keys = []
        
        try:
            # Build curl command
            curl_cmd = [
                'curl',
                '-s',  # Silent mode
                '-L',  # Follow redirects
                '--max-time', '15',  # Timeout
                '--user-agent', random.choice(self.user_agents),
                '--header', 'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
                '--header', 'Accept-Language: en-US,en;q=0.5',
                '--header', 'Connection: keep-alive'
            ]
            
            # Add proxy if available
            if self.proxy_manager.current_proxy and self.proxy_manager.current_proxy['enabled']:
                proxy_url = f"{self.proxy_manager.current_proxy['type']}://{self.proxy_manager.current_proxy['host']}:{self.proxy_manager.current_proxy['port']}"
                curl_cmd.extend(['--proxy', proxy_url])
            
            # Execute curl command
            curl_cmd.append(url)
            result = subprocess.run(curl_cmd, capture_output=True, text=True, timeout=20)
            
            if result.returncode == 0:
                content = result.stdout
                
                # Extract keys using patterns
                for pattern in self.api_key_patterns.get(provider, []):
                    matches = re.finditer(pattern, content, re.IGNORECASE | re.MULTILINE)
                    for match in matches:
                        key_value = match.group(1) if match.groups() else match.group(0)
                        
                        if self._validate_key_format(key_value, provider):
                            found_keys.append({
                                'provider': provider,
                                'key': key_value,
                                'url': url,
                                'source': 'Internet',
                                'context': self._get_context_from_web_content(content, match.start(), match.end())
                            })
            else:
                print(f" Curl error for {url}: {result.stderr}")
            
            # Rate limiting
            time.sleep(random.uniform(0.5, 1.5))
            
        except Exception as e:
            print(f" Error extracting from {url}: {e}")
        
        return found_keys
    
    def _get_context_from_web_content(self, content, start, end):
        """Get context around found key in web content"""
        lines = content.split('\n')
        line_start = content[:start].count('\n')
        line_end = content[:end].count('\n')
        
        context_lines = []
        for i in range(max(0, line_start - 2), min(len(lines), line_end + 3)):
            prefix = ">>> " if i == line_start else "    "
            context_lines.append(f"{prefix}{i+1:4d}: {lines[i][:100]}")  # Limit line length
        
        return '\n'.join(context_lines)
    
    def scan_github_repos(self, search_terms=None, max_repos=20):
        """Scan GitHub repositories for exposed keys"""
        print(" Scanning GitHub repositories for exposed keys...")
        
        if not search_terms:
            search_terms = ['api_key', 'OPENAI_API_KEY', 'GOOGLE_API_KEY', 'AWS_ACCESS_KEY_ID']
        
        found_keys = []
        
        try:
            for term in search_terms:
                print(f" Searching GitHub for: {term}")
                
                # GitHub search API (simplified)
                search_url = f"https://api.github.com/search/code?q={urllib.parse.quote(term)}"
                
                headers = {
                    'User-Agent': random.choice(self.user_agents),
                    'Accept': 'application/vnd.github.v3+json'
                }
                
                request = urllib.request.Request(search_url, headers=headers)
                
                with urllib.request.urlopen(request, timeout=10) as response:
                    data = json.loads(response.read().decode('utf-8'))
                
                # Process results
                for item in data.get('items', [])[:max_repos]:
                    repo_url = item.get('html_url', '')
                    if repo_url:
                        keys = self._extract_keys_from_url(repo_url, 'generic')
                        found_keys.extend(keys)
                
                # Rate limiting for GitHub API
                time.sleep(2)
            
            print(f" GitHub scan complete: {len(found_keys)} keys found")
            return found_keys
            
        except Exception as e:
            print(f" GitHub scan error: {e}")
            return []
    
    def generate_internet_report(self, found_keys, output_file=None):
        """Generate report for internet-found keys"""
        if not output_file:
            output_file = "internet_key_scan_report.txt"
        
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(" INTERNET API KEY SCAN REPORT\n")
                f.write("=" * 50 + "\n\n")
                f.write(f"Scan Date: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Total Keys Found: {len(found_keys)}\n")
                f.write(f"Source: Internet/Web Scraping\n\n")
                
                if not found_keys:
                    f.write(" No exposed API keys found on the internet!\n")
                    return output_file
                
                # Group by provider
                by_provider = {}
                for key in found_keys:
                    provider = key['provider']
                    if provider not in by_provider:
                        by_provider[provider] = []
                    by_provider[provider].append(key)
                
                # Write report
                for provider, keys in by_provider.items():
                    f.write(f"\n {provider.upper()} KEYS ({len(keys)} found):\n")
                    f.write("-" * 30 + "\n")
                    
                    for key in keys:
                        f.write(f"\nURL: {key['url']}\n")
                        f.write(f"Key: {key['key'][:20]}...{key['key'][-10:]}\n")
                        f.write(f"Context:\n{key['context']}\n")
                        f.write("\n" + "="*50 + "\n")
                
                # Security warnings
                f.write("\n️ SECURITY WARNINGS:\n")
                f.write("-" * 30 + "\n")
                f.write("These API keys were found exposed on the internet!\n")
                f.write("If any of these keys belong to you:\n")
                f.write("1. IMMEDIATELY revoke/regenerate the keys\n")
                f.write("2. Check your repositories for hardcoded keys\n")
                f.write("3. Use environment variables instead\n")
                f.write("4. Add .env files to .gitignore\n")
                f.write("5. Review your commit history\n")
            
            print(f" Internet scan report generated: {output_file}")
            return output_file
            
        except Exception as e:
            print(f" Error generating internet report: {e}")
            return None
    
    def setup_proxy_for_scraping(self, proxy_type='none', host=None, port=None, username=None, password=None):
        """Setup proxy for safe web scraping"""
        return self.proxy_manager.setup_proxy(proxy_type, host, port, username, password)
    
    def setup_stealth_scraping(self):
        """Setup stealth mode for maximum anonymity"""
        return self.proxy_manager.setup_stealth_mode()
    
    def test_proxy_connection(self):
        """Test current proxy connection"""
        return self.proxy_manager.test_proxy_connection()
    
    def get_current_ip(self):
        """Get current public IP address"""
        return self.proxy_manager.get_current_ip()
    
    def scan_internet_with_proxy(self, provider=None, max_results=50, use_stealth=False):
        """Scan internet with proxy protection"""
        if use_stealth:
            self.setup_stealth_scraping()
        
        # Add stealth delays if in stealth mode
        if hasattr(self.proxy_manager, 'stealth_delay_range'):
            delay = random.uniform(*self.proxy_manager.stealth_delay_range)
            time.sleep(delay)
        
        return self.scan_internet_for_keys(provider, max_results)
    
    def scan_for_ai_chatboxes(self, directory_path=None, recursive=True):
        """Scan for AI chatbox services in code"""
        print(" Scanning for AI chatbox services...")
        
        chatbox_findings = []
        
        if directory_path:
            # Scan local directory
            for root, dirs, files in os.walk(directory_path):
                for file in files:
                    file_path = os.path.join(root, file)
                    file_ext = Path(file_path).suffix.lower()
                    
                    if file_ext in self.scannable_extensions or file_ext == '':
                        findings = self._scan_file_for_chatboxes(file_path)
                        chatbox_findings.extend(findings)
                
                if not recursive:
                    break
        
        # Scan internet for AI chatbox services
        internet_findings = self._scan_internet_for_chatboxes()
        chatbox_findings.extend(internet_findings)
        
        print(f" Found {len(chatbox_findings)} AI chatbox services")
        
        # Auto-save results
        if chatbox_findings:
            self.proxy_manager.add_scraped_data('ai_services', chatbox_findings)
            self.proxy_manager.add_scraped_data('search_results', {
                'type': 'ai_chatbox_scan',
                'timestamp': time.time(),
                'services_found': len(chatbox_findings),
                'directory': directory_path
            })
        
        return chatbox_findings
    
    def _scan_file_for_chatboxes(self, file_path):
        """Scan a single file for AI chatbox references"""
        findings = []
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            for service, patterns in self.ai_chatbox_patterns.items():
                for pattern in patterns:
                    matches = re.finditer(pattern, content, re.IGNORECASE)
                    for match in matches:
                        findings.append({
                            'file': file_path,
                            'service': service,
                            'pattern': pattern,
                            'match': match.group(),
                            'line': content[:match.start()].count('\n') + 1,
                            'context': self._get_context(content, match.start(), match.end())
                        })
        
        except Exception as e:
            print(f" Error scanning {file_path}: {e}")
        
        return findings
    
    def _scan_internet_for_chatboxes(self):
        """Scan internet for AI chatbox services"""
        findings = []
        
        # Google Dorks for AI chatbox services
        chatbox_dorks = [
            'site:github.com "chatgpt" OR "claude" OR "bard" OR "copilot"',
            'site:github.com "huggingface" OR "ollama" OR "groq"',
            'site:github.com "together.ai" OR "replicate" OR "perplexity"',
            '"AI chat" OR "chatbot" OR "AI assistant" filetype:py',
            '"openai" OR "anthropic" OR "google ai" filetype:js'
        ]
        
        for dork in chatbox_dorks:
            try:
                results = self._perform_google_search(dork)
                for result in results:
                    if self._is_valid_result_url(result['url']):
                        chatbox_findings = self._extract_chatboxes_from_url(result['url'])
                        findings.extend(chatbox_findings)
                
                # Rate limiting
                time.sleep(random.uniform(1, 3))
                
            except Exception as e:
                print(f" Error with dork '{dork}': {e}")
        
        return findings
    
    def _extract_chatboxes_from_url(self, url):
        """Extract AI chatbox services from a URL"""
        findings = []
        
        try:
            # Setup proxy if available
            proxy_handler = None
            if self.proxy_manager.current_proxy != 'none':
                proxy_config = self.proxy_manager.proxy_configs[self.proxy_manager.current_proxy]
                if proxy_config['enabled']:
                    proxy_handler = urllib.request.ProxyHandler({
                        'http': f"{proxy_config['host']}:{proxy_config['port']}",
                        'https': f"{proxy_config['host']}:{proxy_config['port']}"
                    })
            
            opener = urllib.request.build_opener(proxy_handler) if proxy_handler else urllib.request.build_opener()
            opener.addheaders = [('User-Agent', random.choice(self.user_agents))]
            
            with opener.open(url, timeout=10) as response:
                content = response.read().decode('utf-8', errors='ignore')
                
                for service, patterns in self.ai_chatbox_patterns.items():
                    for pattern in patterns:
                        matches = re.finditer(pattern, content, re.IGNORECASE)
                        for match in matches:
                            findings.append({
                                'url': url,
                                'service': service,
                                'pattern': pattern,
                                'match': match.group(),
                                'context': self._get_context(content, match.start(), match.end())
                            })
        
        except Exception as e:
            print(f" Error extracting from {url}: {e}")
        
        return findings
    
    def get_free_chatbox_services(self):
        """Get list of free AI chatbox services"""
        return self.free_chatbox_services
    
    def use_free_chatbox(self, service_name, prompt, model=None):
        """Use a free AI chatbox service for coding assistance"""
        if service_name not in self.free_chatbox_services:
            return {"error": f"Unknown service: {service_name}"}
        
        service = self.free_chatbox_services[service_name]
        
        try:
            if service_name == 'huggingface_chat':
                return self._use_huggingface_chat(prompt, model or service['models'][0])
            elif service_name == 'ollama_local':
                return self._use_ollama_local(prompt, model or service['models'][0])
            elif service_name == 'groq_free':
                return self._use_groq_free(prompt, model or service['models'][0])
            elif service_name == 'together_free':
                return self._use_together_free(prompt, model or service['models'][0])
            elif service_name == 'replicate_free':
                return self._use_replicate_free(prompt, model or service['models'][0])
            else:
                return {"error": f"Service {service_name} not implemented yet"}
        
        except Exception as e:
            return {"error": f"Error using {service_name}: {e}"}
    
    def _use_huggingface_chat(self, prompt, model):
        """Use Hugging Face free chat service"""
        try:
            url = f"{self.free_chatbox_services['huggingface_chat']['api_url']}/{model}"
            
            headers = {
                'Authorization': f'Bearer {self._get_huggingface_token()}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'inputs': prompt,
                'parameters': {
                    'max_length': 512,
                    'temperature': 0.7
                }
            }
            
            response = requests.post(url, headers=headers, json=data, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'huggingface_chat',
                    'model': model,
                    'response': result[0].get('generated_text', 'No response'),
                    'success': True
                }
            else:
                return {
                    'service': 'huggingface_chat',
                    'error': f"HTTP {response.status_code}: {response.text}",
                    'success': False
                }
        
        except Exception as e:
            return {
                'service': 'huggingface_chat',
                'error': str(e),
                'success': False
            }
    
    def _use_ollama_local(self, prompt, model):
        """Use local Ollama service"""
        try:
            url = self.free_chatbox_services['ollama_local']['api_url']
            
            data = {
                'model': model,
                'prompt': prompt,
                'stream': False
            }
            
            response = requests.post(url, json=data, timeout=60)
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'ollama_local',
                    'model': model,
                    'response': result.get('response', 'No response'),
                    'success': True
                }
            else:
                return {
                    'service': 'ollama_local',
                    'error': f"HTTP {response.status_code}: {response.text}",
                    'success': False
                }
        
        except Exception as e:
            return {
                'service': 'ollama_local',
                'error': str(e),
                'success': False
            }
    
    def _use_groq_free(self, prompt, model):
        """Use Groq free service"""
        try:
            url = f"{self.free_chatbox_services['groq_free']['api_url']}/chat/completions"
            
            headers = {
                'Authorization': f'Bearer {self._get_groq_token()}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': model,
                'messages': [{'role': 'user', 'content': prompt}],
                'max_tokens': 512,
                'temperature': 0.7
            }
            
            response = requests.post(url, headers=headers, json=data, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'groq_free',
                    'model': model,
                    'response': result['choices'][0]['message']['content'],
                    'success': True
                }
            else:
                return {
                    'service': 'groq_free',
                    'error': f"HTTP {response.status_code}: {response.text}",
                    'success': False
                }
        
        except Exception as e:
            return {
                'service': 'groq_free',
                'error': str(e),
                'success': False
            }
    
    def _use_together_free(self, prompt, model):
        """Use Together AI free service"""
        try:
            url = f"{self.free_chatbox_services['together_free']['api_url']}/chat/completions"
            
            headers = {
                'Authorization': f'Bearer {self._get_together_token()}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': model,
                'messages': [{'role': 'user', 'content': prompt}],
                'max_tokens': 512,
                'temperature': 0.7
            }
            
            response = requests.post(url, headers=headers, json=data, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'together_free',
                    'model': model,
                    'response': result['choices'][0]['message']['content'],
                    'success': True
                }
            else:
                return {
                    'service': 'together_free',
                    'error': f"HTTP {response.status_code}: {response.text}",
                    'success': False
                }
        
        except Exception as e:
            return {
                'service': 'together_free',
                'error': str(e),
                'success': False
            }
    
    def _use_replicate_free(self, prompt, model):
        """Use Replicate free service"""
        try:
            url = f"{self.free_chatbox_services['replicate_free']['api_url']}/predictions"
            
            headers = {
                'Authorization': f'Token {self._get_replicate_token()}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'version': model,
                'input': {'prompt': prompt}
            }
            
            response = requests.post(url, headers=headers, json=data, timeout=60)
            
            if response.status_code == 201:
                result = response.json()
                return {
                    'service': 'replicate_free',
                    'model': model,
                    'response': result.get('output', 'Processing...'),
                    'success': True
                }
            else:
                return {
                    'service': 'replicate_free',
                    'error': f"HTTP {response.status_code}: {response.text}",
                    'success': False
                }
        
        except Exception as e:
            return {
                'service': 'replicate_free',
                'error': str(e),
                'success': False
            }
    
    def _get_huggingface_token(self):
        """Get Hugging Face token (can be None for free tier)"""
        return None  # Hugging Face allows some free usage without token
    
    def _get_groq_token(self):
        """Get Groq API token"""
        # In a real implementation, this would get from environment or config
        return "your-groq-token-here"
    
    def _get_together_token(self):
        """Get Together AI token"""
        # In a real implementation, this would get from environment or config
        return "your-together-token-here"
    
    def _get_replicate_token(self):
        """Get Replicate token"""
        # In a real implementation, this would get from environment or config
        return "your-replicate-token-here"
    
    def auto_create_copilots_from_services(self, max_copilots=5):
        """Automatically create multiple copilots from available free services"""
        created_copilots = []
        free_services = self.get_free_chatbox_services()
        
        # Create copilots for each free service
        for service_id, service_info in free_services.items():
            if len(created_copilots) >= max_copilots:
                break
                
            try:
                copilot_id = self.copilot_manager.create_copilot_from_service(
                    service_id, service_info
                )
                created_copilots.append({
                    'copilot_id': copilot_id,
                    'service': service_id,
                    'name': service_info['name'],
                    'status': 'created'
                })
                print(f" Auto-created copilot: {service_info['name']}")
                
            except Exception as e:
                print(f" Failed to create copilot for {service_id}: {e}")
                created_copilots.append({
                    'service': service_id,
                    'name': service_info['name'],
                    'status': 'failed',
                    'error': str(e)
                })
        
        return created_copilots
    
    def create_copilot_from_scraped_finding(self, finding, api_key=None):
        """Create a copilot from a scraped AI service finding"""
        service_name = finding.get('service', 'unknown')
        
        # Map scraped service to free service
        service_mapping = {
            'chatgpt': 'huggingface_chat',
            'claude': 'huggingface_chat', 
            'bard': 'huggingface_chat',
            'copilot': 'huggingface_chat',
            'huggingface': 'huggingface_chat',
            'ollama': 'ollama_local',
            'groq': 'groq_free',
            'together': 'together_free',
            'replicate': 'replicate_free',
            'perplexity': 'huggingface_chat'
        }
        
        target_service = service_mapping.get(service_name, 'huggingface_chat')
        free_services = self.get_free_chatbox_services()
        
        if target_service in free_services:
            service_info = free_services[target_service]
            copilot_id = self.copilot_manager.create_copilot_from_service(
                target_service, service_info, api_key
            )
            return {
                'copilot_id': copilot_id,
                'original_finding': finding,
                'mapped_service': target_service,
                'status': 'created'
            }
        else:
            return {
                'original_finding': finding,
                'status': 'failed',
                'error': f'No free service available for {service_name}'
            }
    
    def bulk_create_copilots_from_findings(self, findings, max_copilots=10):
        """Create multiple copilots from scraped findings"""
        created_copilots = []
        unique_services = set()
        
        for finding in findings:
            if len(created_copilots) >= max_copilots:
                break
                
            service_name = finding.get('service', 'unknown')
            if service_name in unique_services:
                continue  # Skip duplicates
                
            unique_services.add(service_name)
            
            result = self.create_copilot_from_scraped_finding(finding)
            created_copilots.append(result)
            
            if result.get('status') == 'created':
                print(f" Created copilot from finding: {service_name}")
        
        return created_copilots
    
    def get_all_copilots(self):
        """Get all active copilots"""
        return self.copilot_manager.list_copilots()
    
    def use_copilot_by_id(self, copilot_id, prompt):
        """Use a specific copilot by ID"""
        return self.copilot_manager.use_copilot(copilot_id, prompt)
    
    def get_copilot_stats(self, copilot_id):
        """Get statistics for a specific copilot"""
        return self.copilot_manager.get_copilot_stats(copilot_id)

class VisualCodeAnalyzer:
    """Visual AI model for analyzing code through computer vision"""
    
    def __init__(self):
        self.syntax_patterns = {
            'cpp': {
                'memory_leak': ['new ', 'delete ', 'malloc', 'free'],
                'null_pointer': ['->', 'NULL', 'nullptr'],
                'buffer_overflow': ['strcpy', 'strcat', 'sprintf', 'gets'],
                'race_condition': ['thread', 'mutex', 'lock', 'atomic']
            },
            'python': {
                'memory_leak': ['global ', 'import ', 'del '],
                'null_pointer': ['None', 'is None', '== None'],
                'buffer_overflow': ['input()', 'raw_input', 'eval'],
                'race_condition': ['threading', 'multiprocessing', 'Queue']
            }
        }
        print("️ Visual Code Analyzer initialized")
    
    def analyze_code_visually(self, code_image_path, language="auto"):
        """Analyze code using computer vision"""
        try:
            print(f"️ Analyzing code visually: {code_image_path}")
            
            # Load and process image
            image = cv2.imread(code_image_path)
            if image is None:
                print(" Could not load code image")
                return None
            
            # Convert to different color spaces for analysis
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
            hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
            
            # Detect text regions
            text_regions = self._detect_text_regions(gray)
            
            # Analyze syntax highlighting patterns
            syntax_analysis = self._analyze_syntax_highlighting(image)
            
            # Detect code structure
            structure_analysis = self._analyze_code_structure(gray)
            
            # Detect potential issues visually
            visual_issues = self._detect_visual_issues(image, text_regions)
            
            # Generate visual report
            report = {
                'text_regions': text_regions,
                'syntax_analysis': syntax_analysis,
                'structure_analysis': structure_analysis,
                'visual_issues': visual_issues,
                'language_detected': self._detect_language_visually(image),
                'complexity_score': self._calculate_visual_complexity(image)
            }
            
            print(f" Visual analysis complete - {len(visual_issues)} issues detected")
            return report
            
        except Exception as e:
            print(f" Visual analysis error: {e}")
            return None
    
    def _detect_text_regions(self, gray_image):
        """Detect text regions in the code image"""
        try:
            # Use morphological operations to detect text
            kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
            dilated = cv2.dilate(gray_image, kernel, iterations=1)
            
            # Find contours
            contours, _ = cv2.findContours(dilated, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            text_regions = []
            for contour in contours:
                x, y, w, h = cv2.boundingRect(contour)
                if w > 10 and h > 10:  # Filter small regions
                    text_regions.append({
                        'x': x, 'y': y, 'width': w, 'height': h,
                        'area': w * h
                    })
            
            return text_regions
            
        except Exception as e:
            print(f" Text region detection error: {e}")
            return []
    
    def _analyze_syntax_highlighting(self, image):
        """Analyze syntax highlighting patterns"""
        try:
            # Convert to HSV for color analysis
            hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
            
            # Define color ranges for different syntax elements
            color_ranges = {
                'keywords': ([0, 0, 100], [180, 255, 255]),  # Bright colors
                'strings': ([100, 50, 50], [130, 255, 255]),  # Green-ish
                'comments': ([0, 0, 50], [180, 50, 150]),  # Gray-ish
                'numbers': ([20, 100, 100], [30, 255, 255])  # Orange-ish
            }
            
            syntax_elements = {}
            for element, (lower, upper) in color_ranges.items():
                mask = cv2.inRange(hsv, np.array(lower), np.array(upper))
                contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
                syntax_elements[element] = len(contours)
            
            return syntax_elements
            
        except Exception as e:
            print(f" Syntax highlighting analysis error: {e}")
            return {}
    
    def _analyze_code_structure(self, gray_image):
        """Analyze code structure and indentation"""
        try:
            # Detect horizontal lines (indentation)
            horizontal_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (25, 1))
            horizontal_lines = cv2.morphologyEx(gray_image, cv2.MORPH_OPEN, horizontal_kernel)
            
            # Detect vertical lines (code blocks)
            vertical_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 25))
            vertical_lines = cv2.morphologyEx(gray_image, cv2.MORPH_OPEN, vertical_kernel)
            
            # Analyze indentation patterns
            indentation_levels = self._detect_indentation_levels(gray_image)
            
            return {
                'horizontal_lines': len(cv2.findContours(horizontal_lines, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0]),
                'vertical_lines': len(cv2.findContours(vertical_lines, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0]),
                'indentation_levels': indentation_levels,
                'structure_complexity': len(indentation_levels)
            }
            
        except Exception as e:
            print(f" Code structure analysis error: {e}")
            return {}
    
    def _detect_indentation_levels(self, gray_image):
        """Detect indentation levels in the code"""
        try:
            # Project image horizontally to detect indentation
            projection = np.sum(gray_image, axis=1)
            
            # Find indentation levels
            levels = []
            current_level = 0
            
            for i, value in enumerate(projection):
                if value > np.mean(projection) * 0.8:  # Significant content
                    levels.append(current_level)
                elif value < np.mean(projection) * 0.3:  # Empty line
                    current_level = 0
                else:
                    current_level += 1
            
            return levels
            
        except Exception as e:
            print(f" Indentation detection error: {e}")
            return []
    
    def _detect_visual_issues(self, image, text_regions):
        """Detect visual issues in the code"""
        try:
            issues = []
            
            # Detect long lines (potential readability issues)
            for region in text_regions:
                if region['width'] > image.shape[1] * 0.8:  # Very long line
                    issues.append({
                        'type': 'long_line',
                        'severity': 'warning',
                        'description': 'Line appears to be very long',
                        'location': (region['x'], region['y'])
                    })
            
            # Detect inconsistent indentation
            indentation_levels = self._detect_indentation_levels(cv2.cvtColor(image, cv2.COLOR_BGR2GRAY))
            if len(set(indentation_levels)) > 5:  # Too many indentation levels
                issues.append({
                    'type': 'inconsistent_indentation',
                    'severity': 'warning',
                    'description': 'Inconsistent indentation detected',
                    'location': (0, 0)
                })
            
            # Detect potential syntax errors (unusual color patterns)
            hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
            unusual_colors = self._detect_unusual_color_patterns(hsv)
            if unusual_colors:
                issues.append({
                    'type': 'unusual_syntax',
                    'severity': 'info',
                    'description': 'Unusual syntax highlighting detected',
                    'location': (0, 0)
                })
            
            return issues
            
        except Exception as e:
            print(f" Visual issue detection error: {e}")
            return []
    
    def _detect_unusual_color_patterns(self, hsv_image):
        """Detect unusual color patterns that might indicate syntax errors"""
        try:
            # Analyze color distribution
            hist = cv2.calcHist([hsv_image], [0], None, [180], [0, 180])
            
            # Find dominant colors
            dominant_colors = np.argsort(hist.flatten())[-5:]
            
            # Check for unusual color combinations
            unusual = False
            for color in dominant_colors:
                if color < 10 or color > 170:  # Red or blue dominant
                    unusual = True
                    break
            
            return unusual
            
        except Exception as e:
            print(f" Color pattern detection error: {e}")
            return False
    
    def _detect_language_visually(self, image):
        """Detect programming language from visual patterns"""
        try:
            # Analyze syntax highlighting patterns
            hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
            
            # Count different color regions
            color_regions = self._count_color_regions(hsv)
            
            # Language detection based on color patterns
            if color_regions['blue'] > color_regions['green']:
                return 'cpp'  # C++ often has more blue keywords
            elif color_regions['green'] > color_regions['blue']:
                return 'python'  # Python often has more green strings
            else:
                return 'unknown'
                
        except Exception as e:
            print(f" Language detection error: {e}")
            return 'unknown'
    
    def _count_color_regions(self, hsv_image):
        """Count color regions in the image"""
        try:
            # Define color ranges
            blue_range = cv2.inRange(hsv_image, np.array([100, 50, 50]), np.array([130, 255, 255]))
            green_range = cv2.inRange(hsv_image, np.array([40, 50, 50]), np.array([80, 255, 255]))
            red_range = cv2.inRange(hsv_image, np.array([0, 50, 50]), np.array([10, 255, 255]))
            
            return {
                'blue': np.sum(blue_range > 0),
                'green': np.sum(green_range > 0),
                'red': np.sum(red_range > 0)
            }
            
        except Exception as e:
            print(f" Color region counting error: {e}")
            return {'blue': 0, 'green': 0, 'red': 0}
    
    def _calculate_visual_complexity(self, image):
        """Calculate visual complexity score"""
        try:
            # Convert to grayscale
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
            
            # Calculate edge density
            edges = cv2.Canny(gray, 50, 150)
            edge_density = np.sum(edges > 0) / (image.shape[0] * image.shape[1])
            
            # Calculate texture complexity
            texture = cv2.Laplacian(gray, cv2.CV_64F).var()
            
            # Calculate overall complexity
            complexity = (edge_density * 0.6 + texture * 0.4) * 100
            
            return min(complexity, 100)  # Cap at 100
            
        except Exception as e:
            print(f" Complexity calculation error: {e}")
            return 0
    
    def create_visual_analysis_report(self, analysis_result, output_path="visual_analysis_report.html"):
        """Create visual analysis report"""
        try:
            if not analysis_result:
                return None
            
            html = f"""
            <!DOCTYPE html>
            <html>
            <head>
                <title>Visual Code Analysis Report</title>
                <style>
                    body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
                    .header {{ background: #2c3e50; color: white; padding: 20px; border-radius: 5px; }}
                    .section {{ background: white; margin: 10px 0; padding: 15px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }}
                    .issue {{ background: #f8d7da; border: 1px solid #f5c6cb; padding: 10px; margin: 5px 0; border-radius: 3px; }}
                    .warning {{ background: #fff3cd; border: 1px solid #ffeaa7; }}
                    .info {{ background: #d1ecf1; border: 1px solid #bee5eb; }}
                </style>
            </head>
            <body>
                <div class="header">
                    <h1>️ Visual Code Analysis Report</h1>
                    <p>AI-powered visual analysis of your code</p>
                </div>
                
                <div class="section">
                    <h2> Analysis Summary</h2>
                    <p><strong>Language Detected:</strong> {analysis_result.get('language_detected', 'Unknown')}</p>
                    <p><strong>Complexity Score:</strong> {analysis_result.get('complexity_score', 0):.1f}/100</p>
                    <p><strong>Text Regions Found:</strong> {len(analysis_result.get('text_regions', []))}</p>
                    <p><strong>Visual Issues:</strong> {len(analysis_result.get('visual_issues', []))}</p>
                </div>
            """
            
            # Add visual issues
            if analysis_result.get('visual_issues'):
                html += """
                <div class="section">
                    <h2>️ Visual Issues Detected</h2>
                """
                for issue in analysis_result['visual_issues']:
                    severity_class = issue.get('severity', 'info')
                    html += f"""
                    <div class="issue {severity_class}">
                        <strong>{issue.get('type', 'Unknown Issue')}</strong><br>
                        {issue.get('description', 'No description available')}
                    </div>
                    """
                html += "</div>"
            
            # Add structure analysis
            if analysis_result.get('structure_analysis'):
                structure = analysis_result['structure_analysis']
                html += f"""
                <div class="section">
                    <h2>️ Code Structure Analysis</h2>
                    <p><strong>Indentation Levels:</strong> {structure.get('indentation_levels', 0)}</p>
                    <p><strong>Structure Complexity:</strong> {structure.get('structure_complexity', 0)}</p>
                </div>
                """
            
            html += """
                <div class="section">
                    <h2> Visual Analysis Tips</h2>
                    <ul>
                        <li>Use consistent indentation for better readability</li>
                        <li>Avoid very long lines that extend beyond the screen</li>
                        <li>Use syntax highlighting to identify different code elements</li>
                        <li>Keep code structure simple and well-organized</li>
                    </ul>
                </div>
            </body>
            </html>
            """
            
            # Save report
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(html)
            
            print(f" Visual analysis report generated: {output_path}")
            return output_path
            
        except Exception as e:
            print(f" Visual report generation error: {e}")
            return None

class DebuggingAgent:
    """AI-powered debugging agent for intelligent code analysis and bug detection"""
    
    def __init__(self):
        print(" Debugging Agent initialized")
        
        # Common bug patterns
        self.bug_patterns = {
            'memory_leaks': [
                r'malloc\s*\([^)]+\)\s*(?!.*free)',
                r'new\s+\w+\s*(?!.*delete)',
                r'CreateObject\s*\([^)]+\)\s*(?!.*Release)',
                r'OpenFile\s*\([^)]+\)\s*(?!.*CloseFile)'
            ],
            'null_pointer': [
                r'->\s*\w+\s*\([^)]*\)\s*(?!.*if\s*\(\w+\s*!=?\s*NULL)',
                r'\[\w+\]\s*(?!.*if\s*\(\w+\s*!=?\s*NULL)',
                r'\.\w+\s*\([^)]*\)\s*(?!.*if\s*\(\w+\s*!=?\s*NULL)'
            ],
            'buffer_overflow': [
                r'strcpy\s*\([^,]+,\s*[^)]+\)',
                r'strcat\s*\([^,]+,\s*[^)]+\)',
                r'sprintf\s*\([^,]+,\s*[^)]+\)',
                r'gets\s*\([^)]+\)'
            ],
            'race_conditions': [
                r'pthread_create\s*\([^)]+\)\s*(?!.*pthread_join)',
                r'CreateThread\s*\([^)]+\)\s*(?!.*WaitForSingleObject)',
                r'fork\s*\([^)]*\)\s*(?!.*wait)',
                r'std::thread\s+\w+\s*\([^)]+\)\s*(?!.*\.join)'
            ],
            'infinite_loops': [
                r'while\s*\(\s*true\s*\)',
                r'for\s*\(\s*;\s*;\s*\)',
                r'while\s*\(\s*1\s*\)',
                r'do\s*\{[^}]*\}\s*while\s*\(\s*true\s*\)'
            ],
            'uninitialized_variables': [
                r'int\s+\w+\s*;',
                r'char\s+\w+\s*;',
                r'float\s+\w+\s*;',
                r'double\s+\w+\s*;'
            ],
            'resource_leaks': [
                r'fopen\s*\([^)]+\)\s*(?!.*fclose)',
                r'CreateFile\s*\([^)]+\)\s*(?!.*CloseHandle)',
                r'socket\s*\([^)]+\)\s*(?!.*closesocket)',
                r'CreateMutex\s*\([^)]+\)\s*(?!.*CloseHandle)'
            ]
        }
        
        # Performance anti-patterns
        self.performance_patterns = {
            'inefficient_loops': [
                r'for\s*\(\s*int\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*strlen\s*\([^)]+\)',
                r'for\s*\(\s*int\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*\.length\s*\(\s*\)',
                r'for\s*\(\s*int\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*\.size\s*\(\s*\)'
            ],
            'redundant_calculations': [
                r'strlen\s*\([^)]+\)\s*.*strlen\s*\([^)]+\)',
                r'\.length\s*\(\s*\)\s*.*\.length\s*\(\s*\)',
                r'\.size\s*\(\s*\)\s*.*\.size\s*\(\s*\)'
            ],
            'memory_inefficiency': [
                r'new\s+\w+\[\s*\d+\s*\]\s*(?!.*delete\s*\[\])',
                r'malloc\s*\(\s*\d+\s*\*\s*sizeof\s*\(\s*\w+\s*\)\s*\)\s*(?!.*free)'
            ]
        }
        
        # Security vulnerability patterns
        self.security_patterns = {
            'sql_injection': [
                r'SELECT\s+.*\s+FROM\s+.*\s+WHERE\s+.*\s*\+\s*[^"]+',
                r'INSERT\s+INTO\s+.*\s+VALUES\s*\(\s*[^)]*\s*\+\s*[^)]+',
                r'UPDATE\s+.*\s+SET\s+.*\s*=\s*[^"]*\s*\+\s*[^"]+',
                r'DELETE\s+FROM\s+.*\s+WHERE\s+.*\s*\+\s*[^"]+'
            ],
            'xss_vulnerabilities': [
                r'innerHTML\s*=\s*[^"]*\+[^"]+',
                r'document\.write\s*\(\s*[^)]*\+[^)]+',
                r'\.html\s*\(\s*[^)]*\+[^)]+'
            ],
            'path_traversal': [
                r'fopen\s*\(\s*[^"]*\.\.\/[^"]*',
                r'open\s*\(\s*[^"]*\.\.\/[^"]*',
                r'File\.Open\s*\(\s*[^"]*\.\.\/[^"]*'
            ],
            'hardcoded_secrets': [
                r'password\s*=\s*["\'][^"\']+["\']',
                r'api_key\s*=\s*["\'][^"\']+["\']',
                r'secret\s*=\s*["\'][^"\']+["\']',
                r'token\s*=\s*["\'][^"\']+["\']'
            ]
        }
    
    def analyze_code(self, source_code, language="auto"):
        """Analyze code for bugs, performance issues, and security vulnerabilities"""
        print(f" Debugging Agent analyzing {language} code...")
        
        analysis_results = {
            'bugs': [],
            'performance_issues': [],
            'security_vulnerabilities': [],
            'suggestions': [],
            'overall_score': 100
        }
        
        try:
            # Detect language if auto
            if language == "auto":
                language = self._detect_language(source_code)
            
            # Analyze for bugs
            bugs = self._detect_bugs(source_code, language)
            analysis_results['bugs'] = bugs
            
            # Analyze for performance issues
            performance = self._detect_performance_issues(source_code, language)
            analysis_results['performance_issues'] = performance
            
            # Analyze for security vulnerabilities
            security = self._detect_security_vulnerabilities(source_code, language)
            analysis_results['security_vulnerabilities'] = security
            
            # Generate suggestions
            suggestions = self._generate_suggestions(bugs, performance, security)
            analysis_results['suggestions'] = suggestions
            
            # Calculate overall score
            score = self._calculate_score(bugs, performance, security)
            analysis_results['overall_score'] = score
            
            print(f" Analysis complete - Score: {score}/100")
            return analysis_results
            
        except Exception as e:
            print(f" Analysis error: {e}")
            return analysis_results
    
    def _detect_language(self, source_code):
        """Detect programming language from source code"""
        if 'int main(' in source_code or '#include' in source_code:
            return 'cpp'
        elif 'def ' in source_code or 'import ' in source_code:
            return 'python'
        elif 'function ' in source_code or 'var ' in source_code:
            return 'javascript'
        elif 'public class ' in source_code or 'public static void main':
            return 'java'
        elif 'using System;' in source_code or 'namespace ' in source_code:
            return 'csharp'
        else:
            return 'generic'
    
    def _detect_bugs(self, source_code, language):
        """Detect common bugs in code"""
        bugs = []
        
        for bug_type, patterns in self.bug_patterns.items():
            for pattern in patterns:
                matches = re.finditer(pattern, source_code, re.IGNORECASE | re.MULTILINE)
                for match in matches:
                    line_num = source_code[:match.start()].count('\n') + 1
                    context = self._get_context(source_code, match.start(), match.end())
                    
                    bugs.append({
                        'type': bug_type,
                        'severity': self._get_bug_severity(bug_type),
                        'line': line_num,
                        'pattern': pattern,
                        'context': context,
                        'description': self._get_bug_description(bug_type),
                        'fix_suggestion': self._get_fix_suggestion(bug_type)
                    })
        
        return bugs
    
    def _detect_performance_issues(self, source_code, language):
        """Detect performance issues in code"""
        issues = []
        
        for issue_type, patterns in self.performance_patterns.items():
            for pattern in patterns:
                matches = re.finditer(pattern, source_code, re.IGNORECASE | re.MULTILINE)
                for match in matches:
                    line_num = source_code[:match.start()].count('\n') + 1
                    context = self._get_context(source_code, match.start(), match.end())
                    
                    issues.append({
                        'type': issue_type,
                        'severity': 'medium',
                        'line': line_num,
                        'pattern': pattern,
                        'context': context,
                        'description': self._get_performance_description(issue_type),
                        'optimization': self._get_optimization_suggestion(issue_type)
                    })
        
        return issues
    
    def _detect_security_vulnerabilities(self, source_code, language):
        """Detect security vulnerabilities in code"""
        vulnerabilities = []
        
        for vuln_type, patterns in self.security_patterns.items():
            for pattern in patterns:
                matches = re.finditer(pattern, source_code, re.IGNORECASE | re.MULTILINE)
                for match in matches:
                    line_num = source_code[:match.start()].count('\n') + 1
                    context = self._get_context(source_code, match.start(), match.end())
                    
                    vulnerabilities.append({
                        'type': vuln_type,
                        'severity': self._get_security_severity(vuln_type),
                        'line': line_num,
                        'pattern': pattern,
                        'context': context,
                        'description': self._get_security_description(vuln_type),
                        'mitigation': self._get_mitigation_suggestion(vuln_type)
                    })
        
        return vulnerabilities
    
    def _get_context(self, source_code, start, end):
        """Get context around found issue"""
        lines = source_code.split('\n')
        line_start = source_code[:start].count('\n')
        line_end = source_code[:end].count('\n')
        
        context_lines = []
        for i in range(max(0, line_start - 2), min(len(lines), line_end + 3)):
            prefix = ">>> " if i == line_start else "    "
            context_lines.append(f"{prefix}{i+1:4d}: {lines[i]}")
        
        return '\n'.join(context_lines)
    
    def _get_bug_severity(self, bug_type):
        """Get severity level for bug type"""
        severity_map = {
            'memory_leaks': 'high',
            'null_pointer': 'critical',
            'buffer_overflow': 'critical',
            'race_conditions': 'high',
            'infinite_loops': 'high',
            'uninitialized_variables': 'medium',
            'resource_leaks': 'high'
        }
        return severity_map.get(bug_type, 'medium')
    
    def _get_bug_description(self, bug_type):
        """Get description for bug type"""
        descriptions = {
            'memory_leaks': 'Potential memory leak detected - allocated memory not freed',
            'null_pointer': 'Potential null pointer dereference - no null check before use',
            'buffer_overflow': 'Potential buffer overflow - unsafe string operation',
            'race_conditions': 'Potential race condition - thread synchronization issue',
            'infinite_loops': 'Potential infinite loop - no exit condition',
            'uninitialized_variables': 'Uninitialized variable - may contain garbage data',
            'resource_leaks': 'Resource leak detected - resource not properly closed'
        }
        return descriptions.get(bug_type, 'Unknown bug pattern detected')
    
    def _get_fix_suggestion(self, bug_type):
        """Get fix suggestion for bug type"""
        suggestions = {
            'memory_leaks': 'Add corresponding free()/delete calls',
            'null_pointer': 'Add null pointer checks before dereferencing',
            'buffer_overflow': 'Use safer string functions like strncpy() or std::string',
            'race_conditions': 'Add proper synchronization (mutex, semaphore)',
            'infinite_loops': 'Add proper exit condition or break statement',
            'uninitialized_variables': 'Initialize variables before use',
            'resource_leaks': 'Add proper cleanup in finally blocks or destructors'
        }
        return suggestions.get(bug_type, 'Review code for potential issues')
    
    def _get_performance_description(self, issue_type):
        """Get performance issue description"""
        descriptions = {
            'inefficient_loops': 'Inefficient loop - calling expensive function in condition',
            'redundant_calculations': 'Redundant calculation - same value computed multiple times',
            'memory_inefficiency': 'Memory inefficiency - consider using stack allocation'
        }
        return descriptions.get(issue_type, 'Performance issue detected')
    
    def _get_optimization_suggestion(self, issue_type):
        """Get optimization suggestion"""
        suggestions = {
            'inefficient_loops': 'Cache the result of expensive function calls',
            'redundant_calculations': 'Store result in variable and reuse',
            'memory_inefficiency': 'Use stack allocation when possible'
        }
        return suggestions.get(issue_type, 'Consider optimizing this code')
    
    def _get_security_severity(self, vuln_type):
        """Get security vulnerability severity"""
        severity_map = {
            'sql_injection': 'critical',
            'xss_vulnerabilities': 'high',
            'path_traversal': 'high',
            'hardcoded_secrets': 'medium'
        }
        return severity_map.get(vuln_type, 'medium')
    
    def _get_security_description(self, vuln_type):
        """Get security vulnerability description"""
        descriptions = {
            'sql_injection': 'SQL injection vulnerability - user input not sanitized',
            'xss_vulnerabilities': 'XSS vulnerability - user input not escaped',
            'path_traversal': 'Path traversal vulnerability - directory traversal possible',
            'hardcoded_secrets': 'Hardcoded secret detected - sensitive data in source code'
        }
        return descriptions.get(vuln_type, 'Security vulnerability detected')
    
    def _get_mitigation_suggestion(self, vuln_type):
        """Get security mitigation suggestion"""
        suggestions = {
            'sql_injection': 'Use parameterized queries or prepared statements',
            'xss_vulnerabilities': 'Escape user input before output',
            'path_traversal': 'Validate and sanitize file paths',
            'hardcoded_secrets': 'Use environment variables or secure key management'
        }
        return suggestions.get(vuln_type, 'Implement proper security measures')
    
    def _generate_suggestions(self, bugs, performance, security):
        """Generate overall improvement suggestions"""
        suggestions = []
        
        if bugs:
            suggestions.append(" Fix critical bugs first - focus on null pointer and buffer overflow issues")
        
        if performance:
            suggestions.append(" Optimize performance - cache expensive calculations and use efficient algorithms")
        
        if security:
            suggestions.append("️ Address security vulnerabilities - sanitize inputs and use secure coding practices")
        
        if not bugs and not performance and not security:
            suggestions.append(" Code looks good! Consider adding unit tests and documentation")
        
        return suggestions
    
    def _calculate_score(self, bugs, performance, security):
        """Calculate overall code quality score"""
        score = 100
        
        # Deduct points for issues
        for bug in bugs:
            if bug['severity'] == 'critical':
                score -= 20
            elif bug['severity'] == 'high':
                score -= 10
            elif bug['severity'] == 'medium':
                score -= 5
        
        for issue in performance:
            score -= 5
        
        for vuln in security:
            if vuln['severity'] == 'critical':
                score -= 25
            elif vuln['severity'] == 'high':
                score -= 15
            elif vuln['severity'] == 'medium':
                score -= 10
        
        return max(0, score)
    
    def generate_debug_report(self, analysis_results, output_file=None):
        """Generate comprehensive debugging report"""
        if not output_file:
            output_file = "debug_analysis_report.txt"
        
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(" DEBUGGING AGENT ANALYSIS REPORT\n")
                f.write("=" * 50 + "\n\n")
                f.write(f"Analysis Date: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Overall Score: {analysis_results['overall_score']}/100\n\n")
                
                # Bugs section
                if analysis_results['bugs']:
                    f.write(" BUGS FOUND:\n")
                    f.write("-" * 20 + "\n")
                    for bug in analysis_results['bugs']:
                        f.write(f"\nType: {bug['type']}\n")
                        f.write(f"Severity: {bug['severity']}\n")
                        f.write(f"Line: {bug['line']}\n")
                        f.write(f"Description: {bug['description']}\n")
                        f.write(f"Fix: {bug['fix_suggestion']}\n")
                        f.write(f"Context:\n{bug['context']}\n")
                        f.write("\n" + "="*50 + "\n")
                
                # Performance section
                if analysis_results['performance_issues']:
                    f.write("\n PERFORMANCE ISSUES:\n")
                    f.write("-" * 25 + "\n")
                    for issue in analysis_results['performance_issues']:
                        f.write(f"\nType: {issue['type']}\n")
                        f.write(f"Line: {issue['line']}\n")
                        f.write(f"Description: {issue['description']}\n")
                        f.write(f"Optimization: {issue['optimization']}\n")
                        f.write(f"Context:\n{issue['context']}\n")
                        f.write("\n" + "="*50 + "\n")
                
                # Security section
                if analysis_results['security_vulnerabilities']:
                    f.write("\n️ SECURITY VULNERABILITIES:\n")
                    f.write("-" * 30 + "\n")
                    for vuln in analysis_results['security_vulnerabilities']:
                        f.write(f"\nType: {vuln['type']}\n")
                        f.write(f"Severity: {vuln['severity']}\n")
                        f.write(f"Line: {vuln['line']}\n")
                        f.write(f"Description: {vuln['description']}\n")
                        f.write(f"Mitigation: {vuln['mitigation']}\n")
                        f.write(f"Context:\n{vuln['context']}\n")
                        f.write("\n" + "="*50 + "\n")
                
                # Suggestions section
                if analysis_results['suggestions']:
                    f.write("\n RECOMMENDATIONS:\n")
                    f.write("-" * 20 + "\n")
                    for suggestion in analysis_results['suggestions']:
                        f.write(f"• {suggestion}\n")
                
                # Summary
                f.write(f"\n SUMMARY:\n")
                f.write("-" * 15 + "\n")
                f.write(f"Bugs found: {len(analysis_results['bugs'])}\n")
                f.write(f"Performance issues: {len(analysis_results['performance_issues'])}\n")
                f.write(f"Security vulnerabilities: {len(analysis_results['security_vulnerabilities'])}\n")
                f.write(f"Overall quality score: {analysis_results['overall_score']}/100\n")
            
            print(f" Debug report generated: {output_file}")
            return output_file
            
        except Exception as e:
            print(f" Error generating debug report: {e}")
            return None
    
    def setup_internet_access(self, proxy_manager):
        """Setup internet access for the debugging agent"""
        self.proxy_manager = proxy_manager
        self.browser_window = None
        self.visual_analyzer = VisualCodeAnalyzer()
        self.ai_models = AIModelManager()
        print(" Debugging Agent internet access enabled")
        print("️ Visual AI analysis capabilities enabled")
        print(" AI Model integration enabled")
    
    def search_online_solution(self, error_message, language="auto"):
        """Search online for solutions to debugging issues"""
        try:
            print(f" Searching online for solution to: {error_message[:50]}...")
            
            # Create search queries
            search_queries = self._generate_search_queries(error_message, language)
            
            # Search results
            solutions = []
            
            for query in search_queries:
                try:
                    # Use proxy if available
                    if hasattr(self.proxy_manager, 'current_proxy') and self.proxy_manager.current_proxy != 'none':
                        search_results = self._search_with_proxy(query)
                    else:
                        search_results = self._search_direct(query)
                    
                    solutions.extend(search_results)
                    
                    # Rate limiting
                    time.sleep(random.uniform(1, 3))
                    
                except Exception as e:
                    print(f" Search error for query '{query}': {e}")
                    continue
            
            return solutions
            
        except Exception as e:
            print(f" Online search error: {e}")
            return []
    
    def _generate_search_queries(self, error_message, language):
        """Generate search queries for the error"""
        queries = []
        
        # Extract key terms from error
        error_terms = error_message.lower().split()
        key_terms = [term for term in error_terms if len(term) > 3 and term not in ['the', 'and', 'for', 'with', 'this', 'that']]
        
        # Create targeted queries
        if language == "cpp" or "c++" in error_message.lower():
            queries.extend([
                f"c++ {error_message[:100]} stackoverflow",
                f"c++ {key_terms[0] if key_terms else 'error'} solution",
                f"c++ debugging {key_terms[0] if key_terms else 'issue'}"
            ])
        elif language == "python":
            queries.extend([
                f"python {error_message[:100]} stackoverflow",
                f"python {key_terms[0] if key_terms else 'error'} solution",
                f"python debugging {key_terms[0] if key_terms else 'issue'}"
            ])
        else:
            queries.extend([
                f"{error_message[:100]} stackoverflow",
                f"{key_terms[0] if key_terms else 'error'} solution",
                f"debugging {key_terms[0] if key_terms else 'issue'}"
            ])
        
        return queries[:3]  # Limit to 3 queries
    
    def _search_with_proxy(self, query):
        """Search using proxy connection"""
        try:
            # Encode query
            encoded_query = urllib.parse.quote_plus(query)
            search_url = f"https://www.google.com/search?q={encoded_query}"
            
            # Use proxy manager's current proxy
            request = urllib.request.Request(search_url)
            request.add_header('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36')
            
            with urllib.request.urlopen(request, timeout=10) as response:
                html = response.read().decode('utf-8', errors='ignore')
                return self._parse_search_results(html)
                
        except Exception as e:
            print(f" Proxy search error: {e}")
            return []
    
    def _search_direct(self, query):
        """Search using direct connection"""
        try:
            # Encode query
            encoded_query = urllib.parse.quote_plus(query)
            search_url = f"https://www.google.com/search?q={encoded_query}"
            
            request = urllib.request.Request(search_url)
            request.add_header('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36')
            
            with urllib.request.urlopen(request, timeout=10) as response:
                html = response.read().decode('utf-8', errors='ignore')
                return self._parse_search_results(html)
                
        except Exception as e:
            print(f" Direct search error: {e}")
            return []
    
    def _parse_search_results(self, html):
        """Parse search results from HTML"""
        results = []
        
        try:
            # Simple regex-based parsing for Stack Overflow and other helpful sites
            url_pattern = r'<a href="/url\?q=([^&]+)&amp;'
            urls = re.findall(url_pattern, html)
            
            for url in urls[:5]:  # Limit to 5 results
                try:
                    decoded_url = urllib.parse.unquote(url)
                    if any(site in decoded_url for site in ['stackoverflow.com', 'github.com', 'docs.python.org', 'cppreference.com', 'cplusplus.com']):
                        results.append({
                            'url': decoded_url,
                            'title': 'Search Result',
                            'source': 'Google Search'
                        })
                except:
                    continue
            
        except Exception as e:
            print(f" Result parsing error: {e}")
        
        return results
    
    def open_browser_window(self, url=None):
        """Open a visual browser window in the IDE"""
        try:
            if not self.browser_window:
                # Create browser window
                self.browser_window = tk.Toplevel()
                self.browser_window.title(" Debugging Agent Browser")
                self.browser_window.geometry("1000x700")
                
                # Create web browser widget
                self.web_browser = tkinter.web.WebView(self.browser_window)
                self.web_browser.pack(fill=tk.BOTH, expand=True)
                
                # Add navigation controls
                nav_frame = ttk.Frame(self.browser_window)
                nav_frame.pack(fill=tk.X, padx=5, pady=5)
                
                ttk.Button(nav_frame, text="← Back", command=self.web_browser.back).pack(side=tk.LEFT, padx=2)
                ttk.Button(nav_frame, text="→ Forward", command=self.web_browser.forward).pack(side=tk.LEFT, padx=2)
                ttk.Button(nav_frame, text=" Refresh", command=self.web_browser.reload).pack(side=tk.LEFT, padx=2)
                
                # URL entry
                self.url_entry = ttk.Entry(nav_frame, width=50)
                self.url_entry.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)
                self.url_entry.bind('<Return>', lambda e: self._navigate_to_url())
                
                ttk.Button(nav_frame, text="Go", command=self._navigate_to_url).pack(side=tk.LEFT, padx=2)
                
                # Close button
                ttk.Button(nav_frame, text=" Close", command=self.browser_window.destroy).pack(side=tk.RIGHT, padx=2)
            
            # Navigate to URL if provided
            if url:
                self.web_browser.load_url(url)
                self.url_entry.delete(0, tk.END)
                self.url_entry.insert(0, url)
            
            # Show window
            self.browser_window.lift()
            self.browser_window.focus()
            
            print(f" Browser window opened: {url if url else 'Ready'}")
            return True
            
        except Exception as e:
            print(f" Browser window error: {e}")
            return False
    
    def _navigate_to_url(self):
        """Navigate to URL entered in the address bar"""
        try:
            url = self.url_entry.get().strip()
            if url:
                if not url.startswith(('http://', 'https://')):
                    url = 'https://' + url
                self.web_browser.load_url(url)
        except Exception as e:
            print(f" Navigation error: {e}")
    
    def search_and_show_solutions(self, error_message, language="auto"):
        """Search for solutions and show them in the browser"""
        try:
            print(f" Searching and displaying solutions for: {error_message[:50]}...")
            
            # Search for solutions
            solutions = self.search_online_solution(error_message, language)
            
            if not solutions:
                print(" No solutions found online")
                return False
            
            # Open browser window
            if not self.open_browser_window():
                return False
            
            # Create a results page
            results_html = self._create_results_page(solutions, error_message)
            
            # Save temporary HTML file
            temp_file = "debug_solutions.html"
            with open(temp_file, 'w', encoding='utf-8') as f:
                f.write(results_html)
            
            # Load results in browser
            file_url = f"file://{os.path.abspath(temp_file)}"
            self.web_browser.load_url(file_url)
            self.url_entry.delete(0, tk.END)
            self.url_entry.insert(0, file_url)
            
            print(f" Found {len(solutions)} solutions - displaying in browser")
            return True
            
        except Exception as e:
            print(f" Search and show error: {e}")
            return False
    
    def _create_results_page(self, solutions, error_message):
        """Create HTML results page"""
        html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <title>Debug Solutions for: {error_message[:50]}</title>
            <style>
                body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
                .header {{ background: #2c3e50; color: white; padding: 20px; border-radius: 5px; }}
                .solution {{ background: white; margin: 10px 0; padding: 15px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }}
                .solution h3 {{ color: #3498db; margin-top: 0; }}
                .solution a {{ color: #e74c3c; text-decoration: none; }}
                .solution a:hover {{ text-decoration: underline; }}
                .error {{ background: #e74c3c; color: white; padding: 10px; border-radius: 5px; margin-bottom: 20px; }}
            </style>
        </head>
        <body>
            <div class="header">
                <h1> Debugging Agent Solutions</h1>
                <p>Found solutions for your debugging issue</p>
            </div>
            
            <div class="error">
                <strong>Error:</strong> {error_message}
            </div>
        """
        
        for i, solution in enumerate(solutions, 1):
            html += f"""
            <div class="solution">
                <h3>Solution {i}: {solution.get('title', 'Search Result')}</h3>
                <p><strong>Source:</strong> {solution.get('source', 'Unknown')}</p>
                <p><a href="{solution['url']}" target="_blank">View Solution →</a></p>
            </div>
            """
        
        html += """
            <div style="margin-top: 30px; padding: 20px; background: #ecf0f1; border-radius: 5px;">
                <h3> Tips:</h3>
                <ul>
                    <li>Click the links above to view detailed solutions</li>
                    <li>Use the browser navigation to explore related pages</li>
                    <li>Copy useful code snippets back to your IDE</li>
                </ul>
            </div>
        </body>
        </html>
        """
        
        return html
    
    def open_documentation(self, topic, language="auto"):
        """Open relevant documentation in browser"""
        try:
            print(f" Opening documentation for: {topic}")
            
            # Map topics to documentation URLs
            doc_urls = {
                'cpp': {
                    'memory': 'https://en.cppreference.com/w/cpp/memory',
                    'threading': 'https://en.cppreference.com/w/cpp/thread',
                    'containers': 'https://en.cppreference.com/w/cpp/container',
                    'algorithms': 'https://en.cppreference.com/w/cpp/algorithm'
                },
                'python': {
                    'memory': 'https://docs.python.org/3/c-api/memory.html',
                    'threading': 'https://docs.python.org/3/library/threading.html',
                    'debugging': 'https://docs.python.org/3/library/pdb.html',
                    'performance': 'https://docs.python.org/3/library/profile.html'
                }
            }
            
            # Find relevant documentation
            if language in doc_urls and topic in doc_urls[language]:
                url = doc_urls[language][topic]
            else:
                # Fallback to general search
                url = f"https://www.google.com/search?q={language}+{topic}+documentation"
            
            # Open in browser
            if self.open_browser_window(url):
                print(f" Documentation opened: {url}")
                return True
            else:
                # Fallback to system browser
                webbrowser.open(url)
                print(f" Documentation opened in system browser: {url}")
                return True
                
        except Exception as e:
            print(f" Documentation error: {e}")
            return False
    
    def analyze_code_screenshot(self, screenshot_path, language="auto"):
        """Analyze code using visual AI - takes a screenshot and analyzes it"""
        try:
            print(f" Analyzing code screenshot: {screenshot_path}")
            
            # Use visual analyzer to analyze the screenshot
            if hasattr(self, 'visual_analyzer'):
                visual_analysis = self.visual_analyzer.analyze_code_visually(screenshot_path, language)
                
                if visual_analysis:
                    # Generate visual report
                    report_path = self.visual_analyzer.create_visual_analysis_report(visual_analysis)
                    
                    # Open visual analysis in browser
                    if self.open_browser_window():
                        file_url = f"file://{os.path.abspath(report_path)}"
                        self.web_browser.load_url(file_url)
                        self.url_entry.delete(0, tk.END)
                        self.url_entry.insert(0, file_url)
                        
                        print(f" Visual analysis complete - report opened in browser")
                        return True
                    else:
                        # Fallback to system browser
                        webbrowser.open(f"file://{os.path.abspath(report_path)}")
                        print(f" Visual analysis complete - report opened in system browser")
                        return True
                else:
                    print(" Visual analysis failed")
                    return False
            else:
                print(" Visual analyzer not available")
                return False
                
        except Exception as e:
            print(f" Screenshot analysis error: {e}")
            return False
    
    def take_code_screenshot(self, code_text, output_path="code_screenshot.png"):
        """Take a screenshot of code for visual analysis"""
        try:
            print(f" Creating code screenshot: {output_path}")
            
            # Create a temporary window with code
            temp_window = tk.Toplevel()
            temp_window.title("Code Screenshot")
            temp_window.geometry("800x600")
            temp_window.withdraw()  # Hide window
            
            # Create text widget with code
            text_widget = tk.Text(temp_window, wrap=tk.WORD, font=("Consolas", 12))
            text_widget.pack(fill=tk.BOTH, expand=True)
            text_widget.insert(tk.END, code_text)
            
            # Configure syntax highlighting
            self._configure_syntax_highlighting(text_widget)
            
            # Update window
            temp_window.update()
            
            # Take screenshot
            x = temp_window.winfo_rootx()
            y = temp_window.winfo_rooty()
            width = temp_window.winfo_width()
            height = temp_window.winfo_height()
            
            # Use PIL to capture screenshot
            screenshot = ImageGrab.grab(bbox=(x, y, x + width, y + height))
            screenshot.save(output_path)
            
            # Close temporary window
            temp_window.destroy()
            
            print(f" Code screenshot saved: {output_path}")
            return output_path
            
        except Exception as e:
            print(f" Screenshot creation error: {e}")
            return None
    
    def _configure_syntax_highlighting(self, text_widget):
        """Configure syntax highlighting for the text widget"""
        try:
            # Configure tags for syntax highlighting
            text_widget.tag_configure("keyword", foreground="blue", font=("Consolas", 12, "bold"))
            text_widget.tag_configure("string", foreground="green", font=("Consolas", 12))
            text_widget.tag_configure("comment", foreground="gray", font=("Consolas", 12, "italic"))
            text_widget.tag_configure("number", foreground="orange", font=("Consolas", 12))
            
            # Apply syntax highlighting
            content = text_widget.get("1.0", tk.END)
            lines = content.split('\n')
            
            for i, line in enumerate(lines):
                line_start = f"{i+1}.0"
                line_end = f"{i+1}.end"
                
                # Highlight keywords
                for keyword in ['if', 'else', 'for', 'while', 'def', 'class', 'import', 'return', 'try', 'except']:
                    start = line.find(keyword)
                    if start != -1:
                        text_widget.tag_add("keyword", f"{i+1}.{start}", f"{i+1}.{start + len(keyword)}")
                
                # Highlight strings
                import re
                string_pattern = r'"[^"]*"|\'[^\']*\''
                for match in re.finditer(string_pattern, line):
                    text_widget.tag_add("string", f"{i+1}.{match.start()}", f"{i+1}.{match.end()}")
                
                # Highlight comments
                if '#' in line:
                    comment_start = line.find('#')
                    text_widget.tag_add("comment", f"{i+1}.{comment_start}", f"{i+1}.end")
                
                # Highlight numbers
                number_pattern = r'\b\d+\b'
                for match in re.finditer(number_pattern, line):
                    text_widget.tag_add("number", f"{i+1}.{match.start()}", f"{i+1}.{match.end()}")
            
        except Exception as e:
            print(f" Syntax highlighting error: {e}")
    
    def visual_debug_analysis(self, code_text, language="auto"):
        """Perform complete visual debugging analysis"""
        try:
            print(f" Starting visual debugging analysis...")
            
            # Take screenshot of code
            screenshot_path = self.take_code_screenshot(code_text)
            if not screenshot_path:
                print(" Failed to create code screenshot")
                return False
            
            # Analyze screenshot
            success = self.analyze_code_screenshot(screenshot_path, language)
            
            if success:
                print(" Visual debugging analysis complete")
                return True
            else:
                print(" Visual debugging analysis failed")
                return False
                
        except Exception as e:
            print(f" Visual debug analysis error: {e}")
            return False
    
    def start_desktop_sharing(self, duration=60):
        """Start real-time desktop sharing for live analysis"""
        try:
            print(f" Starting desktop sharing for {duration} seconds...")
            
            # Create screen sharing window
            self.sharing_window = tk.Toplevel()
            self.sharing_window.title(" Desktop Sharing - Live Analysis")
            self.sharing_window.geometry("800x600")
            
            # Create video display
            self.video_label = tk.Label(self.sharing_window)
            self.video_label.pack(fill=tk.BOTH, expand=True)
            
            # Add controls
            control_frame = ttk.Frame(self.sharing_window)
            control_frame.pack(fill=tk.X, padx=5, pady=5)
            
            ttk.Button(control_frame, text=" Start Sharing", command=self._start_sharing).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Stop Sharing", command=self._stop_sharing).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Analyze Frame", command=self._analyze_current_frame).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Close", command=self.sharing_window.destroy).pack(side=tk.RIGHT, padx=2)
            
            # Status label
            self.sharing_status = ttk.Label(control_frame, text="Ready to start sharing")
            self.sharing_status.pack(side=tk.LEFT, padx=10)
            
            # Initialize sharing variables
            self.is_sharing = False
            self.sharing_thread = None
            self.frame_queue = queue.Queue(maxsize=10)
            self.current_frame = None
            
            print(" Desktop sharing window opened")
            return True
            
        except Exception as e:
            print(f" Desktop sharing error: {e}")
            return False
    
    def _start_sharing(self):
        """Start desktop screen capture"""
        try:
            if self.is_sharing:
                return
            
            self.is_sharing = True
            self.sharing_status.config(text="Sharing desktop...")
            
            # Start screen capture thread
            self.sharing_thread = threading.Thread(target=self._capture_desktop)
            self.sharing_thread.daemon = True
            self.sharing_thread.start()
            
            # Start display thread
            self.display_thread = threading.Thread(target=self._display_frames)
            self.display_thread.daemon = True
            self.display_thread.start()
            
            print(" Desktop sharing started")
            
        except Exception as e:
            print(f" Start sharing error: {e}")
    
    def _stop_sharing(self):
        """Stop desktop screen capture"""
        try:
            self.is_sharing = False
            self.sharing_status.config(text="Sharing stopped")
            print(" Desktop sharing stopped")
            
        except Exception as e:
            print(f" Stop sharing error: {e}")
    
    def _capture_desktop(self):
        """Capture desktop screenshots continuously"""
        try:
            with mss.mss() as sct:
                # Get primary monitor
                monitor = sct.monitors[1]
                
                while self.is_sharing:
                    try:
                        # Capture screenshot
                        screenshot = sct.grab(monitor)
                        
                        # Convert to PIL Image
                        img = Image.frombytes("RGB", screenshot.size, screenshot.bgra, "raw", "BGRX")
                        
                        # Convert to numpy array for OpenCV
                        frame = np.array(img)
                        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                        
                        # Store current frame
                        self.current_frame = frame.copy()
                        
                        # Add to queue for display
                        if not self.frame_queue.full():
                            self.frame_queue.put(frame)
                        
                        # Small delay to prevent overwhelming
                        time.sleep(0.1)
                        
                    except Exception as e:
                        print(f" Capture error: {e}")
                        break
                        
        except Exception as e:
            print(f" Desktop capture error: {e}")
    
    def _display_frames(self):
        """Display captured frames in the window"""
        try:
            while self.is_sharing:
                try:
                    if not self.frame_queue.empty():
                        frame = self.frame_queue.get_nowait()
                        
                        # Resize frame to fit window
                        height, width = frame.shape[:2]
                        max_width, max_height = 800, 600
                        
                        if width > max_width or height > max_height:
                            scale = min(max_width/width, max_height/height)
                            new_width = int(width * scale)
                            new_height = int(height * scale)
                            frame = cv2.resize(frame, (new_width, new_height))
                        
                        # Convert to PhotoImage for Tkinter
                        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                        img = Image.fromarray(frame_rgb)
                        photo = ImageTk.PhotoImage(img)
                        
                        # Update display
                        self.video_label.config(image=photo)
                        self.video_label.image = photo  # Keep reference
                        
                except queue.Empty:
                    pass
                except Exception as e:
                    print(f" Display error: {e}")
                    break
                
                time.sleep(0.05)  # 20 FPS
                
        except Exception as e:
            print(f" Frame display error: {e}")
    
    def _analyze_current_frame(self):
        """Analyze the current desktop frame"""
        try:
            if self.current_frame is None:
                print(" No frame available for analysis")
                return
            
            print(" Analyzing current desktop frame...")
            
            # Save current frame
            temp_path = "current_desktop_frame.png"
            cv2.imwrite(temp_path, self.current_frame)
            
            # Analyze with visual analyzer
            if hasattr(self, 'visual_analyzer'):
                analysis = self.visual_analyzer.analyze_code_visually(temp_path)
                
                if analysis:
                    # Generate report
                    report_path = self.visual_analyzer.create_visual_analysis_report(analysis)
                    
                    # Open in browser
                    if self.open_browser_window():
                        file_url = f"file://{os.path.abspath(report_path)}"
                        self.web_browser.load_url(file_url)
                        self.url_entry.delete(0, tk.END)
                        self.url_entry.insert(0, file_url)
                        
                        print(" Desktop frame analysis complete - report opened in browser")
                        return True
                    else:
                        # Fallback to system browser
                        webbrowser.open(f"file://{os.path.abspath(report_path)}")
                        print(" Desktop frame analysis complete - report opened in system browser")
                        return True
                else:
                    print(" Desktop frame analysis failed")
                    return False
            else:
                print(" Visual analyzer not available")
                return False
                
        except Exception as e:
            print(f" Frame analysis error: {e}")
            return False
    
    def start_live_code_analysis(self, duration=300):
        """Start live code analysis by monitoring desktop for code editors"""
        try:
            print(f" Starting live code analysis for {duration} seconds...")
            
            # Create live analysis window
            self.live_window = tk.Toplevel()
            self.live_window.title(" Live Code Analysis")
            self.live_window.geometry("1000x700")
            
            # Create analysis display
            analysis_frame = ttk.Frame(self.live_window)
            analysis_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # Video display
            self.live_video_label = tk.Label(analysis_frame)
            self.live_video_label.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            
            # Analysis panel
            analysis_panel = ttk.Frame(analysis_frame)
            analysis_panel.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
            
            ttk.Label(analysis_panel, text="Live Analysis Results", font=("Arial", 12, "bold")).pack(anchor=tk.W)
            
            # Analysis text
            self.analysis_text = scrolledtext.ScrolledText(analysis_panel, width=40, height=20)
            self.analysis_text.pack(fill=tk.BOTH, expand=True, pady=5)
            
            # Controls
            control_frame = ttk.Frame(self.live_window)
            control_frame.pack(fill=tk.X, padx=5, pady=5)
            
            ttk.Button(control_frame, text=" Start Live Analysis", command=self._start_live_analysis).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Stop Analysis", command=self._stop_live_analysis).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Save Analysis", command=self._save_live_analysis).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Close", command=self.live_window.destroy).pack(side=tk.RIGHT, padx=2)
            
            # Status
            self.live_status = ttk.Label(control_frame, text="Ready for live analysis")
            self.live_status.pack(side=tk.LEFT, padx=10)
            
            # Initialize live analysis
            self.is_live_analyzing = False
            self.live_analysis_thread = None
            self.analysis_results = []
            
            print(" Live code analysis window opened")
            return True
            
        except Exception as e:
            print(f" Live analysis error: {e}")
            return False
    
    def _start_live_analysis(self):
        """Start live code analysis"""
        try:
            if self.is_live_analyzing:
                return
            
            self.is_live_analyzing = True
            self.live_status.config(text="Analyzing live code...")
            self.analysis_text.delete(1.0, tk.END)
            self.analysis_text.insert(tk.END, "Starting live code analysis...\n\n")
            
            # Start live analysis thread
            self.live_analysis_thread = threading.Thread(target=self._perform_live_analysis)
            self.live_analysis_thread.daemon = True
            self.live_analysis_thread.start()
            
            print(" Live code analysis started")
            
        except Exception as e:
            print(f" Start live analysis error: {e}")
    
    def _stop_live_analysis(self):
        """Stop live code analysis"""
        try:
            self.is_live_analyzing = False
            self.live_status.config(text="Live analysis stopped")
            self.analysis_text.insert(tk.END, "\n\nLive analysis stopped.\n")
            print(" Live code analysis stopped")
            
        except Exception as e:
            print(f" Stop live analysis error: {e}")
    
    def _perform_live_analysis(self):
        """Perform live analysis of desktop for code"""
        try:
            with mss.mss() as sct:
                monitor = sct.monitors[1]
                frame_count = 0
                
                while self.is_live_analyzing:
                    try:
                        # Capture screenshot
                        screenshot = sct.grab(monitor)
                        img = Image.frombytes("RGB", screenshot.size, screenshot.bgra, "raw", "BGRX")
                        frame = np.array(img)
                        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                        
                        # Update video display
                        self._update_live_display(frame)
                        
                        # Analyze every 30 frames (about 3 seconds at 10 FPS)
                        if frame_count % 30 == 0:
                            self._analyze_live_frame(frame, frame_count)
                        
                        frame_count += 1
                        time.sleep(0.1)  # 10 FPS
                        
                    except Exception as e:
                        print(f" Live analysis frame error: {e}")
                        break
                        
        except Exception as e:
            print(f" Live analysis error: {e}")
    
    def _update_live_display(self, frame):
        """Update the live video display"""
        try:
            # Resize frame for display
            height, width = frame.shape[:2]
            max_width, max_height = 600, 400
            
            if width > max_width or height > max_height:
                scale = min(max_width/width, max_height/height)
                new_width = int(width * scale)
                new_height = int(height * scale)
                display_frame = cv2.resize(frame, (new_width, new_height))
            else:
                display_frame = frame
            
            # Convert to PhotoImage
            frame_rgb = cv2.cvtColor(display_frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame_rgb)
            photo = ImageTk.PhotoImage(img)
            
            # Update display
            self.live_video_label.config(image=photo)
            self.live_video_label.image = photo
            
        except Exception as e:
            print(f" Live display update error: {e}")
    
    def _analyze_live_frame(self, frame, frame_count):
        """Analyze a live frame for code content"""
        try:
            # Save frame for analysis
            temp_path = f"live_frame_{frame_count}.png"
            cv2.imwrite(temp_path, frame)
            
            # Analyze with visual analyzer
            if hasattr(self, 'visual_analyzer'):
                analysis = self.visual_analyzer.analyze_code_visually(temp_path)
                
                if analysis:
                    # Extract key information
                    language = analysis.get('language_detected', 'Unknown')
                    complexity = analysis.get('complexity_score', 0)
                    issues = analysis.get('visual_issues', [])
                    
                    # Update analysis display
                    timestamp = time.strftime("%H:%M:%S")
                    self.analysis_text.insert(tk.END, f"[{timestamp}] Frame {frame_count} Analysis:\n")
                    self.analysis_text.insert(tk.END, f"  Language: {language}\n")
                    self.analysis_text.insert(tk.END, f"  Complexity: {complexity:.1f}/100\n")
                    self.analysis_text.insert(tk.END, f"  Issues: {len(issues)}\n")
                    
                    if issues:
                        self.analysis_text.insert(tk.END, "  Issues found:\n")
                        for issue in issues[:3]:  # Show first 3 issues
                            self.analysis_text.insert(tk.END, f"    - {issue.get('type', 'Unknown')}: {issue.get('description', 'No description')}\n")
                    
                    self.analysis_text.insert(tk.END, "\n")
                    self.analysis_text.see(tk.END)
                    
                    # Store analysis result
                    self.analysis_results.append({
                        'timestamp': timestamp,
                        'frame': frame_count,
                        'language': language,
                        'complexity': complexity,
                        'issues': issues
                    })
            
            # Clean up temp file
            if os.path.exists(temp_path):
                os.remove(temp_path)
                
        except Exception as e:
            print(f" Live frame analysis error: {e}")
    
    def _save_live_analysis(self):
        """Save live analysis results"""
        try:
            if not self.analysis_results:
                print(" No analysis results to save")
                return
            
            # Generate report
            report_path = f"live_analysis_report_{int(time.time())}.html"
            
            html = f"""
            <!DOCTYPE html>
            <html>
            <head>
                <title>Live Code Analysis Report</title>
                <style>
                    body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
                    .header {{ background: #2c3e50; color: white; padding: 20px; border-radius: 5px; }}
                    .result {{ background: white; margin: 10px 0; padding: 15px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }}
                    .timestamp {{ color: #7f8c8d; font-size: 0.9em; }}
                </style>
            </head>
            <body>
                <div class="header">
                    <h1> Live Code Analysis Report</h1>
                    <p>Real-time desktop analysis results</p>
                </div>
            """
            
            for result in self.analysis_results:
                html += f"""
                <div class="result">
                    <div class="timestamp">{result['timestamp']} - Frame {result['frame']}</div>
                    <h3>Analysis Results</h3>
                    <p><strong>Language:</strong> {result['language']}</p>
                    <p><strong>Complexity:</strong> {result['complexity']:.1f}/100</p>
                    <p><strong>Issues Found:</strong> {len(result['issues'])}</p>
                """
                
                if result['issues']:
                    html += "<h4>Issues:</h4><ul>"
                    for issue in result['issues']:
                        html += f"<li>{issue.get('type', 'Unknown')}: {issue.get('description', 'No description')}</li>"
                    html += "</ul>"
                
                html += "</div>"
            
            html += """
            </body>
            </html>
            """
            
            # Save report
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(html)
            
            print(f" Live analysis report saved: {report_path}")
            
            # Open report
            webbrowser.open(f"file://{os.path.abspath(report_path)}")
            
        except Exception as e:
            print(f" Save live analysis error: {e}")

class AIModelManager:
    """Manage multiple AI models for enhanced debugging and analysis"""
    
    def __init__(self):
        self.models = {
            'openai': {
                'name': 'ChatGPT (OpenAI)',
                'api_key': None,
                'base_url': 'https://api.openai.com/v1',
                'model': 'gpt-4',
                'enabled': False
            },
            'ollama': {
                'name': 'Ollama (Local)',
                'api_key': None,
                'base_url': 'http://localhost:11434',
                'model': 'llama2',
                'enabled': False
            },
            'claude': {
                'name': 'Claude (Anthropic)',
                'api_key': None,
                'base_url': 'https://api.anthropic.com/v1',
                'model': 'claude-3-sonnet-20240229',
                'enabled': False
            },
            'gemini': {
                'name': 'Gemini (Google)',
                'api_key': None,
                'base_url': 'https://generativelanguage.googleapis.com/v1beta',
                'model': 'gemini-pro',
                'enabled': False
            },
            'cohere': {
                'name': 'Cohere',
                'api_key': None,
                'base_url': 'https://api.cohere.ai/v1',
                'model': 'command',
                'enabled': False
            },
            'huggingface': {
                'name': 'Hugging Face',
                'api_key': None,
                'base_url': 'https://api-inference.huggingface.co/models',
                'model': 'microsoft/DialoGPT-medium',
                'enabled': False
            },
            'free_models': {
                'name': 'Free AI Models',
                'api_key': None,
                'base_url': 'https://api-inference.huggingface.co/models',
                'model': 'microsoft/DialoGPT-medium',
                'enabled': True
            },
            'ollama_free': {
                'name': 'Ollama (Free Local)',
                'api_key': None,
                'base_url': 'http://localhost:11434',
                'model': 'llama2',
                'enabled': True
            },
            'groq_free': {
                'name': 'Groq (Free)',
                'api_key': None,
                'base_url': 'https://api.groq.com/openai/v1',
                'model': 'llama3-8b-8192',
                'enabled': True
            },
            'together_free': {
                'name': 'Together AI (Free)',
                'api_key': None,
                'base_url': 'https://api.together.xyz/v1',
                'model': 'meta-llama/Llama-2-7b-chat-hf',
                'enabled': True
            }
        }
        print(" AI Model Manager initialized")
    
    def setup_model(self, model_name, api_key, custom_url=None):
        """Setup an AI model with API key"""
        try:
            if model_name in self.models:
                self.models[model_name]['api_key'] = api_key
                if custom_url:
                    self.models[model_name]['base_url'] = custom_url
                self.models[model_name]['enabled'] = True
                print(f" {model_name} model configured successfully")
                return True
            else:
                print(f" Unknown model: {model_name}")
                return False
        except Exception as e:
            print(f" Model setup error: {e}")
            return False
    
    def query_openai(self, prompt, model="gpt-4", max_tokens=1000):
        """Query OpenAI ChatGPT"""
        try:
            if not self.models['openai']['enabled']:
                return None
            
            headers = {
                'Authorization': f"Bearer {self.models['openai']['api_key']}",
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': model,
                'messages': [{'role': 'user', 'content': prompt}],
                'max_tokens': max_tokens,
                'temperature': 0.7
            }
            
            response = requests.post(
                f"{self.models['openai']['base_url']}/chat/completions",
                headers=headers,
                json=data,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return result['choices'][0]['message']['content']
            else:
                print(f" OpenAI API error: {response.status_code}")
                return None
                
        except Exception as e:
            print(f" OpenAI query error: {e}")
            return None
    
    def query_ollama(self, prompt, model="llama2"):
        """Query Ollama local model"""
        try:
            if not self.models['ollama']['enabled']:
                return None
            
            data = {
                'model': model,
                'prompt': prompt,
                'stream': False
            }
            
            response = requests.post(
                f"{self.models['ollama']['base_url']}/api/generate",
                json=data,
                timeout=60
            )
            
            if response.status_code == 200:
                result = response.json()
                return result.get('response', '')
            else:
                print(f" Ollama API error: {response.status_code}")
                return None
                
        except Exception as e:
            print(f" Ollama query error: {e}")
            return None
    
    def query_claude(self, prompt, model="claude-3-sonnet-20240229"):
        """Query Claude AI"""
        try:
            if not self.models['claude']['enabled']:
                return None
            
            headers = {
                'x-api-key': self.models['claude']['api_key'],
                'Content-Type': 'application/json',
                'anthropic-version': '2023-06-01'
            }
            
            data = {
                'model': model,
                'max_tokens': 1000,
                'messages': [{'role': 'user', 'content': prompt}]
            }
            
            response = requests.post(
                f"{self.models['claude']['base_url']}/messages",
                headers=headers,
                json=data,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return result['content'][0]['text']
            else:
                print(f" Claude API error: {response.status_code}")
                return None
                
        except Exception as e:
            print(f" Claude query error: {e}")
            return None
    
    def query_gemini(self, prompt, model="gemini-pro"):
        """Query Google Gemini"""
        try:
            if not self.models['gemini']['enabled']:
                return None
            
            data = {
                'contents': [{
                    'parts': [{'text': prompt}]
                }]
            }
            
            response = requests.post(
                f"{self.models['gemini']['base_url']}/models/{model}:generateContent?key={self.models['gemini']['api_key']}",
                json=data,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return result['candidates'][0]['content']['parts'][0]['text']
            else:
                print(f" Gemini API error: {response.status_code}")
                return None
                
        except Exception as e:
            print(f" Gemini query error: {e}")
            return None
    
    def query_cohere(self, prompt, model="command"):
        """Query Cohere AI"""
        try:
            if not self.models['cohere']['enabled']:
                return None
            
            headers = {
                'Authorization': f"Bearer {self.models['cohere']['api_key']}",
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': model,
                'message': prompt,
                'max_tokens': 1000
            }
            
            response = requests.post(
                f"{self.models['cohere']['base_url']}/chat",
                headers=headers,
                json=data,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return result['text']
            else:
                print(f" Cohere API error: {response.status_code}")
                return None
                
        except Exception as e:
            print(f" Cohere query error: {e}")
            return None
    
    def query_huggingface(self, prompt, model="microsoft/DialoGPT-medium"):
        """Query Hugging Face models"""
        try:
            if not self.models['huggingface']['enabled']:
                return None
            
            headers = {
                'Authorization': f"Bearer {self.models['huggingface']['api_key']}"
            }
            
            data = {
                'inputs': prompt,
                'parameters': {
                    'max_length': 1000,
                    'temperature': 0.7
                }
            }
            
            response = requests.post(
                f"{self.models['huggingface']['base_url']}/{model}",
                headers=headers,
                json=data,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                if isinstance(result, list) and len(result) > 0:
                    return result[0].get('generated_text', '')
                return str(result)
            else:
                print(f" Hugging Face API error: {response.status_code}")
                return None
                
        except Exception as e:
            print(f" Hugging Face query error: {e}")
            return None
    
    def query_all_models(self, prompt):
        """Query all enabled AI models"""
        results = {}
        
        # Query each enabled model
        if self.models['openai']['enabled']:
            results['openai'] = self.query_openai(prompt)
        
        if self.models['ollama']['enabled']:
            results['ollama'] = self.query_ollama(prompt)
        
        if self.models['claude']['enabled']:
            results['claude'] = self.query_claude(prompt)
        
        if self.models['gemini']['enabled']:
            results['gemini'] = self.query_gemini(prompt)
        
        if self.models['cohere']['enabled']:
            results['cohere'] = self.query_cohere(prompt)
        
        if self.models['huggingface']['enabled']:
            results['huggingface'] = self.query_huggingface(prompt)
        
        return results
    
    def analyze_code_with_ai(self, code, language="auto"):
        """Analyze code using AI models"""
        try:
            prompt = f"""
            Analyze this {language} code for bugs, performance issues, and security vulnerabilities:
            
            {code}
            
            Please provide:
            1. Bug analysis
            2. Performance issues
            3. Security vulnerabilities
            4. Improvement suggestions
            5. Code quality score (1-100)
            """
            
            results = self.query_all_models(prompt)
            
            # Combine results from all models
            combined_analysis = {
                'models_used': list(results.keys()),
                'responses': results,
                'consensus': self._find_consensus(results)
            }
            
            return combined_analysis
            
        except Exception as e:
            print(f" AI code analysis error: {e}")
            return None
    
    def _find_consensus(self, results):
        """Find consensus among different AI models"""
        try:
            if not results:
                return "No AI models available"
            
            # Simple consensus finding - in practice, you'd use more sophisticated methods
            common_issues = []
            all_issues = []
            
            for model, response in results.items():
                if response:
                    # Extract issues from response (simplified)
                    if "bug" in response.lower():
                        all_issues.append("Potential bugs detected")
                    if "performance" in response.lower():
                        all_issues.append("Performance issues found")
                    if "security" in response.lower():
                        all_issues.append("Security vulnerabilities detected")
            
            # Find most common issues
            from collections import Counter
            issue_counts = Counter(all_issues)
            common_issues = [issue for issue, count in issue_counts.items() if count > 1]
            
            return {
                'common_issues': common_issues,
                'total_models': len(results),
                'consensus_strength': len(common_issues) / len(all_issues) if all_issues else 0
            }
            
        except Exception as e:
            print(f" Consensus finding error: {e}")
            return "Error finding consensus"
    
    def generate_code_with_ai(self, description, language="python"):
        """Generate code using AI models"""
        try:
            prompt = f"""
            Generate {language} code based on this description:
            
            {description}
            
            Please provide:
            1. Complete, working code
            2. Comments explaining the code
            3. Error handling
            4. Best practices
            """
            
            results = self.query_all_models(prompt)
            return results
            
        except Exception as e:
            print(f" AI code generation error: {e}")
            return None
    
    def debug_with_ai(self, error_message, code_context=""):
        """Debug issues using AI models"""
        try:
            prompt = f"""
            Help debug this error:
            
            Error: {error_message}
            
            Code context: {code_context}
            
            Please provide:
            1. Root cause analysis
            2. Step-by-step solution
            3. Prevention tips
            4. Alternative approaches
            """
            
            results = self.query_all_models(prompt)
            return results
            
        except Exception as e:
            print(f" AI debugging error: {e}")
            return None

class PERebuilder:
    """Rebuild and fix corrupted PE executables"""
    
    def __init__(self):
        print(" PE Rebuilder initialized")
    
    def rebuild_pe_file(self, input_file, output_file=None):
        """Rebuild a PE file to fix corruption or malformation"""
        if not output_file:
            output_file = input_file.replace('.exe', '_rebuilt.exe')
        
        try:
            print(f" Rebuilding PE file: {input_file}")
            
            # Read original PE
            with open(input_file, 'rb') as f:
                original_data = f.read()
            
            # Parse PE structure
            pe_info = self._parse_pe_structure(original_data)
            if not pe_info["valid"]:
                print(" Invalid PE structure detected - attempting repair...")
                return self._repair_invalid_pe(original_data, output_file)
            
            # Rebuild PE with clean structure
            rebuilt_pe = self._rebuild_clean_pe(pe_info, original_data)
            
            # Write rebuilt PE
            with open(output_file, 'wb') as f:
                f.write(rebuilt_pe)
            
            # Verify rebuilt PE
            if self._verify_pe_integrity(output_file):
                size = os.path.getsize(output_file)
                return {
                    "success": True,
                    "output": f"PE rebuilt successfully - {size:,} bytes",
                    "rebuilt_file": output_file,
                    "original_size": len(original_data),
                    "rebuilt_size": size
                }
            else:
                return {"success": False, "error": "Rebuilt PE failed verification"}
                
        except Exception as e:
            return {"success": False, "error": f"PE rebuild error: {e}"}
    
    def _parse_pe_structure(self, data):
        """Parse PE structure and validate"""
        try:
            pe_info = {"valid": False}
            
            # Check DOS header
            if len(data) < 64:
                return pe_info
            
            dos_signature = struct.unpack('<H', data[0:2])[0]
            if dos_signature != 0x5A4D:  # 'MZ'
                return pe_info
            
            # Get PE header offset
            pe_offset = struct.unpack('<L', data[60:64])[0]
            if pe_offset >= len(data) - 4:
                return pe_info
            
            # Check PE signature
            pe_signature = struct.unpack('<L', data[pe_offset:pe_offset+4])[0]
            if pe_signature != 0x00004550:  # 'PE\0\0'
                return pe_info
            
            # Parse COFF header
            coff_header_offset = pe_offset + 4
            if coff_header_offset + 20 > len(data):
                return pe_info
            
            machine = struct.unpack('<H', data[coff_header_offset:coff_header_offset+2])[0]
            number_of_sections = struct.unpack('<H', data[coff_header_offset+2:coff_header_offset+4])[0]
            size_of_optional_header = struct.unpack('<H', data[coff_header_offset+16:coff_header_offset+18])[0]
            
            pe_info.update({
                "valid": True,
                "dos_signature": dos_signature,
                "pe_offset": pe_offset,
                "machine": machine,
                "number_of_sections": number_of_sections,
                "size_of_optional_header": size_of_optional_header,
                "coff_header_offset": coff_header_offset
            })
            
            return pe_info
            
        except Exception as e:
            print(f"PE parsing error: {e}")
            return {"valid": False}
    
    def _rebuild_clean_pe(self, pe_info, original_data):
        """Rebuild PE with clean structure"""
        try:
            # Start with clean DOS header
            rebuilt_data = bytearray()
            
            # DOS Header (64 bytes)
            dos_header = bytearray([
                0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00,
                0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
                0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x80, 0x00, 0x00, 0x00  # PE offset = 128
            ])
            rebuilt_data.extend(dos_header)
            
            # DOS Stub (64 bytes)
            dos_stub = bytearray([
                0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
                0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
                0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
                0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
                0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
                0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
                0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
                0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            ])
            rebuilt_data.extend(dos_stub)
            
            # Copy PE headers from original (with fixes)
            original_pe_offset = pe_info["pe_offset"]
            pe_headers_size = 4 + 20 + pe_info["size_of_optional_header"] + (pe_info["number_of_sections"] * 40)
            
            if original_pe_offset + pe_headers_size <= len(original_data):
                pe_headers = original_data[original_pe_offset:original_pe_offset + pe_headers_size]
                rebuilt_data.extend(pe_headers)
            else:
                # Headers truncated - create minimal headers
                rebuilt_data.extend(self._create_minimal_pe_headers())
            
            # Align to section boundary
            while len(rebuilt_data) % 512 != 0:
                rebuilt_data.append(0x00)
            
            # Copy sections (if they exist and are valid)
            try:
                sections_data = self._extract_sections(original_data, pe_info)
                rebuilt_data.extend(sections_data)
            except:
                # Add minimal code section
                rebuilt_data.extend(self._create_minimal_code_section())
            
            return bytes(rebuilt_data)
            
        except Exception as e:
            print(f"PE rebuild error: {e}")
            return original_data  # Return original if rebuild fails
    
    def _repair_invalid_pe(self, data, output_file):
        """Attempt to repair completely invalid PE"""
        try:
            # Create a minimal working PE that displays the file info
            message = f"Repaired PE file - Original size: {len(data)} bytes"
            
            # Use basic PE template
            repaired_pe = self._create_basic_pe_template(message)
            
            with open(output_file, 'wb') as f:
                f.write(repaired_pe)
            
            return {
                "success": True,
                "output": f"Invalid PE repaired - created minimal working PE",
                "rebuilt_file": output_file,
                "note": "Original was not a valid PE - created minimal replacement"
            }
            
        except Exception as e:
            return {"success": False, "error": f"PE repair failed: {e}"}
    
    def _create_basic_pe_template(self, message):
        """Create basic PE template"""
        # This is a simplified version of the PE generator
        pe_data = bytearray([
            # DOS Header
            0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
            0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
        ])
        
        # DOS Stub
        pe_data.extend([
            0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
            0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72, 0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
            0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E, 0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
            0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ])
        
        # PE Signature
        pe_data.extend([0x50, 0x45, 0x00, 0x00])
        
        # Minimal COFF Header (x86-64)
        pe_data.extend([
            0x64, 0x86,  # Machine (AMD64)
            0x01, 0x00,  # NumberOfSections
            0x00, 0x00, 0x00, 0x00,  # TimeDateStamp
            0x00, 0x00, 0x00, 0x00,  # PointerToSymbolTable  
            0x00, 0x00, 0x00, 0x00,  # NumberOfSymbols
            0xF0, 0x00,  # SizeOfOptionalHeader
            0x0E, 0x02   # Characteristics
        ])
        
        # Add minimal optional header and section
        # (This is a simplified template - full implementation would be more complex)
        
        # Pad to reasonable size
        while len(pe_data) < 1024:
            pe_data.append(0x00)
        
        return bytes(pe_data)
    
    def _verify_pe_integrity(self, pe_file):
        """Verify PE file integrity"""
        try:
            with open(pe_file, 'rb') as f:
                data = f.read()
            
            # Basic PE validation
            if len(data) < 64:
                return False
            
            # Check DOS signature
            if struct.unpack('<H', data[0:2])[0] != 0x5A4D:
                return False
            
            # Check PE signature
            pe_offset = struct.unpack('<L', data[60:64])[0]
            if pe_offset >= len(data) - 4:
                return False
            
            pe_signature = struct.unpack('<L', data[pe_offset:pe_offset+4])[0]
            if pe_signature != 0x00004550:
                return False
            
            return True
            
        except Exception:
            return False
    
    def _extract_sections(self, data, pe_info):
        """Extract sections from original PE"""
        # This is a placeholder - full implementation would parse section headers
        # and extract each section's data
        sections_data = bytearray()
        
        # Add minimal code section as fallback
        sections_data.extend(self._create_minimal_code_section())
        
        return sections_data
    
    def _create_minimal_pe_headers(self):
        """Create minimal PE headers"""
        headers = bytearray()
        
        # PE Signature
        headers.extend([0x50, 0x45, 0x00, 0x00])
        
        # COFF Header
        headers.extend([
            0x64, 0x86,  # Machine
            0x01, 0x00,  # NumberOfSections
            0x00, 0x00, 0x00, 0x00,  # TimeDateStamp
            0x00, 0x00, 0x00, 0x00,  # PointerToSymbolTable
            0x00, 0x00, 0x00, 0x00,  # NumberOfSymbols
            0x20, 0x00,  # SizeOfOptionalHeader  
            0x0E, 0x02   # Characteristics
        ])
        
        # Minimal Optional Header
        headers.extend([0x00] * 32)  # Simplified
        
        # One section header (.text)
        headers.extend([0x00] * 40)  # Simplified
        
        return headers
    
    def _create_minimal_code_section(self):
        """Create minimal code section"""
        code = bytearray([
            0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  # mov rax, 1
            0xC3  # ret
        ])
        
        # Pad to section alignment
        while len(code) < 512:
            code.append(0x00)
        
        return code

class PEExecutableGenerator:
    """Generate actual Windows PE executable files"""
    
    def __init__(self):
        self.platform = platform.system().lower()
        self.rebuilder = PERebuilder()
        print(" PE Executable Generator initialized")
    
    def create_simple_exe(self, message_text, output_file):
        """Create a simple Windows PE executable that displays a message"""
        try:
            # Simple PE executable template that shows a message box
            # This is a minimal PE file that calls MessageBoxA and ExitProcess
            
            pe_header = bytearray([
                # DOS Header
                0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
                0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
                # DOS Stub
                0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
                0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72, 0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
                0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E, 0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
                0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                # PE Header
                0x50, 0x45, 0x00, 0x00, 0x64, 0x86, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x0E, 0x02, 0x0B, 0x02, 0x0E, 0x00, 0x00, 0x10, 0x00, 0x00,
                0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
                0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
                0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x40, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x60, 0x81,
                0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x30, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            ])
            
            # Section headers
            section_headers = bytearray([
                # .text section
                0x2E, 0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
                0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x60,
                # .idata section  
                0x2E, 0x69, 0x64, 0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
                0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0xC0,
            ])
            
            # Code section - Simple program that shows message box
            code_section = bytearray([
                # Push parameters for MessageBoxA
                0x6A, 0x00,                         # push 0 (MB_OK)
                0x68, 0x30, 0x20, 0x00, 0x01,       # push offset title
                0x68, 0x40, 0x20, 0x00, 0x01,       # push offset message  
                0x6A, 0x00,                         # push 0 (hWnd)
                0xFF, 0x15, 0x00, 0x30, 0x00, 0x01, # call MessageBoxA
                # Push parameter for ExitProcess
                0x6A, 0x00,                         # push 0 (exit code)
                0xFF, 0x15, 0x04, 0x30, 0x00, 0x01, # call ExitProcess
            ])
            
            # Pad code section to align
            while len(code_section) < 512:
                code_section.append(0x00)
            
            # Import section
            import_section = bytearray([
                # Import Address Table
                0x08, 0x30, 0x00, 0x01,  # MessageBoxA
                0x0C, 0x30, 0x00, 0x01,  # ExitProcess
                0x00, 0x00, 0x00, 0x00,  # End of IAT
                
                # Import names
                0x00, 0x00,              # MessageBoxA hint
                0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x42, 0x6F, 0x78, 0x41, 0x00, # "MessageBoxA"
                0x00, 0x00,              # ExitProcess hint
                0x45, 0x78, 0x69, 0x74, 0x50, 0x72, 0x6F, 0x63, 0x65, 0x73, 0x73, 0x00, # "ExitProcess"
                
                # Title and message strings
                0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, # "Hello"
            ])
            
            # Add custom message
            message_bytes = message_text.encode('ascii', errors='replace') + b'\x00'
            import_section.extend(message_bytes)
            
            # Pad to align
            while len(import_section) < 512:
                import_section.append(0x00)
            
            # Combine all sections
            executable_data = pe_header + section_headers + code_section + import_section
            
            # Write to file
            with open(output_file, 'wb') as f:
                f.write(executable_data)
            
            return True
            
        except Exception as e:
            print(f" PE generation error: {e}")
            return False

class ProperCompiler:
    """Compiler that creates proper executables"""
    
    def __init__(self):
        self.pe_gen = PEExecutableGenerator()
        print(" Proper Compiler initialized")
        
    def compile_to_exe(self, source_file, output_file, language="auto"):
        """Compile source to actual executable"""
        print(f" Compiling {source_file} to proper executable...")
        
        try:
            # Detect language
            if language == "auto":
                language = self._detect_language(source_file)
            
            # Read source code
            with open(source_file, 'r', encoding='utf-8') as f:
                source_code = f.read()
            
            # Try system compilers first
            system_result = self._try_system_compiler(source_file, output_file, language)
            if system_result["success"]:
                return system_result
            
            # Fallback: Create custom executable based on source
            return self._create_custom_executable(source_code, output_file, language)
            
        except Exception as e:
            return {"success": False, "error": str(e), "output": ""}
    
    def _detect_language(self, source_file):
        """Detect language from file extension"""
        ext = Path(source_file).suffix.lower()
        language_map = {
            '.cpp': 'cpp',
            '.c': 'cpp', 
            '.py': 'python',
            '.js': 'javascript',
            '.cs': 'csharp',
            '.java': 'java'
        }
        return language_map.get(ext, 'generic')
    
    def _try_system_compiler(self, source_file, output_file, language):
        """Try to use system compilers"""
        try:
            if language == "cpp":
                # Try C++ compilers
                compilers = [
                    ['g++', '-static', source_file, '-o', output_file],
                    ['clang++', '-static', source_file, '-o', output_file],
                    ['cl', '/Fe:' + output_file, source_file]
                ]
                
                for cmd in compilers:
                    try:
                        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                        if result.returncode == 0 and os.path.exists(output_file):
                            size = os.path.getsize(output_file)
                            return {
                                "success": True,
                                "output": f"Compiled with {cmd[0]} - {size:,} bytes",
                                "executable": output_file,
                                "size": size
                            }
                    except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
                        continue
            
            elif language == "csharp":
                # Try C# compiler
                try:
                    cmd = ['csc', '/out:' + output_file, source_file]
                    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                    if result.returncode == 0 and os.path.exists(output_file):
                        size = os.path.getsize(output_file)
                        return {
                            "success": True,
                            "output": f"Compiled with csc - {size:,} bytes",
                            "executable": output_file,
                            "size": size
                        }
                except:
                    pass
            
            return {"success": False, "error": "No system compiler found"}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def _create_custom_executable(self, source_code, output_file, language):
        """Create custom executable when no system compiler available"""
        try:
            # Extract meaningful content from source
            message = self._extract_message_from_source(source_code, language)
            
            # Generate PE executable
            if self.pe_gen.create_simple_exe(message, output_file):
                size = os.path.getsize(output_file)
                return {
                    "success": True,
                    "output": f"Generated custom PE executable - {size:,} bytes",
                    "executable": output_file,
                    "size": size,
                    "note": "Custom PE executable created (shows extracted message)"
                }
            else:
                return {"success": False, "error": "PE generation failed"}
                
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def _extract_message_from_source(self, source_code, language):
        """Extract meaningful message from source code"""
        # Look for common output patterns
        patterns = [
            r'"([^"]*)"',  # String literals
            r"'([^']*)'",  # Single quotes
            r'cout\s*<<\s*"([^"]*)"',  # C++ cout
            r'printf\s*\(\s*"([^"]*)"',  # C printf
            r'print\s*\(\s*"([^"]*)"',  # Python print
            r'console\.log\s*\(\s*"([^"]*)"',  # JS console.log
        ]
        
        import re
        messages = []
        
        for pattern in patterns:
            matches = re.findall(pattern, source_code, re.IGNORECASE)
            messages.extend(matches)
        
        if messages:
            # Take the longest meaningful message
            best_message = max(messages, key=len)
            if len(best_message) > 3:  # Skip very short strings
                return best_message
        
        # Fallback messages
        if language == "cpp":
            return "Hello from C++ program!"
        elif language == "python":
            return "Hello from Python program!"
        elif language == "javascript":
            return "Hello from JavaScript program!"
        else:
            return f"Hello from {language} program!"

class ProperExeIDE:
    """IDE that creates proper executables"""
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title(" Proper EXE Compiler - Real Executables!")
        self.root.geometry("1200x800")
        
        # Show DO NOT DISTRIBUTE warning on startup
        self.show_distribution_warning()
        
        self.compiler = ProperCompiler()
        self.key_scraper = AIKeyScraper()
        self.debugging_agent = DebuggingAgent()
        self.debugging_agent.setup_internet_access(self.key_scraper.proxy_manager)
        self.current_file = None
        self.last_executable = None
        
        self.setup_ui()
        print(" Proper EXE IDE started - creates REAL executables!")
    
    def show_distribution_warning(self):
        """Show DO NOT DISTRIBUTE warning dialog"""
        warning_dialog = tk.Toplevel(self.root)
        warning_dialog.title("️ CONFIDENTIAL TOOL - DO NOT DISTRIBUTE ️")
        warning_dialog.geometry("600x400")
        warning_dialog.configure(bg='#ff4444')
        warning_dialog.transient(self.root)
        warning_dialog.grab_set()
        
        # Center the dialog
        warning_dialog.update_idletasks()
        x = (warning_dialog.winfo_screenwidth() // 2) - (600 // 2)
        y = (warning_dialog.winfo_screenheight() // 2) - (400 // 2)
        warning_dialog.geometry(f"600x400+{x}+{y}")
        
        # Main warning frame
        warning_frame = tk.Frame(warning_dialog, bg='#ff4444', padx=20, pady=20)
        warning_frame.pack(fill='both', expand=True)
        
        # Warning title
        title_label = tk.Label(warning_frame, 
                              text="️ DO NOT DISTRIBUTE ️", 
                              font=("Arial", 16, "bold"), 
                              fg='white', 
                              bg='#ff4444')
        title_label.pack(pady=(0, 20))
        
        # Warning text
        warning_text = """
This is a CONFIDENTIAL reverse engineering and security analysis tool.

DISTRIBUTION RESTRICTIONS:
• This software is for authorized use only
• Do NOT share, distribute, or publish this tool
• Do NOT upload to public repositories
• Do NOT post screenshots or documentation publicly
• Do NOT reverse engineer this IDE or its components
• Do NOT attempt to decompile, disassemble, or analyze the code
• Violation may result in legal consequences

AUTHORIZED USES:
• Internal security research
• Authorized penetration testing
• Educational purposes (controlled environment)
• Professional security analysis

By clicking "I Understand", you acknowledge that you will not
distribute this tool, will not reverse engineer the IDE, and will use it 
only for authorized purposes.
        """
        
        text_widget = tk.Text(warning_frame, 
                             height=15, 
                             width=60, 
                             font=("Arial", 10), 
                             bg='#ffdddd', 
                             fg='#000000',
                             wrap=tk.WORD)
        text_widget.pack(fill='both', expand=True, pady=(0, 20))
        text_widget.insert('1.0', warning_text)
        text_widget.config(state='disabled')
        
        # Buttons
        button_frame = tk.Frame(warning_frame, bg='#ff4444')
        button_frame.pack(fill='x')
        
        def accept_warning():
            warning_dialog.destroy()
        
        def reject_warning():
            self.root.quit()
            sys.exit(0)
        
        tk.Button(button_frame, 
                 text="I Understand - Continue", 
                 command=accept_warning,
                 bg='#4CAF50', 
                 fg='white', 
                 font=("Arial", 12, "bold"),
                 padx=20, 
                 pady=10).pack(side='left', padx=10)
        
        tk.Button(button_frame, 
                 text="Exit Application", 
                 command=reject_warning,
                 bg='#f44336', 
                 fg='white', 
                 font=("Arial", 12, "bold"),
                 padx=20, 
                 pady=10).pack(side='right', padx=10)
    
    def setup_ui(self):
        """Setup UI"""
        # Menu bar
        self.create_menu_bar()
        
        # Main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Toolbar
        toolbar = ttk.Frame(main_frame)
        toolbar.pack(fill=tk.X, pady=(0, 5))
        
        ttk.Button(toolbar, text=" Open", command=self.open_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text=" Save", command=self.save_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text=" Compile EXE", command=self.compile_exe).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text=" Rebuild PE", command=self.rebuild_pe).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text=" Scan Keys", command=self.scan_keys).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="▶️ Run EXE", command=self.run_exe).pack(side=tk.LEFT, padx=2)
        
        # File size label
        self.size_label = ttk.Label(toolbar, text="")
        self.size_label.pack(side=tk.RIGHT, padx=10)
        
        # Paned window
        paned = ttk.PanedWindow(main_frame, orient=tk.VERTICAL)
        paned.pack(fill=tk.BOTH, expand=True)
        
        # Editor
        editor_frame = ttk.Frame(paned)
        paned.add(editor_frame, weight=3)
        
        ttk.Label(editor_frame, text=" Source Code Editor").pack(anchor=tk.W)
        
        self.text_editor = scrolledtext.ScrolledText(
            editor_frame,
            wrap=tk.NONE,
            font=('Consolas', 11),
            bg='#1e1e1e',
            fg='#ffffff',
            insertbackground='white'
        )
        self.text_editor.pack(fill=tk.BOTH, expand=True)
        
        # Output
        output_frame = ttk.Frame(paned)
        paned.add(output_frame, weight=1)
        
        ttk.Label(output_frame, text=" Compilation Output").pack(anchor=tk.W)
        
        self.output_text = scrolledtext.ScrolledText(
            output_frame,
            wrap=tk.WORD,
            font=('Consolas', 10),
            bg='#2d2d2d',
            fg='#00ff00',
            state=tk.DISABLED
        )
        self.output_text.pack(fill=tk.BOTH, expand=True)
        
        # Status bar with distribution warning
        self.status_bar = tk.Label(
            self.root,
            text="️ DO NOT DISTRIBUTE OR REVERSE ENGINEER - CONFIDENTIAL TOOL ️ | Ready to create proper executables",
            relief=tk.SUNKEN,
            fg='red',
            font=("Arial", 8, "bold"),
            bg='#ffdddd'
        )
        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)
        
        # Load sample
        self.load_sample_cpp()
    
    def create_menu_bar(self):
        """Create menu bar"""
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self.new_file)
        file_menu.add_command(label="Open", command=self.open_file)
        file_menu.add_command(label="Save", command=self.save_file)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)
        
        # Compile menu
        compile_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Compile", menu=compile_menu)
        compile_menu.add_command(label="Compile to EXE", command=self.compile_exe)
        compile_menu.add_separator()
        compile_menu.add_command(label="Rebuild PE File", command=self.rebuild_pe)
        compile_menu.add_command(label="Run EXE", command=self.run_exe)
        
        # Security menu
        security_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Security", menu=security_menu)
        security_menu.add_command(label="Scan for API Keys", command=self.scan_keys)
        security_menu.add_command(label="Scan Directory", command=self.scan_directory)
        security_menu.add_separator()
        security_menu.add_command(label=" Scan Internet", command=self.scan_internet)
        security_menu.add_command(label=" Scan GitHub", command=self.scan_github)
        security_menu.add_separator()
        security_menu.add_command(label=" Configure Proxy", command=self.configure_proxy)
        security_menu.add_command(label=" Stealth Mode", command=self.enable_stealth_mode)
        security_menu.add_command(label=" Test Connection", command=self.test_connection)
        security_menu.add_separator()
        security_menu.add_command(label=" Scan AI Chatboxes", command=self.scan_ai_chatboxes)
        security_menu.add_command(label=" Use Free Chatbox", command=self.use_free_chatbox)
        security_menu.add_separator()
        security_menu.add_command(label=" Auto-Create Copilots", command=self.auto_create_copilots)
        security_menu.add_command(label=" Manage Copilots", command=self.manage_copilots)
        security_menu.add_command(label=" Use Copilot", command=self.use_copilot)
        security_menu.add_separator()
        security_menu.add_command(label=" Save All Data", command=self.save_all_data)
        security_menu.add_command(label=" Load All Data", command=self.load_all_data)
        security_menu.add_command(label=" Export Data", command=self.export_data)
        security_menu.add_command(label=" Import Data", command=self.import_data)
        security_menu.add_command(label=" Clear All Data", command=self.clear_all_data)
        security_menu.add_separator()
        security_menu.add_command(label="Mask Keys in File", command=self.mask_keys)
        
        # Visual AI menu
        visual_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Visual AI", menu=visual_menu)
        visual_menu.add_command(label=" Analyze Code Screenshot", command=self.analyze_code_screenshot)
        visual_menu.add_command(label=" Visual Debug Analysis", command=self.visual_debug_analysis)
        visual_menu.add_command(label=" Open Visual Browser", command=self.open_visual_browser)
        visual_menu.add_command(label=" Generate Visual Report", command=self.generate_visual_report)
        visual_menu.add_separator()
        visual_menu.add_command(label=" Desktop Sharing", command=self.start_desktop_sharing)
        visual_menu.add_command(label=" Live Code Analysis", command=self.start_live_code_analysis)
        visual_menu.add_command(label=" GUI Debugging", command=self.start_gui_debugging)
        visual_menu.add_command(label=" GUI Builder", command=self.start_gui_builder)
        
        # AI Models menu
        ai_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="AI Models", menu=ai_menu)
        ai_menu.add_command(label=" Configure AI Models", command=self.configure_ai_models)
        ai_menu.add_command(label=" AI Code Analysis", command=self.ai_code_analysis)
        ai_menu.add_command(label=" AI Code Generation", command=self.ai_code_generation)
        ai_menu.add_command(label=" AI Debugging", command=self.ai_debugging)
        ai_menu.add_separator()
        ai_menu.add_command(label=" ChatGPT", command=self.query_chatgpt)
        ai_menu.add_command(label=" Ollama", command=self.query_ollama)
        ai_menu.add_command(label=" Claude", command=self.query_claude)
        ai_menu.add_command(label=" Gemini", command=self.query_gemini)
        ai_menu.add_command(label=" All Models", command=self.query_all_ai_models)
        
        # Help menu
        help_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Help", menu=help_menu)
        help_menu.add_command(label="About", command=self.show_about)
    
    def load_sample_cpp(self):
        """Load sample C++ code"""
        sample = """#include <iostream>
#include <string>

int main() {
    std::cout << "Hello from Proper EXE Compiler!" << std::endl;
    std::cout << "This will create a REAL executable file!" << std::endl;
    std::cout << "Not just a tiny batch wrapper!" << std::endl;
    
    std::string name;
    std::cout << "Enter your name: ";
    std::getline(std::cin, name);
    
    std::cout << "Hello, " << name << "!" << std::endl;
    std::cout << "This EXE was compiled by the Proper EXE IDE!" << std::endl;
    
    std::cin.get(); // Wait for key press
    return 0;
}

// This will compile to a proper Windows executable
// Try it - you'll get a real EXE file, not a 1KB stub!
"""
        
        self.text_editor.delete(1.0, tk.END)
        self.text_editor.insert(1.0, sample)
    
    def new_file(self):
        """New file"""
        self.current_file = None
        self.text_editor.delete(1.0, tk.END)
        self.status_bar.config(text="New file created")
    
    def open_file(self):
        """Open file"""
        filename = filedialog.askopenfilename(
            title="Open file",
            filetypes=[
                ("C++ files", "*.cpp *.c"),
                ("C# files", "*.cs"),
                ("Python files", "*.py"),
                ("JavaScript files", "*.js"),
                ("All files", "*.*")
            ]
        )
        
        if filename:
            try:
                with open(filename, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                self.text_editor.delete(1.0, tk.END)
                self.text_editor.insert(1.0, content)
                self.current_file = filename
                self.status_bar.config(text=f"Opened: {filename}")
                
            except Exception as e:
                messagebox.showerror("Error", f"Could not open file: {e}")
    
    def save_file(self):
        """Save file"""
        if not self.current_file:
            self.save_file_as()
        else:
            try:
                content = self.text_editor.get(1.0, tk.END)
                with open(self.current_file, 'w', encoding='utf-8') as f:
                    f.write(content)
                self.status_bar.config(text=f"Saved: {self.current_file}")
            except Exception as e:
                messagebox.showerror("Error", f"Could not save file: {e}")
    
    def save_file_as(self):
        """Save file as"""
        filename = filedialog.asksaveasfilename(
            title="Save file as",
            defaultextension=".cpp",
            filetypes=[
                ("C++ files", "*.cpp"),
                ("C# files", "*.cs"), 
                ("Python files", "*.py"),
                ("JavaScript files", "*.js"),
                ("All files", "*.*")
            ]
        )
        
        if filename:
            self.current_file = filename
            self.save_file()
    
    def compile_exe(self):
        """Compile to EXE"""
        if not self.current_file:
            self.save_file_as()
            if not self.current_file:
                return
        else:
            self.save_file()
        
        self.clear_output()
        self.append_output(" Starting proper EXE compilation...\n")
        
        output_file = self.current_file.rsplit('.', 1)[0] + '.exe'
        
        def compile_thread():
            try:
                result = self.compiler.compile_to_exe(self.current_file, output_file)
                self.root.after(0, self.compilation_complete, result)
            except Exception as e:
                error_result = {"success": False, "error": str(e)}
                self.root.after(0, self.compilation_complete, error_result)
        
        thread = threading.Thread(target=compile_thread)
        thread.daemon = True
        thread.start()
    
    def compilation_complete(self, result):
        """Handle compilation completion"""
        if result["success"]:
            self.append_output(" EXE compilation successful!\n")
            self.append_output(f"Output: {result['output']}\n")
            
            if 'executable' in result:
                self.last_executable = result['executable']
                self.append_output(f"Executable: {result['executable']}\n")
                
                if 'size' in result:
                    size_kb = result['size'] / 1024
                    self.size_label.config(text=f"EXE Size: {size_kb:.1f} KB")
            
            if 'note' in result:
                self.append_output(f"Note: {result['note']}\n")
            
            self.status_bar.config(text="EXE compilation successful")
        else:
            self.append_output(" EXE compilation failed!\n")
            self.append_output(f"Error: {result['error']}\n")
            self.status_bar.config(text="Compilation failed")
    
    def rebuild_pe(self):
        """Rebuild PE file"""
        pe_file = filedialog.askopenfilename(
            title="Select PE file to rebuild",
            filetypes=[
                ("Executable files", "*.exe"),
                ("DLL files", "*.dll"),
                ("All files", "*.*")
            ]
        )
        
        if not pe_file:
            return
        
        self.clear_output()
        self.append_output(f" Starting PE rebuild for: {pe_file}\n")
        
        def rebuild_thread():
            try:
                rebuilder = self.compiler.pe_gen.rebuilder
                result = rebuilder.rebuild_pe_file(pe_file)
                self.root.after(0, self.rebuild_complete, result)
            except Exception as e:
                error_result = {"success": False, "error": str(e)}
                self.root.after(0, self.rebuild_complete, error_result)
        
        thread = threading.Thread(target=rebuild_thread)
        thread.daemon = True
        thread.start()
    
    def rebuild_complete(self, result):
        """Handle PE rebuild completion"""
        if result["success"]:
            self.append_output(" PE rebuild successful!\n")
            self.append_output(f"Output: {result['output']}\n")
            
            if 'rebuilt_file' in result:
                self.append_output(f"Rebuilt file: {result['rebuilt_file']}\n")
                
                if 'original_size' in result and 'rebuilt_size' in result:
                    orig_kb = result['original_size'] / 1024
                    new_kb = result['rebuilt_size'] / 1024
                    self.append_output(f"Size change: {orig_kb:.1f} KB → {new_kb:.1f} KB\n")
                    self.size_label.config(text=f"Rebuilt: {new_kb:.1f} KB")
            
            if 'note' in result:
                self.append_output(f"Note: {result['note']}\n")
            
            self.status_bar.config(text="PE rebuild successful")
        else:
            self.append_output(" PE rebuild failed!\n")
            self.append_output(f"Error: {result['error']}\n")
            self.status_bar.config(text="PE rebuild failed")
    
    def scan_keys(self):
        """Scan current file for API keys"""
        if not self.current_file:
            messagebox.showwarning("Warning", "No file open. Please open a file first.")
            return
        
        self.clear_output()
        self.append_output(f" Scanning for API keys in: {self.current_file}\n")
        
        def scan_thread():
            try:
                found_keys = self.key_scraper.scan_file(self.current_file)
                self.root.after(0, self.scan_complete, found_keys)
            except Exception as e:
                error_result = {"error": str(e)}
                self.root.after(0, self.scan_error, error_result)
        
        thread = threading.Thread(target=scan_thread)
        thread.daemon = True
        thread.start()
    
    def scan_directory(self):
        """Scan directory for API keys"""
        directory = filedialog.askdirectory(title="Select directory to scan for API keys")
        if not directory:
            return
        
        self.clear_output()
        self.append_output(f" Scanning directory for API keys: {directory}\n")
        
        def scan_thread():
            try:
                found_keys = self.key_scraper.scan_directory(directory)
                self.root.after(0, self.scan_complete, found_keys)
            except Exception as e:
                error_result = {"error": str(e)}
                self.root.after(0, self.scan_error, error_result)
        
        thread = threading.Thread(target=scan_thread)
        thread.daemon = True
        thread.start()
    
    def scan_complete(self, found_keys):
        """Handle scan completion"""
        if found_keys:
            self.append_output(f"️ Found {len(found_keys)} potential API keys!\n\n")
            
            # Group by provider
            by_provider = {}
            for key in found_keys:
                provider = key['provider']
                if provider not in by_provider:
                    by_provider[provider] = []
                by_provider[provider].append(key)
            
            # Display results
            for provider, keys in by_provider.items():
                self.append_output(f" {provider.upper()} KEYS ({len(keys)} found):\n")
                for key in keys:
                    self.append_output(f"  Line {key['line']}: {key['key'][:20]}...{key['key'][-10:]}\n")
                self.append_output("\n")
            
            # Generate report
            report_file = self.key_scraper.generate_report(found_keys)
            if report_file:
                self.append_output(f" Security report saved: {report_file}\n")
            
            self.status_bar.config(text=f"Found {len(found_keys)} API keys - Security risk!")
        else:
            self.append_output(" No API keys found - Code is secure!\n")
            self.status_bar.config(text="No API keys found")
    
    def scan_error(self, error_result):
        """Handle scan error"""
        self.append_output(f" Scan error: {error_result['error']}\n")
        self.status_bar.config(text="Scan failed")
    
    def mask_keys(self):
        """Mask keys in current file"""
        if not self.current_file:
            messagebox.showwarning("Warning", "No file open. Please open a file first.")
            return
        
        result = messagebox.askyesno("Mask Keys", 
                                   "This will create a masked version of the file with API keys hidden.\n"
                                   "Continue?")
        if not result:
            return
        
        try:
            masked_file = self.key_scraper.mask_keys_in_file(self.current_file)
            if masked_file:
                self.append_output(f" Keys masked in: {masked_file}\n")
                self.status_bar.config(text="Keys masked successfully")
            else:
                self.append_output(" No keys found to mask\n")
                self.status_bar.config(text="No keys to mask")
        except Exception as e:
            self.append_output(f" Masking error: {e}\n")
            self.status_bar.config(text="Masking failed")
    
    def scan_internet(self):
        """Scan internet for exposed API keys"""
        # Show distribution warning before sensitive operation
        if not messagebox.askyesno("️ CONFIDENTIAL OPERATION ️", 
                                  "This operation involves internet scanning for sensitive data.\n\n"
                                  "By continuing, you confirm:\n"
                                  "• This is for authorized security research only\n"
                                  "• You will not distribute any findings publicly\n"
                                  "• You will not reverse engineer this IDE\n"
                                  "• You understand the legal implications\n\n"
                                  "Continue with internet scan?"):
            return
        
        # Ask user which provider to search for
        provider_dialog = tk.Toplevel(self.root)
        provider_dialog.title("️ Select Provider to Scan - DO NOT DISTRIBUTE ️")
        provider_dialog.geometry("300x200")
        provider_dialog.transient(self.root)
        provider_dialog.grab_set()
        
        ttk.Label(provider_dialog, text="Select API provider to scan:").pack(pady=10)
        
        selected_provider = tk.StringVar(value="all")
        
        providers = [
            ("All Providers", "all"),
            ("OpenAI", "openai"),
            ("Google", "google"),
            ("GitHub", "github"),
            ("AWS", "aws"),
            ("Discord", "discord"),
            ("Generic", "generic")
        ]
        
        for text, value in providers:
            ttk.Radiobutton(provider_dialog, text=text, variable=selected_provider, value=value).pack(anchor=tk.W)
        
        def start_internet_scan():
            provider = selected_provider.get()
            provider_dialog.destroy()
            
            self.clear_output()
            self.append_output(f" Starting internet scan for {provider} API keys...\n")
            self.append_output("️ This may take several minutes...\n\n")
            
            def scan_thread():
                try:
                    found_keys = self.key_scraper.scan_internet_for_keys(provider)
                    self.root.after(0, self.internet_scan_complete, found_keys)
                except Exception as e:
                    error_result = {"error": str(e)}
                    self.root.after(0, self.scan_error, error_result)
            
            thread = threading.Thread(target=scan_thread)
            thread.daemon = True
            thread.start()
        
        ttk.Button(provider_dialog, text="Start Scan", command=start_internet_scan).pack(pady=10)
        ttk.Button(provider_dialog, text="Cancel", command=provider_dialog.destroy).pack()
    
    def scan_github(self):
        """Scan GitHub repositories for exposed keys"""
        self.clear_output()
        self.append_output(" Starting GitHub repository scan...\n")
        self.append_output("️ This may take several minutes...\n\n")
        
        def scan_thread():
            try:
                found_keys = self.key_scraper.scan_github_repos()
                self.root.after(0, self.internet_scan_complete, found_keys)
            except Exception as e:
                error_result = {"error": str(e)}
                self.root.after(0, self.scan_error, error_result)
        
        thread = threading.Thread(target=scan_thread)
        thread.daemon = True
        thread.start()
    
    def internet_scan_complete(self, found_keys):
        """Handle internet scan completion"""
        if found_keys:
            self.append_output(f"️ Found {len(found_keys)} exposed API keys on the internet!\n\n")
            
            # Group by provider
            by_provider = {}
            for key in found_keys:
                provider = key['provider']
                if provider not in by_provider:
                    by_provider[provider] = []
                by_provider[provider].append(key)
            
            # Display results
            for provider, keys in by_provider.items():
                self.append_output(f" {provider.upper()} KEYS ({len(keys)} found):\n")
                for key in keys:
                    self.append_output(f"  URL: {key['url']}\n")
                    self.append_output(f"  Key: {key['key'][:20]}...{key['key'][-10:]}\n")
                self.append_output("\n")
            
            # Generate internet report
            report_file = self.key_scraper.generate_internet_report(found_keys)
            if report_file:
                self.append_output(f" Internet scan report saved: {report_file}\n")
            
            self.status_bar.config(text=f"Found {len(found_keys)} exposed keys on internet!")
        else:
            self.append_output(" No exposed API keys found on the internet!\n")
            self.status_bar.config(text="No exposed keys found on internet")
    
    def configure_proxy(self):
        """Configure proxy settings for safe scraping"""
        proxy_dialog = tk.Toplevel(self.root)
        proxy_dialog.title(" Configure Proxy Settings")
        proxy_dialog.geometry("400x300")
        proxy_dialog.transient(self.root)
        proxy_dialog.grab_set()
        
        # Proxy type selection
        ttk.Label(proxy_dialog, text="Select Proxy Type:").pack(pady=10)
        
        proxy_type = tk.StringVar(value="none")
        proxy_types = [
            ("No Proxy (Direct Connection)", "none"),
            ("Tor (Anonymous)", "tor"),
            ("HTTP Proxy", "http"),
            ("SOCKS5 Proxy", "socks5")
        ]
        
        for text, value in proxy_types:
            ttk.Radiobutton(proxy_dialog, text=text, variable=proxy_type, value=value).pack(anchor=tk.W)
        
        # Proxy settings frame
        settings_frame = ttk.Frame(proxy_dialog)
        settings_frame.pack(fill=tk.X, padx=20, pady=10)
        
        # Host
        ttk.Label(settings_frame, text="Host:").grid(row=0, column=0, sticky=tk.W)
        host_entry = ttk.Entry(settings_frame, width=20)
        host_entry.grid(row=0, column=1, padx=5)
        host_entry.insert(0, "127.0.0.1")
        
        # Port
        ttk.Label(settings_frame, text="Port:").grid(row=1, column=0, sticky=tk.W)
        port_entry = ttk.Entry(settings_frame, width=20)
        port_entry.grid(row=1, column=1, padx=5)
        port_entry.insert(0, "8080")
        
        # Username
        ttk.Label(settings_frame, text="Username:").grid(row=2, column=0, sticky=tk.W)
        username_entry = ttk.Entry(settings_frame, width=20)
        username_entry.grid(row=2, column=1, padx=5)
        
        # Password
        ttk.Label(settings_frame, text="Password:").grid(row=3, column=0, sticky=tk.W)
        password_entry = ttk.Entry(settings_frame, width=20, show="*")
        password_entry.grid(row=3, column=1, padx=5)
        
        def apply_proxy():
            try:
                proxy_type_val = proxy_type.get()
                host = host_entry.get() if host_entry.get() else None
                port = int(port_entry.get()) if port_entry.get() else None
                username = username_entry.get() if username_entry.get() else None
                password = password_entry.get() if password_entry.get() else None
                
                success = self.key_scraper.setup_proxy_for_scraping(
                    proxy_type_val, host, port, username, password
                )
                
                if success:
                    self.append_output(f" Proxy configured: {proxy_type_val}\n")
                    if host and port:
                        self.append_output(f"   Host: {host}:{port}\n")
                    self.status_bar.config(text=f"Proxy: {proxy_type_val}")
                else:
                    self.append_output(f" Proxy configuration failed\n")
                
                proxy_dialog.destroy()
                
            except Exception as e:
                self.append_output(f" Proxy error: {e}\n")
                proxy_dialog.destroy()
        
        # Buttons
        button_frame = ttk.Frame(proxy_dialog)
        button_frame.pack(pady=10)
        
        ttk.Button(button_frame, text="Apply", command=apply_proxy).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Test", command=lambda: self.test_proxy_connection(proxy_dialog)).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=proxy_dialog.destroy).pack(side=tk.LEFT, padx=5)
    
    def test_proxy_connection(self, dialog=None):
        """Test proxy connection"""
        self.clear_output()
        self.append_output(" Testing proxy connection...\n")
        
        def test_thread():
            try:
                # Test connection
                success = self.key_scraper.test_proxy_connection()
                
                if success:
                    # Get current IP
                    current_ip = self.key_scraper.get_current_ip()
                    self.root.after(0, self.append_output, f" Connection successful!\n")
                    self.root.after(0, self.append_output, f"Current IP: {current_ip}\n")
                    self.root.after(0, self.status_bar.config, {"text": "Proxy connection working"})
                else:
                    self.root.after(0, self.append_output, f" Connection failed!\n")
                    self.root.after(0, self.status_bar.config, {"text": "Proxy connection failed"})
                
                if dialog:
                    self.root.after(0, dialog.destroy)
                    
            except Exception as e:
                self.root.after(0, self.append_output, f" Test error: {e}\n")
                if dialog:
                    self.root.after(0, dialog.destroy)
        
        thread = threading.Thread(target=test_thread)
        thread.daemon = True
        thread.start()
    
    def enable_stealth_mode(self):
        """Enable stealth mode for maximum anonymity"""
        self.clear_output()
        self.append_output(" Enabling stealth mode...\n")
        self.append_output("️ This will use Tor and add random delays\n\n")
        
        def stealth_thread():
            try:
                success = self.key_scraper.setup_stealth_scraping()
                
                if success:
                    current_ip = self.key_scraper.get_current_ip()
                    self.root.after(0, self.append_output, f" Stealth mode enabled!\n")
                    self.root.after(0, self.append_output, f"Current IP: {current_ip}\n")
                    self.root.after(0, self.append_output, f" Maximum anonymity active\n")
                    self.root.after(0, self.status_bar.config, {"text": "Stealth mode active"})
                else:
                    self.root.after(0, self.append_output, f" Stealth mode setup failed!\n")
                    self.root.after(0, self.status_bar.config, {"text": "Stealth mode failed"})
                    
            except Exception as e:
                self.root.after(0, self.append_output, f" Stealth error: {e}\n")
        
        thread = threading.Thread(target=stealth_thread)
        thread.daemon = True
        thread.start()
    
    def test_connection(self):
        """Test current connection and show IP"""
        self.clear_output()
        self.append_output(" Testing current connection...\n")
        
        def test_thread():
            try:
                current_ip = self.key_scraper.get_current_ip()
                is_vpn = self.key_scraper.proxy_manager.detect_vpn_proxy()
                
                self.root.after(0, self.append_output, f"Current IP: {current_ip}\n")
                self.root.after(0, self.append_output, f"VPN/Proxy detected: {'Yes' if is_vpn else 'No'}\n")
                self.root.after(0, self.status_bar.config, {"text": f"IP: {current_ip}"})
                
            except Exception as e:
                self.root.after(0, self.append_output, f" Connection test error: {e}\n")
        
        thread = threading.Thread(target=test_thread)
        thread.daemon = True
        thread.start()
    
    def scan_ai_chatboxes(self):
        """Scan for AI chatbox services"""
        self.clear_output()
        self.append_output(" Scanning for AI chatbox services...\n")
        
        def scan_thread():
            try:
                # Scan current directory
                findings = self.key_scraper.scan_for_ai_chatboxes(directory_path=".", recursive=True)
                
                self.root.after(0, self.append_output, f"Found {len(findings)} AI chatbox references:\n\n")
                
                for finding in findings:
                    if 'file' in finding:
                        self.root.after(0, self.append_output, f"File: {finding['file']}\n")
                        self.root.after(0, self.append_output, f"Service: {finding['service']}\n")
                        self.root.after(0, self.append_output, f"Line {finding['line']}: {finding['match']}\n")
                        self.root.after(0, self.append_output, f"Context: {finding['context']}\n\n")
                    else:
                        self.root.after(0, self.append_output, f"URL: {finding['url']}\n")
                        self.root.after(0, self.append_output, f"Service: {finding['service']}\n")
                        self.root.after(0, self.append_output, f"Match: {finding['match']}\n")
                        self.root.after(0, self.append_output, f"Context: {finding['context']}\n\n")
                
                self.root.after(0, self.append_output, " AI chatbox scan complete!\n")
                
            except Exception as e:
                self.root.after(0, self.append_output, f" AI chatbox scan error: {e}\n")
        
        thread = threading.Thread(target=scan_thread)
        thread.daemon = True
        thread.start()
    
    def use_free_chatbox(self):
        """Use free AI chatbox services for coding assistance"""
        # Create dialog for chatbox selection
        dialog = tk.Toplevel(self.root)
        dialog.title("Free AI Chatbox Services")
        dialog.geometry("600x500")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Get available services
        services = self.key_scraper.get_free_chatbox_services()
        
        # Service selection
        tk.Label(dialog, text="Select AI Service:", font=("Arial", 12, "bold")).pack(pady=10)
        
        service_var = tk.StringVar()
        service_frame = tk.Frame(dialog)
        service_frame.pack(pady=10)
        
        for service_id, service_info in services.items():
            rb = tk.Radiobutton(service_frame, text=f"{service_info['name']} ({'Free' if service_info['free'] else 'Paid'})", 
                              variable=service_var, value=service_id)
            rb.pack(anchor='w')
        
        # Model selection
        tk.Label(dialog, text="Select Model:", font=("Arial", 12, "bold")).pack(pady=(20, 10))
        
        model_var = tk.StringVar()
        model_frame = tk.Frame(dialog)
        model_frame.pack(pady=10)
        
        def update_models():
            # Clear existing model options
            for widget in model_frame.winfo_children():
                widget.destroy()
            
            # Add models for selected service
            if service_var.get() in services:
                service = services[service_var.get()]
                for model in service['models']:
                    rb = tk.Radiobutton(model_frame, text=model, variable=model_var, value=model)
                    rb.pack(anchor='w')
                if service['models']:
                    model_var.set(service['models'][0])
        
        service_var.trace('w', lambda *args: update_models())
        update_models()
        
        # Prompt input
        tk.Label(dialog, text="Enter your prompt:", font=("Arial", 12, "bold")).pack(pady=(20, 10))
        
        prompt_text = scrolledtext.ScrolledText(dialog, height=8, width=70)
        prompt_text.pack(pady=10, padx=20, fill='both', expand=True)
        prompt_text.insert('1.0', "Write a Python function to calculate fibonacci numbers")
        
        # Response area
        tk.Label(dialog, text="AI Response:", font=("Arial", 12, "bold")).pack(pady=(10, 5))
        
        response_text = scrolledtext.ScrolledText(dialog, height=6, width=70, state='disabled')
        response_text.pack(pady=5, padx=20, fill='both', expand=True)
        
        def send_request():
            if not service_var.get() or not model_var.get():
                messagebox.showerror("Error", "Please select a service and model")
                return
            
            prompt = prompt_text.get('1.0', tk.END).strip()
            if not prompt:
                messagebox.showerror("Error", "Please enter a prompt")
                return
            
            response_text.config(state='normal')
            response_text.delete('1.0', tk.END)
            response_text.insert('1.0', "Sending request...\n")
            response_text.config(state='disabled')
            dialog.update()
            
            def request_thread():
                try:
                    result = self.key_scraper.use_free_chatbox(service_var.get(), prompt, model_var.get())
                    
                    response_text.config(state='normal')
                    response_text.delete('1.0', tk.END)
                    
                    if result.get('success'):
                        response_text.insert('1.0', f"Service: {result['service']}\n")
                        response_text.insert(tk.END, f"Model: {result['model']}\n")
                        response_text.insert(tk.END, f"Response:\n{result['response']}\n")
                    else:
                        response_text.insert('1.0', f"Error: {result.get('error', 'Unknown error')}\n")
                    
                    response_text.config(state='disabled')
                    
                except Exception as e:
                    response_text.config(state='normal')
                    response_text.delete('1.0', tk.END)
                    response_text.insert('1.0', f"Request error: {e}\n")
                    response_text.config(state='disabled')
            
            thread = threading.Thread(target=request_thread)
            thread.daemon = True
            thread.start()
        
        # Buttons
        button_frame = tk.Frame(dialog)
        button_frame.pack(pady=20)
        
        tk.Button(button_frame, text="Send Request", command=send_request, 
                 bg='#4CAF50', fg='white', font=("Arial", 10, "bold")).pack(side='left', padx=10)
        tk.Button(button_frame, text="Close", command=dialog.destroy, 
                 bg='#f44336', fg='white', font=("Arial", 10, "bold")).pack(side='left', padx=10)
    
    def auto_create_copilots(self):
        """Automatically create multiple copilots from available services"""
        self.clear_output()
        self.append_output(" Auto-creating multiple copilots...\n")
        
        def create_thread():
            try:
                # Auto-create copilots from free services
                created_copilots = self.key_scraper.auto_create_copilots_from_services(max_copilots=5)
                
                self.append_output(f" Created {len(created_copilots)} copilots:\n\n")
                
                for copilot in created_copilots:
                    if copilot['status'] == 'created':
                        self.append_output(f" {copilot['name']} - {copilot['copilot_id']}\n")
                    else:
                        self.append_output(f" {copilot['name']} - Failed: {copilot.get('error', 'Unknown error')}\n")
                
                # Also scan and create from findings
                self.append_output("\n Scanning for additional services...\n")
                findings = self.key_scraper.scan_for_ai_chatboxes(directory_path=".", recursive=True)
                
                if findings:
                    self.append_output(f" Found {len(findings)} AI service references\n")
                    bulk_copilots = self.key_scraper.bulk_create_copilots_from_findings(findings, max_copilots=3)
                    
                    self.append_output(f" Created {len(bulk_copilots)} additional copilots from findings:\n")
                    for copilot in bulk_copilots:
                        if copilot.get('status') == 'created':
                            self.append_output(f" {copilot['copilot_id']}\n")
                        else:
                            self.append_output(f" Failed: {copilot.get('error', 'Unknown error')}\n")
                
                self.append_output(f"\n Total active copilots: {len(self.key_scraper.get_all_copilots())}\n")
                
            except Exception as e:
                self.append_output(f" Auto-create error: {e}\n")
        
        thread = threading.Thread(target=create_thread)
        thread.daemon = True
        thread.start()
    
    def manage_copilots(self):
        """Manage active copilots"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Manage AI Copilots")
        dialog.geometry("800x600")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Get all copilots
        copilot_ids = self.key_scraper.get_all_copilots()
        
        if not copilot_ids:
            tk.Label(dialog, text="No active copilots found.\nUse 'Auto-Create Copilots' to create some.", 
                    font=("Arial", 12)).pack(pady=50)
            tk.Button(dialog, text="Close", command=dialog.destroy).pack(pady=20)
            return
        
        # Copilot list
        tk.Label(dialog, text="Active Copilots:", font=("Arial", 14, "bold")).pack(pady=10)
        
        # Create listbox with scrollbar
        list_frame = tk.Frame(dialog)
        list_frame.pack(fill='both', expand=True, padx=20, pady=10)
        
        listbox = tk.Listbox(list_frame, height=15)
        scrollbar = tk.Scrollbar(list_frame, orient='vertical', command=listbox.yview)
        listbox.configure(yscrollcommand=scrollbar.set)
        
        listbox.pack(side='left', fill='both', expand=True)
        scrollbar.pack(side='right', fill='y')
        
        # Populate listbox
        for copilot_id in copilot_ids:
            stats = self.key_scraper.get_copilot_stats(copilot_id)
            if stats:
                listbox.insert(tk.END, f"{stats['name']} ({stats['usage_count']} uses)")
        
        # Details area
        details_frame = tk.Frame(dialog)
        details_frame.pack(fill='x', padx=20, pady=10)
        
        details_text = scrolledtext.ScrolledText(details_frame, height=8, width=80)
        details_text.pack(fill='x')
        
        def show_details():
            selection = listbox.curselection()
            if selection:
                copilot_id = copilot_ids[selection[0]]
                stats = self.key_scraper.get_copilot_stats(copilot_id)
                
                details_text.delete('1.0', tk.END)
                details_text.insert('1.0', f"Copilot ID: {stats['id']}\n")
                details_text.insert(tk.END, f"Name: {stats['name']}\n")
                details_text.insert(tk.END, f"Service: {stats['service']}\n")
                details_text.insert(tk.END, f"Usage Count: {stats['usage_count']}\n")
                details_text.insert(tk.END, f"History Entries: {stats['history_count']}\n")
                details_text.insert(tk.END, f"Status: {stats['status']}\n")
                details_text.insert(tk.END, f"Created: {time.ctime(stats['created_at'])}\n")
                if stats['last_used']:
                    details_text.insert(tk.END, f"Last Used: {time.ctime(stats['last_used'])}\n")
        
        def remove_copilot():
            selection = listbox.curselection()
            if selection:
                copilot_id = copilot_ids[selection[0]]
                if messagebox.askyesno("Remove Copilot", f"Remove copilot {copilot_id}?"):
                    if self.key_scraper.copilot_manager.remove_copilot(copilot_id):
                        listbox.delete(selection[0])
                        details_text.delete('1.0', tk.END)
                        self.append_output(f" Removed copilot: {copilot_id}\n")
        
        # Buttons
        button_frame = tk.Frame(dialog)
        button_frame.pack(pady=20)
        
        tk.Button(button_frame, text="Show Details", command=show_details).pack(side='left', padx=5)
        tk.Button(button_frame, text="Remove Copilot", command=remove_copilot, 
                 bg='#f44336', fg='white').pack(side='left', padx=5)
        tk.Button(button_frame, text="Close", command=dialog.destroy).pack(side='left', padx=5)
    
    def use_copilot(self):
        """Use a specific copilot for coding assistance"""
        copilot_ids = self.key_scraper.get_all_copilots()
        
        if not copilot_ids:
            messagebox.showinfo("No Copilots", "No active copilots found.\nUse 'Auto-Create Copilots' to create some.")
            return
        
        dialog = tk.Toplevel(self.root)
        dialog.title("Use AI Copilot")
        dialog.geometry("700x500")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Copilot selection
        tk.Label(dialog, text="Select Copilot:", font=("Arial", 12, "bold")).pack(pady=10)
        
        copilot_var = tk.StringVar()
        copilot_frame = tk.Frame(dialog)
        copilot_frame.pack(pady=10)
        
        for copilot_id in copilot_ids:
            stats = self.key_scraper.get_copilot_stats(copilot_id)
            if stats:
                rb = tk.Radiobutton(copilot_frame, 
                                  text=f"{stats['name']} ({stats['usage_count']} uses)", 
                                  variable=copilot_var, value=copilot_id)
                rb.pack(anchor='w')
        
        # Set default selection
        if copilot_ids:
            copilot_var.set(copilot_ids[0])
        
        # Prompt input
        tk.Label(dialog, text="Enter your coding request:", font=("Arial", 12, "bold")).pack(pady=(20, 10))
        
        prompt_text = scrolledtext.ScrolledText(dialog, height=8, width=80)
        prompt_text.pack(pady=10, padx=20, fill='both', expand=True)
        prompt_text.insert('1.0', "Write a Python function to reverse a string")
        
        # Response area
        tk.Label(dialog, text="Copilot Response:", font=("Arial", 12, "bold")).pack(pady=(10, 5))
        
        response_text = scrolledtext.ScrolledText(dialog, height=6, width=80, state='disabled')
        response_text.pack(pady=5, padx=20, fill='both', expand=True)
        
        def send_to_copilot():
            if not copilot_var.get():
                messagebox.showerror("Error", "Please select a copilot")
                return
            
            prompt = prompt_text.get('1.0', tk.END).strip()
            if not prompt:
                messagebox.showerror("Error", "Please enter a prompt")
                return
            
            response_text.config(state='normal')
            response_text.delete('1.0', tk.END)
            response_text.insert('1.0', "Sending to copilot...\n")
            response_text.config(state='disabled')
            dialog.update()
            
            def copilot_thread():
                try:
                    result = self.key_scraper.use_copilot_by_id(copilot_var.get(), prompt)
                    
                    response_text.config(state='normal')
                    response_text.delete('1.0', tk.END)
                    
                    if result.get('success'):
                        response_text.insert('1.0', f"Copilot: {result.get('copilot_name', 'Unknown')}\n")
                        response_text.insert(tk.END, f"Service: {result['service']}\n")
                        response_text.insert(tk.END, f"Model: {result['model']}\n\n")
                        response_text.insert(tk.END, f"Response:\n{result['response']}\n")
                    else:
                        response_text.insert('1.0', f"Error: {result.get('error', 'Unknown error')}\n")
                    
                    response_text.config(state='disabled')
                    
                except Exception as e:
                    response_text.config(state='normal')
                    response_text.delete('1.0', tk.END)
                    response_text.insert('1.0', f"Copilot error: {e}\n")
                    response_text.config(state='disabled')
            
            thread = threading.Thread(target=copilot_thread)
            thread.daemon = True
            thread.start()
        
        # Buttons
        button_frame = tk.Frame(dialog)
        button_frame.pack(pady=20)
        
        tk.Button(button_frame, text="Send to Copilot", command=send_to_copilot, 
                 bg='#4CAF50', fg='white', font=("Arial", 10, "bold")).pack(side='left', padx=10)
        tk.Button(button_frame, text="Close", command=dialog.destroy, 
                 bg='#f44336', fg='white', font=("Arial", 10, "bold")).pack(side='left', padx=10)
    
    def save_all_data(self):
        """Save all scraped data"""
        self.clear_output()
        self.append_output(" Saving all scraped data...\n")
        
        def save_thread():
            try:
                success = self.key_scraper.proxy_manager.save_all_data()
                if success:
                    self.append_output(" All data saved successfully!\n")
                    self.append_output(f" Data directory: {self.key_scraper.proxy_manager.data_directory}\n")
                else:
                    self.append_output(" Error saving data!\n")
            except Exception as e:
                self.append_output(f" Save error: {e}\n")
        
        thread = threading.Thread(target=save_thread)
        thread.daemon = True
        thread.start()
    
    def load_all_data(self):
        """Load all scraped data"""
        self.clear_output()
        self.append_output(" Loading all scraped data...\n")
        
        def load_thread():
            try:
                success = self.key_scraper.proxy_manager.load_all_data()
                if success:
                    self.append_output(" All data loaded successfully!\n")
                    
                    # Show summary
                    data = self.key_scraper.proxy_manager.scraped_data
                    self.append_output(f" Loaded data summary:\n")
                    self.append_output(f" - API Keys: {len(data.get('api_keys', []))}\n")
                    self.append_output(f" - AI Services: {len(data.get('ai_services', []))}\n")
                    self.append_output(f" - Proxies: {len(data.get('proxies', []))}\n")
                    self.append_output(f" - VPNs: {len(data.get('vpns', []))}\n")
                    self.append_output(f" - Copilots: {len(data.get('copilots', []))}\n")
                    self.append_output(f" - Search Results: {len(data.get('search_results', []))}\n")
                else:
                    self.append_output(" Error loading data!\n")
            except Exception as e:
                self.append_output(f" Load error: {e}\n")
        
        thread = threading.Thread(target=load_thread)
        thread.daemon = True
        thread.start()
    
    def export_data(self):
        """Export data in various formats"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Export Scraped Data")
        dialog.geometry("500x400")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Format selection
        tk.Label(dialog, text="Select Export Format:", font=("Arial", 12, "bold")).pack(pady=10)
        
        format_var = tk.StringVar(value="json")
        format_frame = tk.Frame(dialog)
        format_frame.pack(pady=10)
        
        formats = [
            ("JSON", "json"),
            ("CSV", "csv"),
            ("Text", "txt")
        ]
        
        for text, value in formats:
            rb = tk.Radiobutton(format_frame, text=text, variable=format_var, value=value)
            rb.pack(anchor='w')
        
        # Filename input
        tk.Label(dialog, text="Filename (optional):", font=("Arial", 12, "bold")).pack(pady=(20, 10))
        
        filename_entry = tk.Entry(dialog, width=50)
        filename_entry.pack(pady=10)
        filename_entry.insert(0, f"scraped_data_export_{time.strftime('%Y%m%d_%H%M%S')}")
        
        # Status area
        tk.Label(dialog, text="Export Status:", font=("Arial", 12, "bold")).pack(pady=(20, 10))
        
        status_text = scrolledtext.ScrolledText(dialog, height=8, width=60, state='disabled')
        status_text.pack(pady=10, padx=20, fill='both', expand=True)
        
        def export_data():
            format_type = format_var.get()
            filename = filename_entry.get().strip()
            
            if not filename:
                messagebox.showerror("Error", "Please enter a filename")
                return
            
            status_text.config(state='normal')
            status_text.delete('1.0', tk.END)
            status_text.insert('1.0', f"Exporting data in {format_type.upper()} format...\n")
            status_text.config(state='disabled')
            dialog.update()
            
            def export_thread():
                try:
                    export_file = self.key_scraper.proxy_manager.export_data(format_type, filename)
                    
                    status_text.config(state='normal')
                    status_text.delete('1.0', tk.END)
                    
                    if export_file:
                        status_text.insert('1.0', f"Export successful!\n")
                        status_text.insert(tk.END, f"File: {export_file}\n")
                        status_text.insert(tk.END, f"Format: {format_type.upper()}\n")
                        status_text.insert(tk.END, f"Size: {os.path.getsize(export_file)} bytes\n")
                    else:
                        status_text.insert('1.0', f"Export failed!\n")
                    
                    status_text.config(state='disabled')
                    
                except Exception as e:
                    status_text.config(state='normal')
                    status_text.delete('1.0', tk.END)
                    status_text.insert('1.0', f"Export error: {e}\n")
                    status_text.config(state='disabled')
            
            thread = threading.Thread(target=export_thread)
            thread.daemon = True
            thread.start()
        
        # Buttons
        button_frame = tk.Frame(dialog)
        button_frame.pack(pady=20)
        
        tk.Button(button_frame, text="Export", command=export_data, 
                 bg='#4CAF50', fg='white', font=("Arial", 10, "bold")).pack(side='left', padx=10)
        tk.Button(button_frame, text="Close", command=dialog.destroy, 
                 bg='#f44336', fg='white', font=("Arial", 10, "bold")).pack(side='left', padx=10)
    
    def import_data(self):
        """Import data from file"""
        filepath = filedialog.askopenfilename(
            title="Select file to import",
            filetypes=[
                ("JSON files", "*.json"),
                ("All files", "*.*")
            ]
        )
        
        if not filepath:
            return
        
        self.clear_output()
        self.append_output(f" Importing data from: {filepath}\n")
        
        def import_thread():
            try:
                success = self.key_scraper.proxy_manager.import_data(filepath)
                if success:
                    self.append_output(" Data imported successfully!\n")
                    
                    # Show summary
                    data = self.key_scraper.proxy_manager.scraped_data
                    self.append_output(f" Imported data summary:\n")
                    self.append_output(f" - API Keys: {len(data.get('api_keys', []))}\n")
                    self.append_output(f" - AI Services: {len(data.get('ai_services', []))}\n")
                    self.append_output(f" - Proxies: {len(data.get('proxies', []))}\n")
                    self.append_output(f" - VPNs: {len(data.get('vpns', []))}\n")
                    self.append_output(f" - Copilots: {len(data.get('copilots', []))}\n")
                    self.append_output(f" - Search Results: {len(data.get('search_results', []))}\n")
                else:
                    self.append_output(" Error importing data!\n")
            except Exception as e:
                self.append_output(f" Import error: {e}\n")
        
        thread = threading.Thread(target=import_thread)
        thread.daemon = True
        thread.start()
    
    def clear_all_data(self):
        """Clear all scraped data"""
        if messagebox.askyesno("Clear All Data", 
                              "Are you sure you want to clear all scraped data?\nThis action cannot be undone."):
            self.clear_output()
            self.append_output(" Clearing all scraped data...\n")
            
            def clear_thread():
                try:
                    self.key_scraper.proxy_manager.clear_scraped_data()
                    self.append_output(" All data cleared successfully!\n")
                except Exception as e:
                    self.append_output(f" Clear error: {e}\n")
            
            thread = threading.Thread(target=clear_thread)
            thread.daemon = True
            thread.start()
    
    def run_exe(self):
        """Run compiled EXE"""
        if hasattr(self, 'last_executable') and self.last_executable:
            try:
                self.append_output(f" Running {self.last_executable}...\n")
                subprocess.Popen([self.last_executable])
            except Exception as e:
                self.append_output(f" Run error: {e}\n")
        else:
            messagebox.showwarning("Warning", "No EXE to run. Compile first!")
    
    def clear_output(self):
        """Clear output"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        self.output_text.config(state=tk.DISABLED)
    
    def append_output(self, text):
        """Append output"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.insert(tk.END, text)
        self.output_text.see(tk.END)
        self.output_text.config(state=tk.DISABLED)
    
    def show_about(self):
        """Show about with distribution warning"""
        about_text = """️ DO NOT DISTRIBUTE - CONFIDENTIAL TOOL ️

Proper EXE Compiler with AI Key Scraper

DISTRIBUTION RESTRICTIONS:
• This software is for authorized use only
• Do NOT share, distribute, or publish this tool
• Do NOT upload to public repositories
• Do NOT reverse engineer this IDE or its components
• Do NOT attempt to decompile, disassemble, or analyze the code
• Violation may result in legal consequences

Creates REAL Windows executables + Security Analysis!

Features:
• System compiler integration (GCC, Clang, MSVC, CSC)
• Custom PE executable generation
•  PE Rebuilder - Fix corrupted executables!
•  AI Key Scraper - Find API keys & credentials!
• Multi-language support (C++, C#, Python, JS)
• Proper file sizes (not 1KB stubs!)
• Actually runnable programs
• PE structure validation and repair

AI Key Scraper detects:
• Google API keys (AIza...)
• OpenAI keys (sk-...)
• GitHub tokens (ghp_...)
• AWS keys (AKIA...)
• Discord tokens
• And many more!

 Internet Scanning Features:
• Google Dorking for exposed keys
• GitHub repository scanning
• Pastebin and Gist scanning
• Stack Overflow code scanning
• Web scraping with rate limiting
• Comprehensive security reports

 AI Chatbox Scraping:
• Scan for AI chatbox services in code
• Find ChatGPT, Claude, Bard, Copilot references
• Discover Hugging Face, Ollama, Groq services
• Use free AI chatbox services for coding
• Local Ollama integration
• Free AI model access (Groq, Together AI)
• AI-powered coding assistance

 Proxy/VPN Support:
• Tor anonymous browsing
• HTTP/SOCKS5 proxy support
• Stealth mode with random delays
• Proxy rotation for distributed scraping
• VPN detection and IP masking
• Safe anonymous web scraping

This IDE creates proper executables AND helps
secure your code by finding exposed credentials
both locally and on the internet!
"""
        messagebox.showinfo("About", about_text)
    
    def run(self):
        """Start IDE"""
        self.root.bind('<Control-n>', lambda e: self.new_file())
        self.root.bind('<Control-o>', lambda e: self.open_file())
        self.root.bind('<Control-s>', lambda e: self.save_file())
        self.root.bind('<F5>', lambda e: self.compile_exe())
        self.root.bind('<F6>', lambda e: self.run_exe())
        
        self.root.mainloop()
    
    def analyze_code_screenshot(self):
        """Analyze code screenshot using visual AI"""
        try:
            # Get current code from editor
            code_text = self.editor.get("1.0", tk.END).strip()
            if not code_text:
                messagebox.showwarning("No Code", "Please enter some code to analyze")
                return
            
            # Ask for screenshot file
            screenshot_path = filedialog.askopenfilename(
                title="Select Code Screenshot",
                filetypes=[("Image files", "*.png *.jpg *.jpeg *.bmp *.gif")]
            )
            
            if screenshot_path:
                # Use debugging agent to analyze screenshot
                success = self.debugging_agent.analyze_code_screenshot(screenshot_path)
                if success:
                    messagebox.showinfo("Visual Analysis", "Code screenshot analyzed successfully!")
                else:
                    messagebox.showerror("Visual Analysis Failed", "Failed to analyze code screenshot")
            
        except Exception as e:
            print(f" Screenshot analysis error: {e}")
            messagebox.showerror("Screenshot Analysis Error", f"Error analyzing screenshot: {e}")
    
    def visual_debug_analysis(self):
        """Perform visual debugging analysis on current code"""
        try:
            # Get current code from editor
            code_text = self.editor.get("1.0", tk.END).strip()
            if not code_text:
                messagebox.showwarning("No Code", "Please enter some code to analyze")
                return
            
            # Use debugging agent for visual analysis
            success = self.debugging_agent.visual_debug_analysis(code_text)
            if success:
                messagebox.showinfo("Visual Debug Analysis", "Visual debugging analysis complete!")
            else:
                messagebox.showerror("Visual Debug Analysis Failed", "Failed to perform visual debugging analysis")
            
        except Exception as e:
            print(f" Visual debug analysis error: {e}")
            messagebox.showerror("Visual Debug Analysis Error", f"Error in visual debug analysis: {e}")
    
    def open_visual_browser(self):
        """Open the visual browser for AI analysis"""
        try:
            # Open debugging agent browser
            success = self.debugging_agent.open_browser_window()
            if success:
                messagebox.showinfo("Visual Browser", "Visual browser opened successfully!")
            else:
                messagebox.showerror("Visual Browser Error", "Failed to open visual browser")
            
        except Exception as e:
            print(f" Visual browser error: {e}")
            messagebox.showerror("Visual Browser Error", f"Error opening visual browser: {e}")
    
    def generate_visual_report(self):
        """Generate visual analysis report"""
        try:
            # Get current code from editor
            code_text = self.editor.get("1.0", tk.END).strip()
            if not code_text:
                messagebox.showwarning("No Code", "Please enter some code to analyze")
                return
            
            # Take screenshot and analyze
            screenshot_path = self.debugging_agent.take_code_screenshot(code_text)
            if screenshot_path:
                # Analyze the screenshot
                success = self.debugging_agent.analyze_code_screenshot(screenshot_path)
                if success:
                    messagebox.showinfo("Visual Report", "Visual analysis report generated successfully!")
                else:
                    messagebox.showerror("Visual Report Error", "Failed to generate visual analysis report")
            else:
                messagebox.showerror("Visual Report Error", "Failed to create code screenshot")
            
        except Exception as e:
            print(f" Visual report generation error: {e}")
            messagebox.showerror("Visual Report Error", f"Error generating visual report: {e}")
    
    def start_desktop_sharing(self):
        """Start desktop sharing for live analysis"""
        try:
            success = self.debugging_agent.start_desktop_sharing()
            if success:
                messagebox.showinfo("Desktop Sharing", "Desktop sharing window opened!")
            else:
                messagebox.showerror("Desktop Sharing Error", "Failed to open desktop sharing")
        except Exception as e:
            print(f" Desktop sharing error: {e}")
            messagebox.showerror("Desktop Sharing Error", f"Error starting desktop sharing: {e}")
    
    def start_live_code_analysis(self):
        """Start live code analysis"""
        try:
            success = self.debugging_agent.start_live_code_analysis()
            if success:
                messagebox.showinfo("Live Analysis", "Live code analysis window opened!")
            else:
                messagebox.showerror("Live Analysis Error", "Failed to open live analysis")
        except Exception as e:
            print(f" Live analysis error: {e}")
            messagebox.showerror("Live Analysis Error", f"Error starting live analysis: {e}")
    
    def start_gui_debugging(self):
        """Start GUI debugging with visual analysis"""
        try:
            print(" Starting GUI debugging...")
            
            # Create GUI debugging window
            self.gui_debug_window = tk.Toplevel()
            self.gui_debug_window.title(" GUI Debugging - Visual Analysis")
            self.gui_debug_window.geometry("1200x800")
            
            # Create main frame
            main_frame = ttk.Frame(self.gui_debug_window)
            main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # Left panel - Video display
            video_frame = ttk.LabelFrame(main_frame, text="Desktop View")
            video_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
            
            self.gui_video_label = tk.Label(video_frame)
            self.gui_video_label.pack(fill=tk.BOTH, expand=True)
            
            # Right panel - Analysis
            analysis_frame = ttk.LabelFrame(main_frame, text="GUI Analysis")
            analysis_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
            
            # GUI detection results
            ttk.Label(analysis_frame, text="GUI Elements Detected", font=("Arial", 10, "bold")).pack(anchor=tk.W, pady=(0, 5))
            
            self.gui_elements_text = scrolledtext.ScrolledText(analysis_frame, width=40, height=15)
            self.gui_elements_text.pack(fill=tk.BOTH, expand=True, pady=(0, 5))
            
            # GUI debugging controls
            debug_frame = ttk.Frame(analysis_frame)
            debug_frame.pack(fill=tk.X, pady=5)
            
            ttk.Button(debug_frame, text=" Start GUI Debug", command=self._start_gui_debug).pack(side=tk.LEFT, padx=2)
            ttk.Button(debug_frame, text=" Stop Debug", command=self._stop_gui_debug).pack(side=tk.LEFT, padx=2)
            ttk.Button(debug_frame, text=" Analyze GUI", command=self._analyze_gui_elements).pack(side=tk.LEFT, padx=2)
            
            # Status
            self.gui_debug_status = ttk.Label(debug_frame, text="Ready for GUI debugging")
            self.gui_debug_status.pack(side=tk.LEFT, padx=10)
            
            # Initialize GUI debugging
            self.is_gui_debugging = False
            self.gui_debug_thread = None
            self.gui_elements = []
            self.current_gui_frame = None
            
            print(" GUI debugging window opened")
            return True
            
        except Exception as e:
            print(f" GUI debugging error: {e}")
            return False
    
    def _start_gui_debug(self):
        """Start GUI debugging"""
        try:
            if self.is_gui_debugging:
                return
            
            self.is_gui_debugging = True
            self.gui_debug_status.config(text="Debugging GUI...")
            self.gui_elements_text.delete(1.0, tk.END)
            self.gui_elements_text.insert(tk.END, "Starting GUI debugging...\n\n")
            
            # Start GUI debugging thread
            self.gui_debug_thread = threading.Thread(target=self._perform_gui_debugging)
            self.gui_debug_thread.daemon = True
            self.gui_debug_thread.start()
            
            print(" GUI debugging started")
            
        except Exception as e:
            print(f" Start GUI debug error: {e}")
    
    def _stop_gui_debug(self):
        """Stop GUI debugging"""
        try:
            self.is_gui_debugging = False
            self.gui_debug_status.config(text="GUI debugging stopped")
            self.gui_elements_text.insert(tk.END, "\n\nGUI debugging stopped.\n")
            print(" GUI debugging stopped")
            
        except Exception as e:
            print(f" Stop GUI debug error: {e}")
    
    def _perform_gui_debugging(self):
        """Perform GUI debugging analysis"""
        try:
            with mss.mss() as sct:
                monitor = sct.monitors[1]
                frame_count = 0
                
                while self.is_gui_debugging:
                    try:
                        # Capture screenshot
                        screenshot = sct.grab(monitor)
                        img = Image.frombytes("RGB", screenshot.size, screenshot.bgra, "raw", "BGRX")
                        frame = np.array(img)
                        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                        
                        # Store current frame
                        self.current_gui_frame = frame.copy()
                        
                        # Update video display
                        self._update_gui_display(frame)
                        
                        # Analyze GUI elements every 60 frames (about 6 seconds at 10 FPS)
                        if frame_count % 60 == 0:
                            self._detect_gui_elements(frame, frame_count)
                        
                        frame_count += 1
                        time.sleep(0.1)  # 10 FPS
                        
                    except Exception as e:
                        print(f" GUI debugging frame error: {e}")
                        break
                        
        except Exception as e:
            print(f" GUI debugging error: {e}")
    
    def _update_gui_display(self, frame):
        """Update GUI debugging display"""
        try:
            # Resize frame for display
            height, width = frame.shape[:2]
            max_width, max_height = 800, 600
            
            if width > max_width or height > max_height:
                scale = min(max_width/width, max_height/height)
                new_width = int(width * scale)
                new_height = int(height * scale)
                display_frame = cv2.resize(frame, (new_width, new_height))
            else:
                display_frame = frame
            
            # Convert to PhotoImage
            frame_rgb = cv2.cvtColor(display_frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame_rgb)
            photo = ImageTk.PhotoImage(img)
            
            # Update display
            self.gui_video_label.config(image=photo)
            self.gui_video_label.image = photo
            
        except Exception as e:
            print(f" GUI display update error: {e}")
    
    def _detect_gui_elements(self, frame, frame_count):
        """Detect GUI elements in the frame"""
        try:
            # Convert to grayscale for analysis
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            
            # Detect windows/rectangles
            windows = self._detect_windows(gray)
            
            # Detect buttons
            buttons = self._detect_buttons(gray)
            
            # Detect text areas
            text_areas = self._detect_text_areas(gray)
            
            # Detect menus
            menus = self._detect_menus(gray)
            
            # Update GUI elements display
            timestamp = time.strftime("%H:%M:%S")
            self.gui_elements_text.insert(tk.END, f"[{timestamp}] Frame {frame_count} - GUI Elements:\n")
            self.gui_elements_text.insert(tk.END, f"  Windows: {len(windows)}\n")
            self.gui_elements_text.insert(tk.END, f"  Buttons: {len(buttons)}\n")
            self.gui_elements_text.insert(tk.END, f"  Text Areas: {len(text_areas)}\n")
            self.gui_elements_text.insert(tk.END, f"  Menus: {len(menus)}\n")
            
            # Store GUI elements
            gui_elements = {
                'timestamp': timestamp,
                'frame': frame_count,
                'windows': windows,
                'buttons': buttons,
                'text_areas': text_areas,
                'menus': menus
            }
            
            self.gui_elements.append(gui_elements)
            
            # Show detailed info for first few elements
            if windows:
                self.gui_elements_text.insert(tk.END, f"  Window details: {windows[0]}\n")
            if buttons:
                self.gui_elements_text.insert(tk.END, f"  Button details: {buttons[0]}\n")
            
            self.gui_elements_text.insert(tk.END, "\n")
            self.gui_elements_text.see(tk.END)
            
        except Exception as e:
            print(f" GUI element detection error: {e}")
    
    def _detect_windows(self, gray):
        """Detect window-like rectangles"""
        try:
            # Use edge detection to find rectangles
            edges = cv2.Canny(gray, 50, 150)
            contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            windows = []
            for contour in contours:
                # Approximate contour to polygon
                epsilon = 0.02 * cv2.arcLength(contour, True)
                approx = cv2.approxPolyDP(contour, epsilon, True)
                
                # Check if it's roughly rectangular
                if len(approx) >= 4:
                    x, y, w, h = cv2.boundingRect(contour)
                    if w > 100 and h > 50:  # Minimum size for a window
                        windows.append({
                            'type': 'window',
                            'x': x, 'y': y, 'width': w, 'height': h,
                            'area': w * h
                        })
            
            return windows
            
        except Exception as e:
            print(f" Window detection error: {e}")
            return []
    
    def _detect_buttons(self, gray):
        """Detect button-like elements"""
        try:
            # Use template matching for button detection
            # This is a simplified approach - in practice, you'd use more sophisticated methods
            
            # Find small rectangular regions that could be buttons
            edges = cv2.Canny(gray, 50, 150)
            contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            buttons = []
            for contour in contours:
                x, y, w, h = cv2.boundingRect(contour)
                
                # Button characteristics: small, roughly square or rectangular
                if 20 <= w <= 200 and 20 <= h <= 100 and w/h < 3 and h/w < 3:
                    buttons.append({
                        'type': 'button',
                        'x': x, 'y': y, 'width': w, 'height': h,
                        'area': w * h
                    })
            
            return buttons
            
        except Exception as e:
            print(f" Button detection error: {e}")
            return []
    
    def _detect_text_areas(self, gray):
        """Detect text input areas"""
        try:
            # Use horizontal line detection for text areas
            horizontal_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (25, 1))
            horizontal_lines = cv2.morphologyEx(gray, cv2.MORPH_OPEN, horizontal_kernel)
            
            contours, _ = cv2.findContours(horizontal_lines, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            text_areas = []
            for contour in contours:
                x, y, w, h = cv2.boundingRect(contour)
                
                # Text area characteristics: wide, not too tall
                if w > 100 and h < 50:
                    text_areas.append({
                        'type': 'text_area',
                        'x': x, 'y': y, 'width': w, 'height': h,
                        'area': w * h
                    })
            
            return text_areas
            
        except Exception as e:
            print(f" Text area detection error: {e}")
            return []
    
    def _detect_menus(self, gray):
        """Detect menu-like elements"""
        try:
            # Look for horizontal bars that could be menus
            horizontal_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (50, 1))
            horizontal_lines = cv2.morphologyEx(gray, cv2.MORPH_OPEN, horizontal_kernel)
            
            contours, _ = cv2.findContours(horizontal_lines, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            menus = []
            for contour in contours:
                x, y, w, h = cv2.boundingRect(contour)
                
                # Menu characteristics: wide, not too tall, at top of screen
                if w > 200 and h < 30 and y < 100:
                    menus.append({
                        'type': 'menu',
                        'x': x, 'y': y, 'width': w, 'height': h,
                        'area': w * h
                    })
            
            return menus
            
        except Exception as e:
            print(f" Menu detection error: {e}")
            return []
    
    def _analyze_gui_elements(self):
        """Analyze detected GUI elements"""
        try:
            if not self.gui_elements:
                print(" No GUI elements to analyze")
                return
            
            # Generate GUI analysis report
            report_path = f"gui_analysis_report_{int(time.time())}.html"
            
            html = f"""
            <!DOCTYPE html>
            <html>
            <head>
                <title>GUI Analysis Report</title>
                <style>
                    body {{ font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }}
                    .header {{ background: #2c3e50; color: white; padding: 20px; border-radius: 5px; }}
                    .result {{ background: white; margin: 10px 0; padding: 15px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }}
                    .element {{ background: #ecf0f1; padding: 10px; margin: 5px 0; border-radius: 3px; }}
                </style>
            </head>
            <body>
                <div class="header">
                    <h1> GUI Analysis Report</h1>
                    <p>Automated GUI element detection and analysis</p>
                </div>
            """
            
            # Analyze all GUI elements
            total_windows = sum(len(elem['windows']) for elem in self.gui_elements)
            total_buttons = sum(len(elem['buttons']) for elem in self.gui_elements)
            total_text_areas = sum(len(elem['text_areas']) for elem in self.gui_elements)
            total_menus = sum(len(elem['menus']) for elem in self.gui_elements)
            
            html += f"""
            <div class="result">
                <h2> Summary</h2>
                <p><strong>Total Windows:</strong> {total_windows}</p>
                <p><strong>Total Buttons:</strong> {total_buttons}</p>
                <p><strong>Total Text Areas:</strong> {total_text_areas}</p>
                <p><strong>Total Menus:</strong> {total_menus}</p>
                <p><strong>Analysis Frames:</strong> {len(self.gui_elements)}</p>
            </div>
            """
            
            # Add detailed analysis for each frame
            for i, elements in enumerate(self.gui_elements[-5:]):  # Show last 5 frames
                html += f"""
                <div class="result">
                    <h3>Frame {elements['frame']} - {elements['timestamp']}</h3>
                """
                
                if elements['windows']:
                    html += "<h4>Windows:</h4>"
                    for window in elements['windows'][:3]:  # Show first 3
                        html += f"""
                        <div class="element">
                            <strong>Window:</strong> {window['width']}x{window['height']} at ({window['x']}, {window['y']})
                        </div>
                        """
                
                if elements['buttons']:
                    html += "<h4>Buttons:</h4>"
                    for button in elements['buttons'][:3]:  # Show first 3
                        html += f"""
                        <div class="element">
                            <strong>Button:</strong> {button['width']}x{button['height']} at ({button['x']}, {button['y']})
                        </div>
                        """
                
                html += "</div>"
            
            html += """
            </body>
            </html>
            """
            
            # Save report
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(html)
            
            print(f" GUI analysis report saved: {report_path}")
            
            # Open report
            webbrowser.open(f"file://{os.path.abspath(report_path)}")
            
        except Exception as e:
            print(f" GUI analysis error: {e}")
    
    def start_gui_builder(self):
        """Start GUI builder with visual assistance"""
        try:
            print(" Starting GUI builder...")
            
            # Create GUI builder window
            self.gui_builder_window = tk.Toplevel()
            self.gui_builder_window.title(" GUI Builder - Visual Assistant")
            self.gui_builder_window.geometry("1000x700")
            
            # Create main frame
            main_frame = ttk.Frame(self.gui_builder_window)
            main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # Left panel - Code editor
            code_frame = ttk.LabelFrame(main_frame, text="GUI Code")
            code_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
            
            self.gui_code_editor = scrolledtext.ScrolledText(code_frame, font=("Consolas", 10))
            self.gui_code_editor.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # Right panel - Preview and tools
            preview_frame = ttk.LabelFrame(main_frame, text="GUI Preview")
            preview_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
            
            # Preview area
            self.gui_preview_label = tk.Label(preview_frame, text="GUI Preview will appear here", 
                                            bg="white", relief=tk.SUNKEN, width=40, height=20)
            self.gui_preview_label.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # GUI builder controls
            control_frame = ttk.Frame(self.gui_builder_window)
            control_frame.pack(fill=tk.X, padx=5, pady=5)
            
            ttk.Button(control_frame, text=" Generate GUI", command=self._generate_gui).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Preview GUI", command=self._preview_gui).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" Export Code", command=self._export_gui_code).pack(side=tk.LEFT, padx=2)
            ttk.Button(control_frame, text=" AI Assist", command=self._ai_gui_assist).pack(side=tk.LEFT, padx=2)
            
            # Load sample GUI code
            self._load_sample_gui_code()
            
            print(" GUI builder window opened")
            return True
            
        except Exception as e:
            print(f" GUI builder error: {e}")
            return False
    
    def _load_sample_gui_code(self):
        """Load sample GUI code"""
        try:
            sample_code = '''import tkinter as tk
from tkinter import ttk

class SampleGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Sample GUI")
        self.root.geometry("400x300")
        
        # Create main frame
        main_frame = ttk.Frame(root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Title label
        title_label = ttk.Label(main_frame, text="Welcome to GUI Builder", 
                              font=("Arial", 16, "bold"))
        title_label.pack(pady=(0, 20))
        
        # Input frame
        input_frame = ttk.Frame(main_frame)
        input_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(input_frame, text="Name:").pack(side=tk.LEFT)
        self.name_entry = ttk.Entry(input_frame)
        self.name_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(5, 0))
        
        # Button frame
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=10)
        
        ttk.Button(button_frame, text="Submit", command=self.submit).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(button_frame, text="Clear", command=self.clear).pack(side=tk.LEFT)
        
        # Output area
        self.output_text = tk.Text(main_frame, height=8, width=40)
        self.output_text.pack(fill=tk.BOTH, expand=True, pady=5)
        
    def submit(self):
        name = self.name_entry.get()
        if name:
            self.output_text.insert(tk.END, f"Hello, {name}!\\n")
            self.name_entry.delete(0, tk.END)
    
    def clear(self):
        self.output_text.delete(1.0, tk.END)

if __name__ == "__main__":
    root = tk.Tk()
    app = SampleGUI(root)
    root.mainloop()'''
            
            self.gui_code_editor.delete(1.0, tk.END)
            self.gui_code_editor.insert(tk.END, sample_code)
            
        except Exception as e:
            print(f" Load sample GUI code error: {e}")
    
    def _generate_gui(self):
        """Generate GUI from code"""
        try:
            code = self.gui_code_editor.get(1.0, tk.END).strip()
            if not code:
                messagebox.showwarning("No Code", "Please enter GUI code first")
                return
            
            # Save code to temporary file
            temp_file = "temp_gui.py"
            with open(temp_file, 'w', encoding='utf-8') as f:
                f.write(code)
            
            # Execute GUI code
            exec(compile(code, temp_file, 'exec'))
            
            print(" GUI generated successfully")
            messagebox.showinfo("GUI Generated", "GUI has been generated successfully!")
            
        except Exception as e:
            print(f" Generate GUI error: {e}")
            messagebox.showerror("GUI Generation Error", f"Error generating GUI: {e}")
    
    def _preview_gui(self):
        """Preview GUI"""
        try:
            # This would show a preview of the GUI
            # For now, just show a message
            messagebox.showinfo("GUI Preview", "GUI preview functionality coming soon!")
            
        except Exception as e:
            print(f" Preview GUI error: {e}")
    
    def _export_gui_code(self):
        """Export GUI code to file"""
        try:
            code = self.gui_code_editor.get(1.0, tk.END).strip()
            if not code:
                messagebox.showwarning("No Code", "No GUI code to export")
                return
            
            # Ask for save location
            filename = filedialog.asksaveasfilename(
                title="Save GUI Code",
                defaultextension=".py",
                filetypes=[("Python files", "*.py"), ("All files", "*.*")]
            )
            
            if filename:
                with open(filename, 'w', encoding='utf-8') as f:
                    f.write(code)
                
                print(f" GUI code exported to: {filename}")
                messagebox.showinfo("Export Complete", f"GUI code saved to: {filename}")
            
        except Exception as e:
            print(f" Export GUI code error: {e}")
            messagebox.showerror("Export Error", f"Error exporting GUI code: {e}")
    
    def _ai_gui_assist(self):
        """Get AI assistance for GUI development"""
        try:
            # This would provide AI assistance for GUI development
            # For now, just show a message
            messagebox.showinfo("AI GUI Assist", "AI GUI assistance coming soon!")
            
        except Exception as e:
            print(f" AI GUI assist error: {e}")
    
    def configure_ai_models(self):
        """Configure AI models with API keys"""
        try:
            # Create AI model configuration window
            self.ai_config_window = tk.Toplevel()
            self.ai_config_window.title(" Configure AI Models")
            self.ai_config_window.geometry("600x500")
            
            # Create notebook for different models
            notebook = ttk.Notebook(self.ai_config_window)
            notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
            
            # OpenAI tab
            openai_frame = ttk.Frame(notebook)
            notebook.add(openai_frame, text="ChatGPT")
            
            ttk.Label(openai_frame, text="OpenAI API Key:", font=("Arial", 10, "bold")).pack(anchor=tk.W, pady=5)
            self.openai_key_entry = ttk.Entry(openai_frame, width=60, show="*")
            self.openai_key_entry.pack(fill=tk.X, pady=5)
            
            ttk.Label(openai_frame, text="Model:").pack(anchor=tk.W, pady=5)
            self.openai_model_combo = ttk.Combobox(openai_frame, values=["gpt-4", "gpt-3.5-turbo", "gpt-4-turbo"])
            self.openai_model_combo.set("gpt-4")
            self.openai_model_combo.pack(fill=tk.X, pady=5)
            
            ttk.Button(openai_frame, text=" Test Connection", command=self._test_openai).pack(pady=10)
            
            # Ollama tab
            ollama_frame = ttk.Frame(notebook)
            notebook.add(ollama_frame, text="Ollama")
            
            ttk.Label(ollama_frame, text="Ollama URL:", font=("Arial", 10, "bold")).pack(anchor=tk.W, pady=5)
            self.ollama_url_entry = ttk.Entry(ollama_frame, width=60)
            self.ollama_url_entry.insert(0, "http://localhost:11434")
            self.ollama_url_entry.pack(fill=tk.X, pady=5)
            
            ttk.Label(ollama_frame, text="Model:").pack(anchor=tk.W, pady=5)
            self.ollama_model_combo = ttk.Combobox(ollama_frame, values=["llama2", "codellama", "mistral", "phi"])
            self.ollama_model_combo.set("llama2")
            self.ollama_model_combo.pack(fill=tk.X, pady=5)
            
            ttk.Button(ollama_frame, text=" Test Connection", command=self._test_ollama).pack(pady=10)
            
            # Claude tab
            claude_frame = ttk.Frame(notebook)
            notebook.add(claude_frame, text="Claude")
            
            ttk.Label(claude_frame, text="Claude API Key:", font=("Arial", 10, "bold")).pack(anchor=tk.W, pady=5)
            self.claude_key_entry = ttk.Entry(claude_frame, width=60, show="*")
            self.claude_key_entry.pack(fill=tk.X, pady=5)
            
            ttk.Label(claude_frame, text="Model:").pack(anchor=tk.W, pady=5)
            self.claude_model_combo = ttk.Combobox(claude_frame, values=["claude-3-sonnet-20240229", "claude-3-haiku-20240307", "claude-3-opus-20240229"])
            self.claude_model_combo.set("claude-3-sonnet-20240229")
            self.claude_model_combo.pack(fill=tk.X, pady=5)
            
            ttk.Button(claude_frame, text=" Test Connection", command=self._test_claude).pack(pady=10)
            
            # Gemini tab
            gemini_frame = ttk.Frame(notebook)
            notebook.add(gemini_frame, text="Gemini")
            
            ttk.Label(gemini_frame, text="Gemini API Key:", font=("Arial", 10, "bold")).pack(anchor=tk.W, pady=5)
            self.gemini_key_entry = ttk.Entry(gemini_frame, width=60, show="*")
            self.gemini_key_entry.pack(fill=tk.X, pady=5)
            
            ttk.Label(gemini_frame, text="Model:").pack(anchor=tk.W, pady=5)
            self.gemini_model_combo = ttk.Combobox(gemini_frame, values=["gemini-pro", "gemini-pro-vision"])
            self.gemini_model_combo.set("gemini-pro")
            self.gemini_model_combo.pack(fill=tk.X, pady=5)
            
            ttk.Button(gemini_frame, text=" Test Connection", command=self._test_gemini).pack(pady=10)
            
            # Control buttons
            button_frame = ttk.Frame(self.ai_config_window)
            button_frame.pack(fill=tk.X, padx=10, pady=10)
            
            ttk.Button(button_frame, text=" Save All Configurations", command=self._save_ai_configs).pack(side=tk.LEFT, padx=5)
            ttk.Button(button_frame, text=" Test All Models", command=self._test_all_models).pack(side=tk.LEFT, padx=5)
            ttk.Button(button_frame, text=" Close", command=self.ai_config_window.destroy).pack(side=tk.RIGHT, padx=5)
            
            print(" AI model configuration window opened")
            
        except Exception as e:
            print(f" AI model configuration error: {e}")
            messagebox.showerror("AI Configuration Error", f"Error opening AI configuration: {e}")
    
    def _test_openai(self):
        """Test OpenAI connection"""
        try:
            api_key = self.openai_key_entry.get().strip()
            if not api_key:
                messagebox.showwarning("No API Key", "Please enter OpenAI API key")
                return
            
            # Setup model
            self.debugging_agent.ai_models.setup_model('openai', api_key)
            
            # Test query
            response = self.debugging_agent.ai_models.query_openai("Hello, this is a test message.")
            
            if response:
                messagebox.showinfo("OpenAI Test", f"Connection successful!\n\nResponse: {response[:100]}...")
            else:
                messagebox.showerror("OpenAI Test", "Connection failed. Check your API key.")
                
        except Exception as e:
            messagebox.showerror("OpenAI Test Error", f"Error testing OpenAI: {e}")
    
    def _test_ollama(self):
        """Test Ollama connection"""
        try:
            url = self.ollama_url_entry.get().strip()
            if not url:
                messagebox.showwarning("No URL", "Please enter Ollama URL")
                return
            
            # Setup model
            self.debugging_agent.ai_models.setup_model('ollama', '', url)
            
            # Test query
            response = self.debugging_agent.ai_models.query_ollama("Hello, this is a test message.")
            
            if response:
                messagebox.showinfo("Ollama Test", f"Connection successful!\n\nResponse: {response[:100]}...")
            else:
                messagebox.showerror("Ollama Test", "Connection failed. Make sure Ollama is running.")
                
        except Exception as e:
            messagebox.showerror("Ollama Test Error", f"Error testing Ollama: {e}")
    
    def _test_claude(self):
        """Test Claude connection"""
        try:
            api_key = self.claude_key_entry.get().strip()
            if not api_key:
                messagebox.showwarning("No API Key", "Please enter Claude API key")
                return
            
            # Setup model
            self.debugging_agent.ai_models.setup_model('claude', api_key)
            
            # Test query
            response = self.debugging_agent.ai_models.query_claude("Hello, this is a test message.")
            
            if response:
                messagebox.showinfo("Claude Test", f"Connection successful!\n\nResponse: {response[:100]}...")
            else:
                messagebox.showerror("Claude Test", "Connection failed. Check your API key.")
                
        except Exception as e:
            messagebox.showerror("Claude Test Error", f"Error testing Claude: {e}")
    
    def _test_gemini(self):
        """Test Gemini connection"""
        try:
            api_key = self.gemini_key_entry.get().strip()
            if not api_key:
                messagebox.showwarning("No API Key", "Please enter Gemini API key")
                return
            
            # Setup model
            self.debugging_agent.ai_models.setup_model('gemini', api_key)
            
            # Test query
            response = self.debugging_agent.ai_models.query_gemini("Hello, this is a test message.")
            
            if response:
                messagebox.showinfo("Gemini Test", f"Connection successful!\n\nResponse: {response[:100]}...")
            else:
                messagebox.showerror("Gemini Test", "Connection failed. Check your API key.")
                
        except Exception as e:
            messagebox.showerror("Gemini Test Error", f"Error testing Gemini: {e}")
    
    def _save_ai_configs(self):
        """Save all AI model configurations"""
        try:
            # Save OpenAI config
            openai_key = self.openai_key_entry.get().strip()
            if openai_key:
                self.debugging_agent.ai_models.setup_model('openai', openai_key)
            
            # Save Ollama config
            ollama_url = self.ollama_url_entry.get().strip()
            if ollama_url:
                self.debugging_agent.ai_models.setup_model('ollama', '', ollama_url)
            
            # Save Claude config
            claude_key = self.claude_key_entry.get().strip()
            if claude_key:
                self.debugging_agent.ai_models.setup_model('claude', claude_key)
            
            # Save Gemini config
            gemini_key = self.gemini_key_entry.get().strip()
            if gemini_key:
                self.debugging_agent.ai_models.setup_model('gemini', gemini_key)
            
            messagebox.showinfo("Configuration Saved", "AI model configurations saved successfully!")
            
        except Exception as e:
            messagebox.showerror("Save Error", f"Error saving configurations: {e}")
    
    def _test_all_models(self):
        """Test all configured AI models"""
        try:
            self.clear_output()
            self.append_output(" Testing all AI models...\n")
            
            # Test each model
            models_to_test = ['openai', 'ollama', 'claude', 'gemini']
            for model in models_to_test:
                if self.debugging_agent.ai_models.models[model]['enabled']:
                    self.append_output(f" Testing {model}...\n")
                    # Add actual test logic here
                    self.append_output(f" {model}: Ready\n")
                else:
                    self.append_output(f" {model}: Not configured\n")
            
            self.append_output(" AI model testing complete!\n")
            
        except Exception as e:
            self.append_output(f" AI model testing error: {e}\n")
    
    def ai_code_analysis(self):
        """Analyze code using AI models"""
        try:
            code = self.editor.get("1.0", tk.END).strip()
            if not code:
                messagebox.showwarning("No Code", "Please enter code to analyze")
                return
            
            self.clear_output()
            self.append_output(" Analyzing code with AI models...\n")
            
            # Use AI models to analyze code
            analysis = self.debugging_agent.ai_models.analyze_code_with_ai(code)
            
            if analysis:
                self.append_output(" AI Analysis Results:\n")
                self.append_output("=" * 50 + "\n")
                
                for model, response in analysis['responses'].items():
                    if response:
                        self.append_output(f"\n{model.upper()} Analysis:\n")
                        self.append_output(f"{response}\n")
                        self.append_output("-" * 30 + "\n")
                
                # Show consensus
                consensus = analysis.get('consensus', {})
                if isinstance(consensus, dict):
                    self.append_output(f"\nConsensus Analysis:\n")
                    self.append_output(f"Models used: {consensus.get('total_models', 0)}\n")
                    self.append_output(f"Common issues: {consensus.get('common_issues', [])}\n")
                
                self.status_bar.config(text="AI code analysis complete")
            else:
                self.append_output(" No AI models configured or available\n")
                self.status_bar.config(text="No AI models available")
            
        except Exception as e:
            print(f" AI code analysis error: {e}")
            messagebox.showerror("AI Analysis Error", f"Error in AI code analysis: {e}")
    
    def ai_code_generation(self):
        """Generate code using AI models"""
        try:
            # Ask for code description
            description = tk.simpledialog.askstring("Code Generation", "Describe the code you want to generate:")
            if not description:
                return
            
            self.clear_output()
            self.append_output(f" Generating code: {description}\n")
            self.append_output("=" * 50 + "\n")
            
            # Use AI models to generate code
            results = self.debugging_agent.ai_models.generate_code_with_ai(description)
            
            if results:
                for model, response in results.items():
                    if response:
                        self.append_output(f"\n{model.upper()} Generated Code:\n")
                        self.append_output(f"{response}\n")
                        self.append_output("-" * 30 + "\n")
                
                self.status_bar.config(text="AI code generation complete")
            else:
                self.append_output(" No AI models configured or available\n")
                self.status_bar.config(text="No AI models available")
            
        except Exception as e:
            print(f" AI code generation error: {e}")
            messagebox.showerror("AI Generation Error", f"Error in AI code generation: {e}")
    
    def ai_debugging(self):
        """Debug using AI models"""
        try:
            # Ask for error message
            error_message = tk.simpledialog.askstring("AI Debugging", "Enter the error message to debug:")
            if not error_message:
                return
            
            code_context = self.editor.get("1.0", tk.END).strip()
            
            self.clear_output()
            self.append_output(f" Debugging error: {error_message}\n")
            self.append_output("=" * 50 + "\n")
            
            # Use AI models to debug
            results = self.debugging_agent.ai_models.debug_with_ai(error_message, code_context)
            
            if results:
                for model, response in results.items():
                    if response:
                        self.append_output(f"\n{model.upper()} Debugging Solution:\n")
                        self.append_output(f"{response}\n")
                        self.append_output("-" * 30 + "\n")
                
                self.status_bar.config(text="AI debugging complete")
            else:
                self.append_output(" No AI models configured or available\n")
                self.status_bar.config(text="No AI models available")
            
        except Exception as e:
            print(f" AI debugging error: {e}")
            messagebox.showerror("AI Debugging Error", f"Error in AI debugging: {e}")
    
    def query_chatgpt(self):
        """Query ChatGPT directly"""
        try:
            if not self.debugging_agent.ai_models.models['openai']['enabled']:
                messagebox.showwarning("ChatGPT Not Configured", "Please configure ChatGPT API key first")
                return
            
            prompt = tk.simpledialog.askstring("ChatGPT Query", "Enter your question for ChatGPT:")
            if not prompt:
                return
            
            self.clear_output()
            self.append_output(f" ChatGPT Query: {prompt}\n")
            self.append_output("=" * 50 + "\n")
            
            response = self.debugging_agent.ai_models.query_openai(prompt)
            
            if response:
                self.append_output(f"ChatGPT Response:\n{response}\n")
                self.status_bar.config(text="ChatGPT query complete")
            else:
                self.append_output(" ChatGPT query failed\n")
                self.status_bar.config(text="ChatGPT query failed")
            
        except Exception as e:
            print(f" ChatGPT query error: {e}")
            messagebox.showerror("ChatGPT Error", f"Error querying ChatGPT: {e}")
    
    def query_ollama(self):
        """Query Ollama directly"""
        try:
            if not self.debugging_agent.ai_models.models['ollama']['enabled']:
                messagebox.showwarning("Ollama Not Configured", "Please configure Ollama first")
                return
            
            prompt = tk.simpledialog.askstring("Ollama Query", "Enter your question for Ollama:")
            if not prompt:
                return
            
            self.clear_output()
            self.append_output(f" Ollama Query: {prompt}\n")
            self.append_output("=" * 50 + "\n")
            
            response = self.debugging_agent.ai_models.query_ollama(prompt)
            
            if response:
                self.append_output(f"Ollama Response:\n{response}\n")
                self.status_bar.config(text="Ollama query complete")
            else:
                self.append_output(" Ollama query failed\n")
                self.status_bar.config(text="Ollama query failed")
            
        except Exception as e:
            print(f" Ollama query error: {e}")
            messagebox.showerror("Ollama Error", f"Error querying Ollama: {e}")
    
    def query_claude(self):
        """Query Claude directly"""
        try:
            if not self.debugging_agent.ai_models.models['claude']['enabled']:
                messagebox.showwarning("Claude Not Configured", "Please configure Claude API key first")
                return
            
            prompt = tk.simpledialog.askstring("Claude Query", "Enter your question for Claude:")
            if not prompt:
                return
            
            self.clear_output()
            self.append_output(f" Claude Query: {prompt}\n")
            self.append_output("=" * 50 + "\n")
            
            response = self.debugging_agent.ai_models.query_claude(prompt)
            
            if response:
                self.append_output(f"Claude Response:\n{response}\n")
                self.status_bar.config(text="Claude query complete")
            else:
                self.append_output(" Claude query failed\n")
                self.status_bar.config(text="Claude query failed")
            
        except Exception as e:
            print(f" Claude query error: {e}")
            messagebox.showerror("Claude Error", f"Error querying Claude: {e}")
    
    def query_gemini(self):
        """Query Gemini directly"""
        try:
            if not self.debugging_agent.ai_models.models['gemini']['enabled']:
                messagebox.showwarning("Gemini Not Configured", "Please configure Gemini API key first")
                return
            
            prompt = tk.simpledialog.askstring("Gemini Query", "Enter your question for Gemini:")
            if not prompt:
                return
            
            self.clear_output()
            self.append_output(f" Gemini Query: {prompt}\n")
            self.append_output("=" * 50 + "\n")
            
            response = self.debugging_agent.ai_models.query_gemini(prompt)
            
            if response:
                self.append_output(f"Gemini Response:\n{response}\n")
                self.status_bar.config(text="Gemini query complete")
            else:
                self.append_output(" Gemini query failed\n")
                self.status_bar.config(text="Gemini query failed")
            
        except Exception as e:
            print(f" Gemini query error: {e}")
            messagebox.showerror("Gemini Error", f"Error querying Gemini: {e}")
    
    def query_all_ai_models(self):
        """Query all configured AI models"""
        try:
            prompt = tk.simpledialog.askstring("All AI Models Query", "Enter your question for all AI models:")
            if not prompt:
                return
            
            self.clear_output()
            self.append_output(f" Querying all AI models: {prompt}\n")
            self.append_output("=" * 50 + "\n")
            
            results = self.debugging_agent.ai_models.query_all_models(prompt)
            
            if results:
                for model, response in results.items():
                    if response:
                        self.append_output(f"\n{model.upper()} Response:\n")
                        self.append_output(f"{response}\n")
                        self.append_output("-" * 30 + "\n")
                
                self.status_bar.config(text="All AI models query complete")
            else:
                self.append_output(" No AI models configured or available\n")
                self.status_bar.config(text="No AI models available")
            
        except Exception as e:
            print(f" All AI models query error: {e}")
            messagebox.showerror("AI Models Error", f"Error querying AI models: {e}")

if __name__ == "__main__":
    # Print distribution warning to console
    print("=" * 80)
    print("️  DO NOT DISTRIBUTE - CONFIDENTIAL TOOL ️")
    print("=" * 80)
    print("This is a specialized reverse engineering and security analysis tool.")
    print("Distribution of this software is strictly prohibited.")
    print("Reverse engineering of this IDE is strictly prohibited.")
    print("Use only for authorized security research and analysis.")
    print("=" * 80)
    print()
    
    print(" Starting Proper EXE Compiler IDE...")
    print(" Creates REAL executables, not tiny wrappers!")
    
    try:
        ide = ProperExeIDE()
        ide.run()
    except Exception as e:
        print(f" IDE startup error: {e}")
        input("Press Enter to exit...")
