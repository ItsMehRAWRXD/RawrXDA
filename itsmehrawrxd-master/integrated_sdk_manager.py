#!/usr/bin/env python3
"""
Integrated SDK Manager
Combines fileless storage with curl downloading for complete SDK management
"""

import os
import sys
import json
import time
import threading
from typing import Dict, List, Any, Optional
from fileless_sdk_storage import FilelessSDKStorage
from curl_sdk_downloader import CurlSDKDownloader

class IntegratedSDKManager:
    """Integrated SDK manager combining storage and downloading"""
    
    def __init__(self):
        self.initialized = False
        self.storage = FilelessSDKStorage()
        self.downloader = CurlSDKDownloader()
        self.auto_download = True
        self.auto_cleanup = True
        
        print("Integrated SDK Manager created")
    
    def initialize(self):
        """Initialize the integrated SDK manager"""
        try:
            # Initialize storage
            if not self.storage.initialize():
                print("❌ Failed to initialize storage")
                return False
            
            # Initialize downloader
            if not self.downloader.initialize():
                print("❌ Failed to initialize downloader")
                return False
            
            # Start integration thread
            self.start_integration_thread()
            
            self.initialized = True
            print("Integrated SDK Manager initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Integrated SDK Manager: {e}")
            return False
    
    def start_integration_thread(self):
        """Start integration thread for automatic management"""
        integration_thread = threading.Thread(target=self.integration_manager)
        integration_thread.daemon = True
        integration_thread.start()
        
        print("Integration thread started")
    
    def integration_manager(self):
        """Manage integration between storage and downloader"""
        while True:
            try:
                # Check if downloads are complete
                if not self.downloader.downloading and self.downloader.download_queue:
                    # Process completed downloads
                    self.process_completed_downloads()
                
                # Auto-cleanup if enabled
                if self.auto_cleanup:
                    self.auto_cleanup_old_data()
                
                time.sleep(10)  # Check every 10 seconds
                
            except Exception as e:
                print(f"Integration manager error: {e}")
                time.sleep(10)
    
    def process_completed_downloads(self):
        """Process completed downloads"""
        try:
            # This would process any completed downloads
            # and integrate them with the storage system
            pass
        except Exception as e:
            print(f"Error processing completed downloads: {e}")
    
    def auto_cleanup_old_data(self):
        """Automatically cleanup old data"""
        try:
            # This would trigger cleanup in the storage system
            pass
        except Exception as e:
            print(f"Error during auto cleanup: {e}")
    
    def get_sdk(self, name: str, version: Optional[str] = None, 
                platform: Optional[str] = None, auto_download: bool = True) -> Optional[Dict[str, Any]]:
        """Get SDK from storage or download if not available"""
        try:
            # Try to get from storage first
            sdk = self.storage.get_sdk(name, version)
            
            if sdk:
                print(f"✅ SDK found in storage: {name} v{sdk['version']}")
                return sdk
            
            # If not found and auto-download is enabled
            if auto_download and self.auto_download:
                print(f"📥 SDK not found in storage, downloading: {name}")
                
                if self.downloader.download_sdk(name, version, platform):
                    # Wait for download to complete
                    self.wait_for_download_completion()
                    
                    # Try to get from storage again
                    sdk = self.storage.get_sdk(name, version)
                    if sdk:
                        print(f"✅ SDK downloaded and stored: {name} v{sdk['version']}")
                        return sdk
            
            print(f"❌ SDK not available: {name}")
            return None
            
        except Exception as e:
            print(f"Error getting SDK {name}: {e}")
            return None
    
    def get_toolchain(self, name: str, version: Optional[str] = None, 
                     platform: Optional[str] = None, auto_download: bool = True) -> Optional[Dict[str, Any]]:
        """Get toolchain from storage or download if not available"""
        try:
            # Try to get from storage first
            toolchain = self.storage.get_toolchain(name, version)
            
            if toolchain:
                print(f"✅ Toolchain found in storage: {name} v{toolchain['version']}")
                return toolchain
            
            # If not found and auto-download is enabled
            if auto_download and self.auto_download:
                print(f"📥 Toolchain not found in storage, downloading: {name}")
                
                if self.downloader.download_toolchain(name, version, platform):
                    # Wait for download to complete
                    self.wait_for_download_completion()
                    
                    # Try to get from storage again
                    toolchain = self.storage.get_toolchain(name, version)
                    if toolchain:
                        print(f"✅ Toolchain downloaded and stored: {name} v{toolchain['version']}")
                        return toolchain
            
            print(f"❌ Toolchain not available: {name}")
            return None
            
        except Exception as e:
            print(f"Error getting toolchain {name}: {e}")
            return None
    
    def get_compiler(self, name: str, version: Optional[str] = None, 
                    platform: Optional[str] = None, auto_download: bool = True) -> Optional[Dict[str, Any]]:
        """Get compiler from storage or download if not available"""
        try:
            # Try to get from storage first
            compiler = self.storage.get_compiler(name, version)
            
            if compiler:
                print(f"✅ Compiler found in storage: {name} v{compiler['version']}")
                return compiler
            
            # If not found and auto-download is enabled
            if auto_download and self.auto_download:
                print(f"📥 Compiler not found in storage, downloading: {name}")
                
                if self.downloader.download_compiler(name, version, platform):
                    # Wait for download to complete
                    self.wait_for_download_completion()
                    
                    # Try to get from storage again
                    compiler = self.storage.get_compiler(name, version)
                    if compiler:
                        print(f"✅ Compiler downloaded and stored: {name} v{compiler['version']}")
                        return compiler
            
            print(f"❌ Compiler not available: {name}")
            return None
            
        except Exception as e:
            print(f"Error getting compiler {name}: {e}")
            return None
    
    def wait_for_download_completion(self, timeout: int = 300):
        """Wait for download completion"""
        try:
            start_time = time.time()
            
            while (self.downloader.downloading or 
                   self.downloader.download_queue) and \
                  (time.time() - start_time) < timeout:
                time.sleep(1)
            
            if (time.time() - start_time) >= timeout:
                print("⚠️ Download timeout reached")
            
        except Exception as e:
            print(f"Error waiting for download completion: {e}")
    
    def download_essential_toolchain(self, platform: Optional[str] = None) -> bool:
        """Download essential toolchain"""
        try:
            if self.downloader.download_essential_toolchain(platform):
                print("✅ Essential toolchain download started")
                return True
            else:
                print("❌ Failed to start essential toolchain download")
                return False
                
        except Exception as e:
            print(f"Error downloading essential toolchain: {e}")
            return False
    
    def download_all_sdks(self, platform: Optional[str] = None) -> bool:
        """Download all available SDKs"""
        try:
            if self.downloader.download_all_sdks(platform):
                print("✅ All SDKs download started")
                return True
            else:
                print("❌ Failed to start all SDKs download")
                return False
                
        except Exception as e:
            print(f"Error downloading all SDKs: {e}")
            return False
    
    def list_available_sdks(self) -> List[Dict[str, Any]]:
        """List all available SDKs"""
        return self.downloader.list_available_sdks()
    
    def list_stored_sdks(self) -> List[Dict[str, Any]]:
        """List all stored SDKs"""
        return self.storage.list_sdks()
    
    def list_stored_toolchains(self) -> List[Dict[str, Any]]:
        """List all stored toolchains"""
        return self.storage.list_toolchains()
    
    def list_stored_compilers(self) -> List[Dict[str, Any]]:
        """List all stored compilers"""
        return self.storage.list_compilers()
    
    def search_sdks(self, query: str) -> List[Dict[str, Any]]:
        """Search for SDKs"""
        return self.downloader.search_sdks(query)
    
    def search_stored_items(self, query: str) -> List[Dict[str, Any]]:
        """Search stored items"""
        return self.storage.search_items(query)
    
    def get_storage_statistics(self) -> Dict[str, Any]:
        """Get storage statistics"""
        return self.storage.get_storage_statistics()
    
    def get_download_status(self) -> Dict[str, Any]:
        """Get download status"""
        return self.downloader.get_download_status()
    
    def get_integrated_status(self) -> Dict[str, Any]:
        """Get integrated status"""
        return {
            'initialized': self.initialized,
            'auto_download': self.auto_download,
            'auto_cleanup': self.auto_cleanup,
            'storage_stats': self.get_storage_statistics(),
            'download_status': self.get_download_status()
        }
    
    def set_auto_download(self, enabled: bool):
        """Set auto-download mode"""
        self.auto_download = enabled
        print(f"✅ Auto-download set to: {enabled}")
    
    def set_auto_cleanup(self, enabled: bool):
        """Set auto-cleanup mode"""
        self.auto_cleanup = enabled
        print(f"✅ Auto-cleanup set to: {enabled}")
    
    def clear_download_queue(self):
        """Clear download queue"""
        self.downloader.clear_download_queue()
        print("✅ Download queue cleared")
    
    def set_max_concurrent_downloads(self, max_downloads: int):
        """Set maximum concurrent downloads"""
        self.downloader.set_max_concurrent_downloads(max_downloads)
        print(f"✅ Max concurrent downloads set to {max_downloads}")
    
    def add_custom_repository(self, name: str, display_name: str, 
                            base_url: str, download_urls: List[str], 
                            item_type: str = 'sdk') -> bool:
        """Add custom repository"""
        return self.downloader.add_custom_repository(name, display_name, base_url, download_urls, item_type)
    
    def update_repository_urls(self, name: str, new_urls: List[str]) -> bool:
        """Update repository URLs"""
        return self.downloader.update_repository_urls(name, new_urls)
    
    def get_platform_urls(self, platform: str) -> Dict[str, str]:
        """Get platform URLs"""
        return self.downloader.get_platform_urls(platform)
    
    def detect_platform(self) -> str:
        """Detect current platform"""
        return self.downloader.detect_platform()
    
    def shutdown(self):
        """Shutdown the integrated SDK manager"""
        try:
            # Shutdown storage
            self.storage.shutdown()
            
            # Shutdown downloader
            self.downloader.shutdown()
            
            self.initialized = False
            print("Integrated SDK Manager shutdown complete")
            
        except Exception as e:
            print(f"Error during shutdown: {e}")

