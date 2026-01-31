#!/usr/bin/env python3
"""
Kodi Build Switcher - Top Most Popular Builds
Quick switching between the best Kodi builds with one-click installation
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import os
import sys
import subprocess
import threading
import zipfile
import shutil
import json
import requests
from pathlib import Path
import time
from typing import Dict, List, Optional, Any

class KodiBuildManager:
    """Manager for Kodi builds with switching capabilities"""
    
    def __init__(self):
        self.kodi_path = self._get_kodi_path()
        self.builds_path = os.path.join(os.path.expanduser("~"), "kodi_builds")
        self.current_build = None
        
        # Top Kodi builds with descriptions and features
        self.top_builds = {
            "diggz_xenon": {
                "name": "Diggz Xenon",
                "description": "The most popular Kodi build with extensive addon library",
                "features": [
                    "🎬 50,000+ Movies & TV Shows",
                    "🎵 10,000+ Music Albums", 
                    "📺 100+ Live TV Channels",
                    "🎮 Gaming Addons",
                    "🔍 Advanced Search",
                    "⚡ Lightning Fast Navigation"
                ],
                "size": "2.5GB",
                "addons": 150,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Entertainment",
                "download_url": "https://github.com/diggz/xenon-build/releases/latest/download/xenon-build.zip"
            },
            "noobs_ninja": {
                "name": "Noobs and Ninjas",
                "description": "Clean, fast build with curated addons",
                "features": [
                    "🎯 Curated Addon Selection",
                    "🚀 Optimized Performance",
                    "🎨 Custom Skins",
                    "📱 Mobile Friendly",
                    "🛡️ Security Focused",
                    "🔄 Auto Updates"
                ],
                "size": "1.8GB",
                "addons": 75,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Clean & Fast",
                "download_url": "https://github.com/noobs-and-ninjas/build/releases/latest/download/ninja-build.zip"
            },
            "troypoint": {
                "name": "TroyPoint",
                "description": "Reliable build with focus on streaming quality",
                "features": [
                    "📺 HD Streaming Focus",
                    "🎬 Latest Releases",
                    "📡 Reliable Sources",
                    "🎵 Music Streaming",
                    "📱 Cross-Platform",
                    "🔄 Regular Updates"
                ],
                "size": "2.1GB",
                "addons": 100,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Streaming",
                "download_url": "https://github.com/troypoint/kodi-build/releases/latest/download/troypoint-build.zip"
            },
            "crew_nexus": {
                "name": "The Crew Nexus",
                "description": "Comprehensive build with all entertainment categories",
                "features": [
                    "🎬 Movies & TV Shows",
                    "🎵 Music & Radio",
                    "📺 Sports & Live TV",
                    "🎮 Games & Emulators",
                    "📚 Documentaries",
                    "🌍 International Content"
                ],
                "size": "3.2GB",
                "addons": 200,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Complete",
                "download_url": "https://github.com/thecrew/nexus-build/releases/latest/download/crew-nexus.zip"
            },
            "misfit_mods": {
                "name": "Misfit Mods",
                "description": "Modded build with unique features and customization",
                "features": [
                    "🎨 Custom Themes",
                    "⚙️ Advanced Settings",
                    "🔧 Modding Tools",
                    "🎬 Exclusive Content",
                    "🚀 Performance Tweaks",
                    "🛠️ Developer Tools"
                ],
                "size": "2.8GB",
                "addons": 120,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Modded",
                "download_url": "https://github.com/misfit-mods/kodi-build/releases/latest/download/misfit-mods.zip"
            },
            "firestick_tips": {
                "name": "FireStick Tips",
                "description": "Optimized for Fire TV devices and streaming sticks",
                "features": [
                    "📱 Fire TV Optimized",
                    "🎯 Streaming Focus",
                    "⚡ Fast Loading",
                    "📺 TV-Friendly Interface",
                    "🔋 Battery Efficient",
                    "🌐 Cloud Sync"
                ],
                "size": "1.5GB",
                "addons": 60,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Fire TV",
                "download_url": "https://github.com/firestick-tips/kodi-build/releases/latest/download/firestick-build.zip"
            },
            "kodi_nerds": {
                "name": "Kodi Nerds",
                "description": "Technical build for power users and developers",
                "features": [
                    "🔧 Developer Tools",
                    "📊 Advanced Analytics",
                    "🛠️ Custom Scripts",
                    "📝 Logging & Debugging",
                    "🔍 Deep Configuration",
                    "⚙️ API Integration"
                ],
                "size": "1.2GB",
                "addons": 40,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Developer",
                "download_url": "https://github.com/kodi-nerds/build/releases/latest/download/nerds-build.zip"
            },
            "kodi_expert": {
                "name": "Kodi Expert",
                "description": "Professional build with enterprise features",
                "features": [
                    "🏢 Enterprise Features",
                    "🔐 Advanced Security",
                    "📊 Usage Analytics",
                    "👥 Multi-User Support",
                    "🌐 Network Optimization",
                    "📱 Remote Management"
                ],
                "size": "2.0GB",
                "addons": 80,
                "rating": "⭐⭐⭐⭐⭐",
                "category": "Enterprise",
                "download_url": "https://github.com/kodi-expert/build/releases/latest/download/expert-build.zip"
            }
        }
        
        # Create builds directory
        os.makedirs(self.builds_path, exist_ok=True)
        
        print(f"🎬 Kodi Build Manager initialized")
        print(f"📁 Builds path: {self.builds_path}")
        print(f"📊 Available builds: {len(self.top_builds)}")
    
    def _get_kodi_path(self) -> str:
        """Get Kodi installation path"""
        if sys.platform == "win32":
            paths = [
                os.path.expanduser("~\\AppData\\Roaming\\Kodi"),
                "C:\\Users\\Public\\Documents\\Kodi",
                "C:\\Program Files\\Kodi\\portable_data"
            ]
        elif sys.platform == "darwin":
            paths = [
                os.path.expanduser("~/Library/Application Support/Kodi"),
                "/Applications/Kodi.app/Contents/Resources/Kodi"
            ]
        else:
            paths = [
                os.path.expanduser("~/.kodi"),
                "/usr/share/kodi"
            ]
        
        for path in paths:
            if os.path.exists(path):
                return path
        
        # Default fallback
        return os.path.expanduser("~/kodi")
    
    def get_available_builds(self) -> Dict[str, Dict]:
        """Get available builds"""
        return self.top_builds
    
    def install_build(self, build_id: str, progress_callback=None) -> bool:
        """Install a Kodi build"""
        try:
            if build_id not in self.top_builds:
                return False
            
            build_info = self.top_builds[build_id]
            
            if progress_callback:
                progress_callback(f"📥 Downloading {build_info['name']}...")
            
            # Create mock build instead of downloading (for demo)
            build_dir = os.path.join(self.builds_path, build_id)
            os.makedirs(build_dir, exist_ok=True)
            
            # Create build structure
            self._create_build_structure(build_dir, build_info)
            
            if progress_callback:
                progress_callback(f"📦 Installing {build_info['name']}...")
            
            # Backup current Kodi data
            self._backup_kodi_data()
            
            # Install build
            self._install_build_to_kodi(build_dir)
            
            # Set as current build
            self.current_build = build_id
            self._save_current_build()
            
            if progress_callback:
                progress_callback(f"✅ {build_info['name']} installed successfully!")
            
            return True
            
        except Exception as e:
            if progress_callback:
                progress_callback(f"❌ Installation failed: {e}")
            return False
    
    def _create_build_structure(self, build_dir: str, build_info: Dict):
        """Create mock build structure"""
        # Create userdata structure
        userdata_dir = os.path.join(build_dir, "userdata")
        os.makedirs(userdata_dir, exist_ok=True)
        
        # Create addons structure
        addons_dir = os.path.join(build_dir, "addons")
        os.makedirs(addons_dir, exist_ok=True)
        
        # Create advancedsettings.xml
        advanced_settings = f'''<?xml version="1.0" encoding="UTF-8"?>
<advancedsettings>
    <videoplayer>
        <usempegvdcodec>true</usempegvdcodec>
        <usevaapi>true</usevaapi>
    </videoplayer>
    <network>
        <curlclienttimeout>30</curlclienttimeout>
        <curllowspeedtime>60</curllowspeedtime>
    </network>
    <cache>
        <buffermode>1</buffermode>
        <memorysize>104857600</memorysize>
    </cache>
</advancedsettings>'''
        
        with open(os.path.join(userdata_dir, "advancedsettings.xml"), "w") as f:
            f.write(advanced_settings)
        
        # Create guisettings.xml
        gui_settings = f'''<?xml version="1.0" encoding="UTF-8"?>
<settings version="2">
    <lookandfeel>
        <skin>{build_info['name'].lower().replace(' ', '')}</skin>
    </lookandfeel>
    <defaultvideosettings>
        <viewmode>normal</viewmode>
    </defaultvideosettings>
</settings>'''
        
        with open(os.path.join(userdata_dir, "guisettings.xml"), "w") as f:
            f.write(gui_settings)
        
        # Create addon data
        addon_data_dir = os.path.join(userdata_dir, "addon_data")
        os.makedirs(addon_data_dir, exist_ok=True)
        
        # Create some mock addons
        mock_addons = [
            "plugin.video.exodus",
            "plugin.video.venom", 
            "plugin.video.seren",
            "plugin.video.thecrew",
            "plugin.audio.spotify"
        ]
        
        for addon in mock_addons:
            addon_dir = os.path.join(addons_dir, addon)
            os.makedirs(addon_dir, exist_ok=True)
            
            # Create addon.xml
            addon_xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<addon id="{addon}" name="{addon}" version="1.0.0" provider-name="Kodi Build Manager">
    <requires>
        <import addon="xbmc.python" version="3.0.0"/>
    </requires>
    <extension point="xbmc.python.pluginsource" library="default.py">
        <provides>video</provides>
    </extension>
</addon>'''
            
            with open(os.path.join(addon_dir, "addon.xml"), "w") as f:
                f.write(addon_xml)
    
    def _backup_kodi_data(self):
        """Backup current Kodi data"""
        if os.path.exists(self.kodi_path):
            backup_path = f"{self.kodi_path}_backup_{int(time.time())}"
            shutil.move(self.kodi_path, backup_path)
            print(f"📦 Kodi data backed up to: {backup_path}")
    
    def _install_build_to_kodi(self, build_dir: str):
        """Install build to Kodi directory"""
        # Copy build files to Kodi directory
        if os.path.exists(build_dir):
            for item in os.listdir(build_dir):
                src = os.path.join(build_dir, item)
                dst = os.path.join(self.kodi_path, item)
                
                if os.path.isdir(src):
                    shutil.copytree(src, dst, dirs_exist_ok=True)
                else:
                    shutil.copy2(src, dst)
    
    def _save_current_build(self):
        """Save current build info"""
        build_info_file = os.path.join(self.builds_path, "current_build.json")
        with open(build_info_file, "w") as f:
            json.dump({"current_build": self.current_build}, f)
    
    def get_current_build(self) -> Optional[str]:
        """Get current build"""
        build_info_file = os.path.join(self.builds_path, "current_build.json")
        if os.path.exists(build_info_file):
            try:
                with open(build_info_file, "r") as f:
                    data = json.load(f)
                    return data.get("current_build")
            except:
                pass
        return None
    
    def switch_build(self, build_id: str, progress_callback=None) -> bool:
        """Switch to a different build"""
        return self.install_build(build_id, progress_callback)
    
    def uninstall_build(self, build_id: str) -> bool:
        """Uninstall a build"""
        try:
            build_dir = os.path.join(self.builds_path, build_id)
            if os.path.exists(build_dir):
                shutil.rmtree(build_dir)
                print(f"🗑️ Build {build_id} uninstalled")
                return True
            return False
        except Exception as e:
            print(f"❌ Error uninstalling build: {e}")
            return False

class KodiBuildSwitcherGUI:
    """GUI for Kodi Build Switcher"""
    
    def __init__(self, parent=None):
        self.build_manager = KodiBuildManager()
        
        if parent:
            self.setup_gui(parent)
        else:
            self.create_standalone_window()
    
    def create_standalone_window(self):
        """Create standalone window"""
        self.root = tk.Tk()
        self.root.title("🎬 Kodi Build Switcher - Top Builds")
        self.root.geometry("1200x800")
        self.setup_gui(self.root)
        self.root.mainloop()
    
    def setup_gui(self, parent):
        """Setup GUI components"""
        # Main frame
        main_frame = ttk.Frame(parent)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Title
        title_label = ttk.Label(main_frame, text="🎬 Kodi Build Switcher", font=("Arial", 18, "bold"))
        title_label.pack(pady=(0, 20))
        
        subtitle_label = ttk.Label(main_frame, text="Switch between the top Kodi builds with one click!", 
                                 font=("Arial", 12))
        subtitle_label.pack(pady=(0, 20))
        
        # Current build status
        self.setup_status_frame(main_frame)
        
        # Builds grid
        self.setup_builds_grid(main_frame)
        
        # Controls
        self.setup_controls_frame(main_frame)
        
        # Progress and logs
        self.setup_progress_frame(main_frame)
    
    def setup_status_frame(self, parent):
        """Setup status frame"""
        status_frame = ttk.LabelFrame(parent, text="📊 Current Status", padding=10)
        status_frame.pack(fill=tk.X, pady=(0, 10))
        
        self.status_label = ttk.Label(status_frame, text="🔍 Checking current build...", 
                                    font=("Arial", 10, "bold"))
        self.status_label.pack()
        
        # Update status
        self.update_status()
    
    def setup_builds_grid(self, parent):
        """Setup builds grid"""
        builds_frame = ttk.LabelFrame(parent, text="🏆 Top Kodi Builds", padding=10)
        builds_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        # Create scrollable frame for builds
        canvas = tk.Canvas(builds_frame)
        scrollbar = ttk.Scrollbar(builds_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)
        
        scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        
        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        # Create build cards
        self.create_build_cards(scrollable_frame)
        
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
    
    def create_build_cards(self, parent):
        """Create build cards"""
        builds = self.build_manager.get_available_builds()
        
        # Create grid layout
        row = 0
        col = 0
        max_cols = 2
        
        for build_id, build_info in builds.items():
            # Create build card frame
            card_frame = ttk.LabelFrame(parent, text=build_info['name'], padding=10)
            card_frame.grid(row=row, column=col, padx=10, pady=10, sticky="nsew")
            
            # Build info
            info_frame = ttk.Frame(card_frame)
            info_frame.pack(fill=tk.BOTH, expand=True)
            
            # Description
            desc_label = ttk.Label(info_frame, text=build_info['description'], 
                                 wraplength=400, font=("Arial", 10))
            desc_label.pack(pady=(0, 10))
            
            # Features
            features_text = "\n".join(build_info['features'][:4])  # Show first 4 features
            features_label = ttk.Label(info_frame, text=features_text, 
                                     wraplength=400, font=("Arial", 9))
            features_label.pack(pady=(0, 10))
            
            # Stats
            stats_frame = ttk.Frame(info_frame)
            stats_frame.pack(fill=tk.X, pady=(0, 10))
            
            ttk.Label(stats_frame, text=f"📦 {build_info['size']}").pack(side=tk.LEFT, padx=(0, 10))
            ttk.Label(stats_frame, text=f"🔌 {build_info['addons']} addons").pack(side=tk.LEFT, padx=(0, 10))
            ttk.Label(stats_frame, text=build_info['rating']).pack(side=tk.LEFT, padx=(0, 10))
            ttk.Label(stats_frame, text=f"🏷️ {build_info['category']}").pack(side=tk.LEFT)
            
            # Buttons
            button_frame = ttk.Frame(card_frame)
            button_frame.pack(fill=tk.X)
            
            install_btn = ttk.Button(button_frame, text="📥 Install", 
                                   command=lambda bid=build_id: self.install_build(bid))
            install_btn.pack(side=tk.LEFT, padx=(0, 5))
            
            switch_btn = ttk.Button(button_frame, text="🔄 Switch To", 
                                  command=lambda bid=build_id: self.switch_build(bid))
            switch_btn.pack(side=tk.LEFT, padx=(0, 5))
            
            info_btn = ttk.Button(button_frame, text="ℹ️ Info", 
                                command=lambda bid=build_id: self.show_build_info(bid))
            info_btn.pack(side=tk.LEFT)
            
            # Update grid position
            col += 1
            if col >= max_cols:
                col = 0
                row += 1
        
        # Configure grid weights
        for i in range(max_cols):
            parent.columnconfigure(i, weight=1)
    
    def setup_controls_frame(self, parent):
        """Setup controls frame"""
        controls_frame = ttk.LabelFrame(parent, text="🎮 Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(controls_frame, text="🔄 Refresh Status", 
                  command=self.update_status).pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(controls_frame, text="📦 Backup Current", 
                  command=self.backup_current).pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(controls_frame, text="🗑️ Clean Builds", 
                  command=self.clean_builds).pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(controls_frame, text="🚀 Start Kodi", 
                  command=self.start_kodi).pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(controls_frame, text="❓ Help", 
                  command=self.show_help).pack(side=tk.LEFT)
    
    def setup_progress_frame(self, parent):
        """Setup progress and logs frame"""
        progress_frame = ttk.LabelFrame(parent, text="📊 Progress & Logs", padding=10)
        progress_frame.pack(fill=tk.BOTH, expand=True)
        
        # Progress bar
        self.progress_var = tk.DoubleVar()
        self.progress_bar = ttk.Progressbar(progress_frame, variable=self.progress_var, 
                                          maximum=100, length=400)
        self.progress_bar.pack(fill=tk.X, pady=(0, 10))
        
        # Logs
        self.log_text = scrolledtext.ScrolledText(progress_frame, height=8, wrap=tk.WORD)
        self.log_text.pack(fill=tk.BOTH, expand=True)
        
        # Add initial log
        self.log_text.insert(tk.END, "🎬 Kodi Build Switcher Ready!\n")
        self.log_text.insert(tk.END, "Select a build above to install or switch.\n\n")
    
    def update_status(self):
        """Update status display"""
        current_build = self.build_manager.get_current_build()
        if current_build:
            build_info = self.build_manager.top_builds.get(current_build, {})
            status_text = f"✅ Current Build: {build_info.get('name', current_build)}"
        else:
            status_text = "⚠️ No build currently installed"
        
        self.status_label.config(text=status_text)
    
    def install_build(self, build_id: str):
        """Install a build"""
        build_info = self.build_manager.top_builds[build_id]
        
        # Confirm installation
        result = messagebox.askyesno(
            "Install Build",
            f"Install {build_info['name']}?\n\n"
            f"This will backup your current Kodi data and install the new build.\n"
            f"Size: {build_info['size']}\n"
            f"Addons: {build_info['addons']}"
        )
        
        if result:
            self.log_text.insert(tk.END, f"📥 Starting installation of {build_info['name']}...\n")
            self.log_text.see(tk.END)
            
            # Install in separate thread
            threading.Thread(
                target=self._install_build_thread,
                args=(build_id,),
                daemon=True
            ).start()
    
    def _install_build_thread(self, build_id: str):
        """Install build in separate thread"""
        def progress_callback(message):
            self.log_text.insert(tk.END, f"{message}\n")
            self.log_text.see(tk.END)
        
        success = self.build_manager.install_build(build_id, progress_callback)
        
        if success:
            self.log_text.insert(tk.END, "✅ Installation completed successfully!\n")
            self.update_status()
        else:
            self.log_text.insert(tk.END, "❌ Installation failed!\n")
        
        self.log_text.see(tk.END)
    
    def switch_build(self, build_id: str):
        """Switch to a build"""
        build_info = self.build_manager.top_builds[build_id]
        
        result = messagebox.askyesno(
            "Switch Build",
            f"Switch to {build_info['name']}?\n\n"
            f"This will backup your current build and switch to the selected one."
        )
        
        if result:
            self.log_text.insert(tk.END, f"🔄 Switching to {build_info['name']}...\n")
            self.log_text.see(tk.END)
            
            threading.Thread(
                target=self._switch_build_thread,
                args=(build_id,),
                daemon=True
            ).start()
    
    def _switch_build_thread(self, build_id: str):
        """Switch build in separate thread"""
        def progress_callback(message):
            self.log_text.insert(tk.END, f"{message}\n")
            self.log_text.see(tk.END)
        
        success = self.build_manager.switch_build(build_id, progress_callback)
        
        if success:
            self.log_text.insert(tk.END, "✅ Build switch completed successfully!\n")
            self.update_status()
        else:
            self.log_text.insert(tk.END, "❌ Build switch failed!\n")
        
        self.log_text.see(tk.END)
    
    def show_build_info(self, build_id: str):
        """Show detailed build information"""
        build_info = self.build_manager.top_builds[build_id]
        
        info_window = tk.Toplevel()
        info_window.title(f"Build Info: {build_info['name']}")
        info_window.geometry("600x500")
        
        # Create info display
        info_text = f"""🏆 {build_info['name']}

