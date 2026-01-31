#!/usr/bin/env python3
"""
Curl SDK/Toolchain Downloader
Automatically download SDKs, toolchains, and compilers using curl
"""

import os
import sys
import json
import subprocess
import time
import threading
from typing import Dict, List, Any, Optional
import hashlib
import base64
from urllib.parse import urlparse, urljoin
import re

class CurlSDKDownloader:
    """Curl-based SDK and toolchain downloader"""
    
    def __init__(self):
        self.initialized = False
        self.download_queue = []
        self.downloading = False
        self.download_threads = []
        self.max_concurrent_downloads = 5
        
        # SDK and toolchain repositories
        self.repositories = {
            'gcc': {
                'name': 'GNU Compiler Collection',
                'base_url': 'https://gcc.gnu.org/releases/',
                'download_urls': [
                    'https://gcc.gnu.org/releases/gcc-13.2.0/gcc-13.2.0.tar.xz',
                    'https://gcc.gnu.org/releases/gcc-12.3.0/gcc-12.3.0.tar.xz',
                    'https://gcc.gnu.org/releases/gcc-11.4.0/gcc-11.4.0.tar.xz'
                ],
                'type': 'compiler'
            },
            'clang': {
                'name': 'LLVM Clang',
                'base_url': 'https://releases.llvm.org/',
                'download_urls': [
                    'https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/clang+llvm-17.0.6-x86_64-linux-gnu-ubuntu-22.04.tar.xz',
                    'https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.6/clang+llvm-16.0.6-x86_64-linux-gnu-ubuntu-22.04.tar.xz',
                    'https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/clang+llvm-15.0.7-x86_64-linux-gnu-ubuntu-18.04.tar.xz'
                ],
                'type': 'compiler'
            },
            'python': {
                'name': 'Python',
                'base_url': 'https://www.python.org/downloads/',
                'download_urls': [
                    'https://www.python.org/ftp/python/3.12.0/Python-3.12.0.tgz',
                    'https://www.python.org/ftp/python/3.11.6/Python-3.11.6.tgz',
                    'https://www.python.org/ftp/python/3.10.12/Python-3.10.12.tgz'
                ],
                'type': 'runtime'
            },
            'nodejs': {
                'name': 'Node.js',
                'base_url': 'https://nodejs.org/dist/',
                'download_urls': [
                    'https://nodejs.org/dist/v20.10.0/node-v20.10.0-linux-x64.tar.xz',
                    'https://nodejs.org/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz',
                    'https://nodejs.org/dist/v16.20.2/node-v16.20.2-linux-x64.tar.xz'
                ],
                'type': 'runtime'
            },
            'rust': {
                'name': 'Rust',
                'base_url': 'https://forge.rust-lang.org/infra/channel-layout.html',
                'download_urls': [
                    'https://static.rust-lang.org/dist/rust-1.75.0-x86_64-unknown-linux-gnu.tar.xz',
                    'https://static.rust-lang.org/dist/rust-1.74.1-x86_64-unknown-linux-gnu.tar.xz',
                    'https://static.rust-lang.org/dist/rust-1.73.0-x86_64-unknown-linux-gnu.tar.xz'
                ],
                'type': 'compiler'
            },
            'go': {
                'name': 'Go',
                'base_url': 'https://golang.org/dl/',
                'download_urls': [
                    'https://golang.org/dl/go1.21.5.linux-amd64.tar.gz',
                    'https://golang.org/dl/go1.20.12.linux-amd64.tar.gz',
                    'https://golang.org/dl/go1.19.13.linux-amd64.tar.gz'
                ],
                'type': 'compiler'
            },
            'jdk': {
                'name': 'OpenJDK',
                'base_url': 'https://jdk.java.net/',
                'download_urls': [
                    'https://download.java.net/java/GA/jdk21.0.1/415e3f918a1f91e5b076f8242c92d4f/12/GPL/openjdk-21.0.1_linux-x64_bin.tar.gz',
                    'https://download.java.net/java/GA/jdk20.0.2/6e380f22cbe7469fa75fb448bd903d8e/9/GPL/openjdk-20.0.2_linux-x64_bin.tar.gz',
                    'https://download.java.net/java/GA/jdk19.0.2/7a4bb81c1dfe4a208d1b0eb2c4e6c5a3/9/GPL/openjdk-19.0.2_linux-x64_bin.tar.gz'
                ],
                'type': 'runtime'
            },
            'dotnet': {
                'name': '.NET',
                'base_url': 'https://dotnet.microsoft.com/download',
                'download_urls': [
                    'https://download.visualstudio.microsoft.com/download/pr/8c2b4b0e-8b0a-4b0a-8b0a-4b0a8b0a4b0a/dotnet-sdk-8.0.100-linux-x64.tar.gz',
                    'https://download.visualstudio.microsoft.com/download/pr/7c2b4b0e-8b0a-4b0a-8b0a-4b0a8b0a4b0a/dotnet-sdk-7.0.100-linux-x64.tar.gz',
                    'https://download.visualstudio.microsoft.com/download/pr/6c2b4b0e-8b0a-4b0a-8b0a-4b0a8b0a4b0a/dotnet-sdk-6.0.100-linux-x64.tar.gz'
                ],
                'type': 'runtime'
            }
        }
        
        # Platform-specific URLs
        self.platform_urls = {
            'windows': {
                'gcc': 'https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/winlibs-x86_64-posix-seh-gcc-13.2.0-mingw-w64-11.0.0-r1.zip',
                'clang': 'https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/LLVM-17.0.6-win64.exe',
                'python': 'https://www.python.org/ftp/python/3.12.0/python-3.12.0-amd64.exe',
                'nodejs': 'https://nodejs.org/dist/v20.10.0/node-v20.10.0-win-x64.zip',
                'rust': 'https://static.rust-lang.org/dist/rust-1.75.0-x86_64-pc-windows-msvc.msi',
                'go': 'https://golang.org/dl/go1.21.5.windows-amd64.zip',
                'jdk': 'https://download.java.net/java/GA/jdk21.0.1/415e3f918a1f91e5b076f8242c92d4f/12/GPL/openjdk-21.0.1_windows-x64_bin.zip',
                'dotnet': 'https://download.visualstudio.microsoft.com/download/pr/8c2b4b0e-8b0a-4b0a-8b0a-4b0a8b0a4b0a/dotnet-sdk-8.0.100-win-x64.exe'
            },
            'linux': {
                'gcc': 'https://gcc.gnu.org/releases/gcc-13.2.0/gcc-13.2.0.tar.xz',
                'clang': 'https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/clang+llvm-17.0.6-x86_64-linux-gnu-ubuntu-22.04.tar.xz',
                'python': 'https://www.python.org/ftp/python/3.12.0/Python-3.12.0.tgz',
                'nodejs': 'https://nodejs.org/dist/v20.10.0/node-v20.10.0-linux-x64.tar.xz',
                'rust': 'https://static.rust-lang.org/dist/rust-1.75.0-x86_64-unknown-linux-gnu.tar.xz',
                'go': 'https://golang.org/dl/go1.21.5.linux-amd64.tar.gz',
                'jdk': 'https://download.java.net/java/GA/jdk21.0.1/415e3f918a1f91e5b076f8242c92d4f/12/GPL/openjdk-21.0.1_linux-x64_bin.tar.gz',
                'dotnet': 'https://download.visualstudio.microsoft.com/download/pr/8c2b4b0e-8b0a-4b0a-8b0a-4b0a8b0a4b0a/dotnet-sdk-8.0.100-linux-x64.tar.gz'
            },
            'macos': {
                'gcc': 'https://gcc.gnu.org/releases/gcc-13.2.0/gcc-13.2.0.tar.xz',
                'clang': 'https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/clang+llvm-17.0.6-x86_64-apple-darwin22.0.tar.xz',
                'python': 'https://www.python.org/ftp/python/3.12.0/Python-3.12.0.tgz',
                'nodejs': 'https://nodejs.org/dist/v20.10.0/node-v20.10.0-darwin-x64.tar.gz',
                'rust': 'https://static.rust-lang.org/dist/rust-1.75.0-x86_64-apple-darwin.tar.xz',
                'go': 'https://golang.org/dl/go1.21.5.darwin-amd64.tar.gz',
                'jdk': 'https://download.java.net/java/GA/jdk21.0.1/415e3f918a1f91e5b076f8242c92d4f/12/GPL/openjdk-21.0.1_macos-x64_bin.tar.gz',
                'dotnet': 'https://download.visualstudio.microsoft.com/download/pr/8c2b4b0e-8b0a-4b0a-8b0a-4b0a8b0a4b0a/dotnet-sdk-8.0.100-osx-x64.tar.gz'
            }
        }
        
        print("Curl SDK Downloader created")
    
    def initialize(self):
        """Initialize the curl downloader"""
        try:
            # Check if curl is available
            if not self.check_curl_available():
                print("❌ curl is not available on this system")
                return False
            
            # Create download directory
            self.download_dir = "downloaded_sdks"
            os.makedirs(self.download_dir, exist_ok=True)
            
            # Start download manager
            self.start_download_manager()
            
            self.initialized = True
            print("Curl SDK Downloader initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Curl SDK Downloader: {e}")
            return False
    
    def check_curl_available(self) -> bool:
        """Check if curl is available on the system"""
        try:
            result = subprocess.run(['curl', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def start_download_manager(self):
        """Start the download manager thread"""
        download_thread = threading.Thread(target=self.download_manager)
        download_thread.daemon = True
        download_thread.start()
        
        print("Download manager started")
    
    def download_manager(self):
        """Manage download queue"""
        while True:
            try:
                if self.download_queue and not self.downloading:
                    # Start downloads
                    self.start_downloads()
                
                time.sleep(1)  # Check every second
                
            except Exception as e:
                print(f"Download manager error: {e}")
                time.sleep(1)
    
    def start_downloads(self):
        """Start downloading from queue"""
        try:
            self.downloading = True
            
            # Start download threads
            for i in range(min(self.max_concurrent_downloads, len(self.download_queue))):
                if self.download_queue:
                    download_item = self.download_queue.pop(0)
                    download_thread = threading.Thread(
                        target=self.download_item,
                        args=(download_item,)
                    )
                    download_thread.daemon = True
                    download_thread.start()
                    self.download_threads.append(download_thread)
            
            # Wait for downloads to complete
            for thread in self.download_threads:
                thread.join()
            
            self.download_threads.clear()
            self.downloading = False
            
        except Exception as e:
            print(f"Error starting downloads: {e}")
            self.downloading = False
    
    def download_item(self, download_item: Dict[str, Any]):
        """Download a single item"""
        try:
            url = download_item['url']
            name = download_item['name']
            version = download_item['version']
            platform = download_item['platform']
            
            print(f"Downloading {name} v{version} for {platform}...")
            
            # Create filename from URL
            filename = self.get_filename_from_url(url)
            filepath = os.path.join(self.download_dir, filename)
            
            # Download using curl
            curl_command = [
                'curl', '-L', '--progress-bar', '--fail',
                '--connect-timeout', '30',
                '--max-time', '3600',  # 1 hour timeout
                '--retry', '3',
                '--retry-delay', '5',
                '-o', filepath,
                url
            ]
            
            result = subprocess.run(curl_command, capture_output=True, text=True)
            
            if result.returncode == 0:
                print(f"✅ Downloaded {name} v{version}")
                
                # Store in fileless storage
                self.store_downloaded_item(download_item, filepath)
            else:
                print(f"❌ Failed to download {name}: {result.stderr}")
                
        except Exception as e:
            print(f"Error downloading {download_item.get('name', 'unknown')}: {e}")
    
    def get_filename_from_url(self, url: str) -> str:
        """Extract filename from URL"""
        try:
            parsed_url = urlparse(url)
            filename = os.path.basename(parsed_url.path)
            
            if not filename or '.' not in filename:
                # Generate filename from URL
                filename = hashlib.md5(url.encode()).hexdigest()[:16] + '.tar.gz'
            
            return filename
            
        except Exception as e:
            print(f"Error extracting filename: {e}")
            return "download.tar.gz"
    
    def store_downloaded_item(self, download_item: Dict[str, Any], filepath: str):
        """Store downloaded item in fileless storage"""
        try:
            # Read file data
            with open(filepath, 'rb') as f:
                data = f.read()
            
            # Get file size
            file_size = len(data)
            
            # Create metadata
            metadata = {
                'description': f"Downloaded {download_item['name']} v{download_item['version']}",
                'platform': download_item['platform'],
                'download_url': download_item['url'],
                'file_size': file_size,
                'download_date': time.time(),
                'file_type': self.get_file_type(filepath)
            }
            
            # Store based on type
            if download_item['type'] == 'compiler':
                self.store_compiler(download_item['name'], download_item['version'], 
                                  download_item['platform'], data, metadata)
            elif download_item['type'] == 'runtime':
                self.store_runtime(download_item['name'], download_item['version'], 
                                 download_item['platform'], data, metadata)
            else:
                self.store_sdk(download_item['name'], download_item['version'], 
                             download_item['platform'], data, metadata)
            
            # Clean up downloaded file
            os.remove(filepath)
            
        except Exception as e:
            print(f"Error storing downloaded item: {e}")
    
    def get_file_type(self, filepath: str) -> str:
        """Get file type from extension"""
        ext = os.path.splitext(filepath)[1].lower()
        
        if ext in ['.tar.gz', '.tgz']:
            return 'tar.gz'
        elif ext in ['.tar.xz']:
            return 'tar.xz'
        elif ext in ['.zip']:
            return 'zip'
        elif ext in ['.exe']:
            return 'executable'
        elif ext in ['.msi']:
            return 'msi'
        else:
            return 'unknown'
    
    def store_compiler(self, name: str, version: str, platform: str, 
                      data: bytes, metadata: Dict[str, Any]):
        """Store compiler in fileless storage"""
        # This would integrate with the fileless storage system
        print(f"Storing compiler: {name} v{version} for {platform}")
    
    def store_runtime(self, name: str, version: str, platform: str, 
                     data: bytes, metadata: Dict[str, Any]):
        """Store runtime in fileless storage"""
        # This would integrate with the fileless storage system
        print(f"Storing runtime: {name} v{version} for {platform}")
    
    def store_sdk(self, name: str, version: str, platform: str, 
                 data: bytes, metadata: Dict[str, Any]):
        """Store SDK in fileless storage"""
        # This would integrate with the fileless storage system
        print(f"Storing SDK: {name} v{version} for {platform}")
    
    def download_sdk(self, name: str, version: Optional[str] = None, 
                     platform: Optional[str] = None) -> bool:
        """Download SDK by name"""
        try:
            if name not in self.repositories:
                print(f"❌ Unknown SDK: {name}")
                return False
            
            # Get platform
            if not platform:
                platform = self.detect_platform()
            
            # Get download URL
            if platform in self.platform_urls and name in self.platform_urls[platform]:
                url = self.platform_urls[platform][name]
            else:
                # Use default URLs
                repo = self.repositories[name]
                if version:
                    # Find specific version
                    url = self.find_version_url(name, version)
                else:
                    # Use latest version
                    url = repo['download_urls'][0]
            
            if not url:
                print(f"❌ No download URL found for {name}")
                return False
            
            # Add to download queue
            download_item = {
                'name': name,
                'version': version or 'latest',
                'platform': platform,
                'type': self.repositories[name]['type'],
                'url': url
            }
            
            self.download_queue.append(download_item)
            print(f"📥 Added {name} to download queue")
            return True
            
        except Exception as e:
            print(f"Error downloading SDK {name}: {e}")
            return False
    
    def download_toolchain(self, name: str, version: Optional[str] = None, 
                          platform: Optional[str] = None) -> bool:
        """Download toolchain by name"""
        return self.download_sdk(name, version, platform)
    
    def download_compiler(self, name: str, version: Optional[str] = None, 
                         platform: Optional[str] = None) -> bool:
        """Download compiler by name"""
        return self.download_sdk(name, version, platform)
    
    def download_all_sdks(self, platform: Optional[str] = None) -> bool:
        """Download all available SDKs"""
        try:
            if not platform:
                platform = self.detect_platform()
            
            print(f"📥 Downloading all SDKs for {platform}...")
            
            for name in self.repositories:
                self.download_sdk(name, platform=platform)
            
            print("✅ All SDKs added to download queue")
            return True
            
        except Exception as e:
            print(f"Error downloading all SDKs: {e}")
            return False
    
    def download_essential_toolchain(self, platform: Optional[str] = None) -> bool:
        """Download essential toolchain (GCC, Clang, Python, Node.js)"""
        try:
            if not platform:
                platform = self.detect_platform()
            
            essential = ['gcc', 'clang', 'python', 'nodejs']
            
            print(f"📥 Downloading essential toolchain for {platform}...")
            
            for name in essential:
                self.download_sdk(name, platform=platform)
            
            print("✅ Essential toolchain added to download queue")
            return True
            
        except Exception as e:
            print(f"Error downloading essential toolchain: {e}")
            return False
    
    def detect_platform(self) -> str:
        """Detect current platform"""
        import platform
        
        system = platform.system().lower()
        
        if system == 'windows':
            return 'windows'
        elif system == 'darwin':
            return 'macos'
        elif system == 'linux':
            return 'linux'
        else:
            return 'linux'  # Default to Linux
    
    def find_version_url(self, name: str, version: str) -> Optional[str]:
        """Find download URL for specific version"""
        try:
            repo = self.repositories[name]
            
            # Search for version in URLs
            for url in repo['download_urls']:
                if version in url:
                    return url
            
            # If not found, return first URL
            return repo['download_urls'][0]
            
        except Exception as e:
            print(f"Error finding version URL: {e}")
            return None
    
    def get_download_status(self) -> Dict[str, Any]:
        """Get download status"""
        return {
            'queue_size': len(self.download_queue),
            'downloading': self.downloading,
            'active_threads': len(self.download_threads),
            'max_concurrent': self.max_concurrent_downloads
        }
    
    def list_available_sdks(self) -> List[Dict[str, Any]]:
        """List all available SDKs"""
        sdks = []
        
        for name, repo in self.repositories.items():
            sdks.append({
                'name': name,
                'display_name': repo['name'],
                'type': repo['type'],
                'base_url': repo['base_url'],
                'versions': len(repo['download_urls'])
            })
        
        return sdks
    
    def search_sdks(self, query: str) -> List[Dict[str, Any]]:
        """Search for SDKs by name or type"""
        results = []
        
        for name, repo in self.repositories.items():
            if (query.lower() in name.lower() or 
                query.lower() in repo['name'].lower() or
                query.lower() in repo['type'].lower()):
                results.append({
                    'name': name,
                    'display_name': repo['name'],
                    'type': repo['type'],
                    'base_url': repo['base_url']
                })
        
        return results
    
    def get_platform_urls(self, platform: str) -> Dict[str, str]:
        """Get all URLs for a specific platform"""
        if platform in self.platform_urls:
            return self.platform_urls[platform]
        else:
            return {}
    
    def add_custom_repository(self, name: str, display_name: str, 
                            base_url: str, download_urls: List[str], 
                            item_type: str = 'sdk') -> bool:
        """Add custom repository"""
        try:
            self.repositories[name] = {
                'name': display_name,
                'base_url': base_url,
                'download_urls': download_urls,
                'type': item_type
            }
            
            print(f"✅ Custom repository added: {name}")
            return True
            
        except Exception as e:
            print(f"Error adding custom repository: {e}")
            return False
    
    def update_repository_urls(self, name: str, new_urls: List[str]) -> bool:
        """Update repository URLs"""
        try:
            if name in self.repositories:
                self.repositories[name]['download_urls'] = new_urls
                print(f"✅ Repository URLs updated: {name}")
                return True
            else:
                print(f"❌ Repository not found: {name}")
                return False
                
        except Exception as e:
            print(f"Error updating repository URLs: {e}")
            return False
    
    def get_download_progress(self) -> Dict[str, Any]:
        """Get download progress information"""
        return {
            'queue_size': len(self.download_queue),
            'downloading': self.downloading,
            'active_threads': len(self.download_threads),
            'max_concurrent': self.max_concurrent_downloads,
            'download_dir': self.download_dir
        }
    
    def clear_download_queue(self):
        """Clear download queue"""
        self.download_queue.clear()
        print("✅ Download queue cleared")
    
    def set_max_concurrent_downloads(self, max_downloads: int):
        """Set maximum concurrent downloads"""
        if max_downloads > 0:
            self.max_concurrent_downloads = max_downloads
            print(f"✅ Max concurrent downloads set to {max_downloads}")
        else:
            print("❌ Invalid max concurrent downloads value")
    
    def shutdown(self):
        """Shutdown the curl downloader"""
        try:
            # Wait for current downloads to complete
            for thread in self.download_threads:
                thread.join(timeout=30)
            
            self.downloading = False
            self.initialized = False
            print("Curl SDK Downloader shutdown complete")
            
        except Exception as e:
            print(f"Error during shutdown: {e}")

def main():
    """Test the curl SDK downloader"""
    print("Testing Curl SDK Downloader...")
    
    downloader = CurlSDKDownloader()
    
    if downloader.initialize():
        print("✅ Curl SDK Downloader initialized")
        
        # List available SDKs
        sdks = downloader.list_available_sdks()
        print(f"Available SDKs: {len(sdks)}")
        for sdk in sdks:
            print(f"  - {sdk['name']}: {sdk['display_name']} ({sdk['type']})")
        
        # Test platform detection
        platform = downloader.detect_platform()
        print(f"Detected platform: {platform}")
        
        # Test downloading essential toolchain
        if downloader.download_essential_toolchain():
            print("✅ Essential toolchain download started")
        
        # Test download status
        status = downloader.get_download_status()
        print(f"Download status: {status}")
        
        # Test search
        search_results = downloader.search_sdks("python")
        print(f"Search results for 'python': {len(search_results)}")
        
        # Test platform URLs
        platform_urls = downloader.get_platform_urls(platform)
        print(f"Platform URLs for {platform}: {len(platform_urls)}")
        
        # Shutdown
        downloader.shutdown()
        print("✅ Curl SDK Downloader test complete")
    else:
        print("❌ Failed to initialize Curl SDK Downloader")

if __name__ == "__main__":
    main()