def main():
    """Test the integrated SDK manager"""
    print("Testing Integrated SDK Manager...")
    
    manager = IntegratedSDKManager()
    
    if manager.initialize():
        print("✅ Integrated SDK Manager initialized")
        
        # Test platform detection
        platform = manager.detect_platform()
        print(f"Detected platform: {platform}")
        
        # Test listing available SDKs
        available_sdks = manager.list_available_sdks()
        print(f"Available SDKs: {len(available_sdks)}")
        
        # Test listing stored SDKs
        stored_sdks = manager.list_stored_sdks()
        print(f"Stored SDKs: {len(stored_sdks)}")
        
        # Test search
        search_results = manager.search_sdks("python")
        print(f"Search results for 'python': {len(search_results)}")
        
        # Test integrated status
        status = manager.get_integrated_status()
        print(f"Integrated status: {status}")
        
        # Test getting SDK (this would trigger download if not available)
        # sdk = manager.get_sdk("python", "3.12.0", platform)
        # if sdk:
        #     print(f"✅ SDK retrieved: {sdk['name']} v{sdk['version']}")
        
        # Shutdown
        manager.shutdown()
        print("✅ Integrated SDK Manager test complete")
    else:
        print("❌ Failed to initialize Integrated SDK Manager")

if __name__ == "__main__":
    main()