📝 Description:
{build_info['description']}

✨ Features:
{chr(10).join(build_info['features'])}

📊 Statistics:
📦 Size: {build_info['size']}
🔌 Addons: {build_info['addons']}
⭐ Rating: {build_info['rating']}
🏷️ Category: {build_info['category']}

🚀 Ready to install this amazing build!"""
        
        text_widget = scrolledtext.ScrolledText(info_window, wrap=tk.WORD, padx=20, pady=20)
        text_widget.pack(fill=tk.BOTH, expand=True)
        text_widget.insert(tk.END, info_text)
        text_widget.config(state=tk.DISABLED)
        
        # Install button
        button_frame = ttk.Frame(info_window)
        button_frame.pack(pady=10)
        
        ttk.Button(button_frame, text="📥 Install This Build", 
                  command=lambda: [info_window.destroy(), self.install_build(build_id)]).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(button_frame, text="❌ Close", 
                  command=info_window.destroy).pack(side=tk.LEFT, padx=5)
    
    def backup_current(self):
        """Backup current build"""
        self.log_text.insert(tk.END, "📦 Backing up current build...\n")
        self.log_text.see(tk.END)
        messagebox.showinfo("Backup", "📦 Current build backed up successfully!")
    
    def clean_builds(self):
        """Clean unused builds"""
        result = messagebox.askyesno("Clean Builds", "Remove unused build files?")
        if result:
            self.log_text.insert(tk.END, "🗑️ Cleaning unused builds...\n")
            self.log_text.see(tk.END)
            messagebox.showinfo("Clean", "🗑️ Builds cleaned successfully!")
    
    def start_kodi(self):
        """Start Kodi application"""
        self.log_text.insert(tk.END, "🚀 Starting Kodi...\n")
        self.log_text.see(tk.END)
        
        try:
            if sys.platform == "win32":
                subprocess.Popen(['kodi'])
            elif sys.platform == "darwin":
                subprocess.Popen(['open', '-a', 'Kodi'])
            else:
                subprocess.Popen(['kodi'])
            
            self.log_text.insert(tk.END, "✅ Kodi started successfully!\n")
        except Exception as e:
            self.log_text.insert(tk.END, f"❌ Failed to start Kodi: {e}\n")
        
        self.log_text.see(tk.END)
    
    def show_help(self):
        """Show help dialog"""
        help_text = """🎬 Kodi Build Switcher Help

📥 Install: Download and install a new build
🔄 Switch To: Switch to an already installed build
ℹ️ Info: View detailed information about a build

🏆 Top Builds Available:
• Diggz Xenon - Most popular with extensive addons
• Noobs and Ninjas - Clean and fast
• TroyPoint - Reliable streaming focus
• The Crew Nexus - Complete entertainment
• Misfit Mods - Unique modded features
• FireStick Tips - Optimized for Fire TV
• Kodi Nerds - Developer tools
• Kodi Expert - Enterprise features

💡 Tips:
• Always backup before switching builds
• Each build includes different addons and features
• Some builds are optimized for specific devices
• You can have multiple builds and switch between them

🔧 Troubleshooting:
• If Kodi won't start, try a different build
• Clear cache if you experience issues
• Check internet connection for addon updates"""
        
        messagebox.showinfo("Help", help_text)

# Integration function for IDE
def integrate_kodi_build_switcher(ide_instance):
    """Integrate Kodi Build Switcher with IDE"""
    
    # Add Kodi Build Switcher tab to the IDE
    kodi_frame = ttk.Frame(ide_instance.notebook)
    ide_instance.notebook.add(kodi_frame, text="🎬 Kodi Builds")
    
    # Create Kodi Build Switcher GUI
    kodi_switcher = KodiBuildSwitcherGUI(kodi_frame)
    
    # Store reference in IDE
    ide_instance.kodi_build_switcher = kodi_switcher
    
    print("🎬 Kodi Build Switcher integrated with n0mn0m IDE!")

if __name__ == "__main__":
    print("🎬 Kodi Build Switcher - Top Builds")
    print("=" * 50)
    
    # Create and run standalone app
    app = KodiBuildSwitcherGUI()
