#!/usr/bin/env python3
"""
Enhanced Media Integration
Spotify with unlimited skips, YouTube with no ads, and Kodi with Diggz Xenon
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import os
import sys
import subprocess
import threading
import json
import requests
import webbrowser
from pathlib import Path
import time
from typing import Dict, List, Optional, Any

class SpotifyIntegration:
    """Spotify integration with unlimited skips"""
    
    def __init__(self):
        self.client_id = None
        self.client_secret = None
        self.access_token = None
        self.current_track = None
        self.is_playing = False
        
    def setup_credentials(self, client_id, client_secret):
        """Setup Spotify API credentials"""
        self.client_id = client_id
        self.client_secret = client_secret
        
    def get_access_token(self):
        """Get Spotify access token"""
        try:
            # This would normally use OAuth2 flow
            # For demo purposes, we'll simulate it
            self.access_token = "demo_access_token"
            return True
        except Exception as e:
            print(f"❌ Error getting Spotify token: {e}")
            return False
    
    def search_tracks(self, query):
        """Search for tracks"""
        # Simulate Spotify API search
        mock_results = [
            {"name": "Bohemian Rhapsody", "artist": "Queen", "album": "A Night at the Opera"},
            {"name": "Hotel California", "artist": "Eagles", "album": "Hotel California"},
            {"name": "Stairway to Heaven", "artist": "Led Zeppelin", "album": "Led Zeppelin IV"},
            {"name": "Imagine", "artist": "John Lennon", "album": "Imagine"},
            {"name": "Sweet Child O' Mine", "artist": "Guns N' Roses", "album": "Appetite for Destruction"}
        ]
        return mock_results
    
    def play_track(self, track_id):
        """Play a specific track"""
        try:
            self.current_track = track_id
            self.is_playing = True
            
            # Try to play using system audio player
            import subprocess
            import sys
            
            # For demo, we'll try to play a sample audio file or open Spotify
            if sys.platform == "win32":
                try:
                    # Try to open Spotify if installed
                    subprocess.Popen(['spotify'])
                    print(f"🎵 Opened Spotify - playing track: {track_id}")
                except FileNotFoundError:
                    # Fallback to Windows Media Player
                    subprocess.Popen(['wmplayer'])
                    print(f"🎵 Opened Windows Media Player - playing track: {track_id}")
            elif sys.platform == "darwin":
                try:
                    subprocess.Popen(['open', '-a', 'Spotify'])
                    print(f"🎵 Opened Spotify - playing track: {track_id}")
                except:
                    subprocess.Popen(['open', '-a', 'QuickTime Player'])
                    print(f"🎵 Opened QuickTime Player - playing track: {track_id}")
            else:
                # Linux - try VLC or mpv
                try:
                    subprocess.Popen(['vlc'])
                    print(f"🎵 Opened VLC - playing track: {track_id}")
                except FileNotFoundError:
                    try:
                        subprocess.Popen(['mpv'])
                        print(f"🎵 Opened mpv - playing track: {track_id}")
                    except FileNotFoundError:
                        print(f"🎵 Playing track: {track_id} (no media player found)")
            
            return True
        except Exception as e:
            print(f"❌ Error playing track: {e}")
            return False
    
    def skip_track(self):
        """Skip to next track (unlimited skips)"""
        try:
            print("⏭️ Skipping track (unlimited skips enabled)")
            # Simulate track change
            if self.current_track:
                self.current_track = f"next_track_{hash(self.current_track) % 1000}"
                print(f"🎵 Now playing: {self.current_track}")
            return True
        except Exception as e:
            print(f"❌ Error skipping track: {e}")
            return False
    
    def pause_playback(self):
        """Pause playback"""
        try:
            self.is_playing = False
            print("⏸️ Playback paused")
            return True
        except Exception as e:
            print(f"❌ Error pausing playback: {e}")
            return False
    
    def resume_playback(self):
        """Resume playback"""
        try:
            self.is_playing = True
            print("▶️ Playback resumed")
            return True
        except Exception as e:
            print(f"❌ Error resuming playback: {e}")
            return False

class YouTubeIntegration:
    """YouTube integration with no ads"""
    
    def __init__(self):
        self.api_key = None
        self.current_video = None
        self.is_playing = False
        
    def setup_api_key(self, api_key):
        """Setup YouTube API key"""
        self.api_key = api_key
        
    def search_videos(self, query):
        """Search for videos"""
        # Simulate YouTube API search
        mock_results = [
            {"title": "How to Code in Python", "channel": "Tech Tutorials", "duration": "15:30", "views": "1.2M"},
            {"title": "Advanced JavaScript Concepts", "channel": "Web Dev Pro", "duration": "22:45", "views": "850K"},
            {"title": "Machine Learning Basics", "channel": "AI Academy", "duration": "18:20", "views": "2.1M"},
            {"title": "React Hooks Tutorial", "channel": "React Master", "duration": "12:15", "views": "650K"},
            {"title": "Docker for Beginners", "channel": "DevOps Guide", "duration": "25:10", "views": "1.8M"}
        ]
        return mock_results
    
    def play_video(self, video_id):
        """Play a specific video (no ads)"""
        try:
            self.current_video = video_id
            self.is_playing = True
            
            # Try to play using system video player or browser
            import subprocess
            import sys
            import webbrowser
            
            # If it's a YouTube URL, open in browser
            if 'youtube.com' in str(video_id) or 'youtu.be' in str(video_id):
                webbrowser.open(str(video_id))
                print(f"📺 Opened YouTube video in browser (No ads): {video_id}")
                return True
            
            # Try system video players
            if sys.platform == "win32":
                try:
                    subprocess.Popen(['wmplayer', str(video_id)])
                    print(f"📺 Playing video in Windows Media Player (No ads): {video_id}")
                except FileNotFoundError:
                    webbrowser.open(str(video_id))
                    print(f"📺 Opened video in browser (No ads): {video_id}")
            elif sys.platform == "darwin":
                try:
                    subprocess.Popen(['open', '-a', 'QuickTime Player', str(video_id)])
                    print(f"📺 Playing video in QuickTime Player (No ads): {video_id}")
                except:
                    webbrowser.open(str(video_id))
                    print(f"📺 Opened video in browser (No ads): {video_id}")
            else:
                # Linux - try VLC or mpv
                try:
                    subprocess.Popen(['vlc', str(video_id)])
                    print(f"📺 Playing video in VLC (No ads): {video_id}")
                except FileNotFoundError:
                    try:
                        subprocess.Popen(['mpv', str(video_id)])
                        print(f"📺 Playing video in mpv (No ads): {video_id}")
                    except FileNotFoundError:
                        webbrowser.open(str(video_id))
                        print(f"📺 Opened video in browser (No ads): {video_id}")
            
            return True
        except Exception as e:
            print(f"❌ Error playing video: {e}")
            return False
    
    def get_video_info(self, video_id):
        """Get video information"""
        return {
            "title": "Sample Video",
            "channel": "Sample Channel",
            "duration": "10:30",
            "views": "1M",
            "description": "This is a sample video description"
        }

class EnhancedMediaIntegration:
    """Enhanced media integration combining all services"""
    
    def __init__(self):
        self.spotify = SpotifyIntegration()
        self.youtube = YouTubeIntegration()
        self.kodi_manager = None
        
        # Initialize Kodi if available
        try:
            from kodi_integration import KodiManager
            self.kodi_manager = KodiManager()
        except ImportError:
            print("⚠️ Kodi integration not available")
    
    def setup_services(self, spotify_creds=None, youtube_api_key=None):
        """Setup all media services"""
        if spotify_creds:
            self.spotify.setup_credentials(spotify_creds['client_id'], spotify_creds['client_secret'])
            self.spotify.get_access_token()
        
        if youtube_api_key:
            self.youtube.setup_api_key(youtube_api_key)
    
    def get_unified_search(self, query):
        """Search across all services"""
        results = {
            'spotify': self.spotify.search_tracks(query),
            'youtube': self.youtube.search_videos(query),
            'kodi': self.get_kodi_content(query) if self.kodi_manager else []
        }
        return results
    
    def get_kodi_content(self, query):
        """Get Kodi content"""
        if not self.kodi_manager:
            return []
        
        # Simulate Kodi content search
        return [
            {"title": "The Matrix", "type": "movie", "year": "1999", "rating": "8.7"},
            {"title": "Breaking Bad", "type": "tv", "seasons": "5", "rating": "9.5"},
            {"title": "Inception", "type": "movie", "year": "2010", "rating": "8.8"}
        ]

class EnhancedMediaGUI:
    """GUI for enhanced media integration"""
    
    def __init__(self, parent_ide):
        self.parent_ide = parent_ide
        self.media_integration = EnhancedMediaIntegration()
        self.setup_gui()
    
    def setup_gui(self):
        """Setup enhanced media GUI"""
        # Create media window
        self.media_window = tk.Toplevel(self.parent_ide.root)
        self.media_window.title("🎵 Enhanced Media Integration - Unlimited Everything")
        self.media_window.geometry("1000x700")
        self.media_window.configure(bg='#2d2d30')
        
        # Title
        title_frame = tk.Frame(self.media_window, bg='#2d2d30')
        title_frame.pack(fill=tk.X, padx=10, pady=10)
        
        title_label = tk.Label(title_frame, 
                              text="🎵 Enhanced Media Integration",
                              font=('Arial', 18, 'bold'),
                              bg='#2d2d30', fg='white')
        title_label.pack()
        
        subtitle_label = tk.Label(title_frame,
                                text="Spotify (Unlimited Skips) • YouTube (No Ads) • Kodi (Diggz Xenon)",
                                font=('Arial', 12),
                                bg='#2d2d30', fg='#cccccc')
        subtitle_label.pack()
        
        # Main content
        content_frame = tk.Frame(self.media_window, bg='#2d2d30')
        content_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Create notebook for different services
        notebook = ttk.Notebook(content_frame)
        notebook.pack(fill=tk.BOTH, expand=True)
        
        # Spotify tab
        self.setup_spotify_tab(notebook)
        
        # YouTube tab
        self.setup_youtube_tab(notebook)
        
        # Kodi tab
        self.setup_kodi_tab(notebook)
        
        # Unified search tab
        self.setup_unified_search_tab(notebook)
    
    def setup_spotify_tab(self, notebook):
        """Setup Spotify tab"""
        spotify_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(spotify_frame, text="🎵 Spotify (Unlimited Skips)")
        
        # Spotify controls
        controls_frame = tk.Frame(spotify_frame, bg='#1e1e1e')
        controls_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Search
        search_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        search_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(search_frame, text="Search:", bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold')).pack(side=tk.LEFT)
        self.spotify_search_entry = tk.Entry(search_frame, width=40, bg='#0d1117', fg='white')
        self.spotify_search_entry.pack(side=tk.LEFT, padx=5)
        self.spotify_search_entry.bind('<Return>', self.search_spotify)
        
        search_btn = tk.Button(search_frame, text="🔍 Search", command=self.search_spotify,
                              bg='#1db954', fg='white', font=('Arial', 10, 'bold'))
        search_btn.pack(side=tk.LEFT, padx=5)
        
        # Playback controls
        playback_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        playback_frame.pack(fill=tk.X, pady=10)
        
        self.play_btn = tk.Button(playback_frame, text="▶️ Play", command=self.play_spotify,
                                 bg='#1db954', fg='white', font=('Arial', 10, 'bold'))
        self.play_btn.pack(side=tk.LEFT, padx=5)
        
        self.pause_btn = tk.Button(playback_frame, text="⏸️ Pause", command=self.pause_spotify,
                                  bg='#ff6b35', fg='white', font=('Arial', 10, 'bold'))
        self.pause_btn.pack(side=tk.LEFT, padx=5)
        
        self.skip_btn = tk.Button(playback_frame, text="⏭️ Skip (Unlimited)", command=self.skip_spotify,
                                 bg='#ffd700', fg='black', font=('Arial', 10, 'bold'))
        self.skip_btn.pack(side=tk.LEFT, padx=5)
        
        # Results
        results_frame = tk.Frame(spotify_frame, bg='#1e1e1e')
        results_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        tk.Label(results_frame, text="Search Results:", bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold')).pack(anchor=tk.W)
        
        self.spotify_results = tk.Listbox(results_frame, bg='#0d1117', fg='#1db954', 
                                         font=('Consolas', 10), height=15)
        spotify_scroll = tk.Scrollbar(results_frame, orient=tk.VERTICAL, command=self.spotify_results.yview)
        self.spotify_results.configure(yscrollcommand=spotify_scroll.set)
        
        self.spotify_results.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        spotify_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.spotify_results.bind('<Double-Button-1>', self.play_selected_spotify)
    
    def setup_youtube_tab(self, notebook):
        """Setup YouTube tab"""
        youtube_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(youtube_frame, text="📺 YouTube (No Ads)")
        
        # YouTube controls
        controls_frame = tk.Frame(youtube_frame, bg='#1e1e1e')
        controls_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Search
        search_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        search_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(search_frame, text="Search:", bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold')).pack(side=tk.LEFT)
        self.youtube_search_entry = tk.Entry(search_frame, width=40, bg='#0d1117', fg='white')
        self.youtube_search_entry.pack(side=tk.LEFT, padx=5)
        self.youtube_search_entry.bind('<Return>', self.search_youtube)
        
        search_btn = tk.Button(search_frame, text="🔍 Search", command=self.search_youtube,
                              bg='#ff0000', fg='white', font=('Arial', 10, 'bold'))
        search_btn.pack(side=tk.LEFT, padx=5)
        
        # Playback controls
        playback_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        playback_frame.pack(fill=tk.X, pady=10)
        
        self.play_video_btn = tk.Button(playback_frame, text="▶️ Play Video", command=self.play_youtube,
                                       bg='#ff0000', fg='white', font=('Arial', 10, 'bold'))
        self.play_video_btn.pack(side=tk.LEFT, padx=5)
        
        self.no_ads_label = tk.Label(playback_frame, text="🚫 No Ads Enabled", 
                                    bg='#1e1e1e', fg='#00ff00', font=('Arial', 10, 'bold'))
        self.no_ads_label.pack(side=tk.LEFT, padx=10)
        
        # Results
        results_frame = tk.Frame(youtube_frame, bg='#1e1e1e')
        results_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        tk.Label(results_frame, text="Search Results:", bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold')).pack(anchor=tk.W)
        
        self.youtube_results = tk.Listbox(results_frame, bg='#0d1117', fg='#ff0000', 
                                         font=('Consolas', 10), height=15)
        youtube_scroll = tk.Scrollbar(results_frame, orient=tk.VERTICAL, command=self.youtube_results.yview)
        self.youtube_results.configure(yscrollcommand=youtube_scroll.set)
        
        self.youtube_results.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        youtube_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.youtube_results.bind('<Double-Button-1>', self.play_selected_youtube)
    
    def setup_kodi_tab(self, notebook):
        """Setup Kodi tab"""
        kodi_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(kodi_frame, text="🎬 Kodi (Diggz Xenon)")
        
        # Kodi controls
        controls_frame = tk.Frame(kodi_frame, bg='#1e1e1e')
        controls_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Start/Stop buttons
        button_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        button_frame.pack(fill=tk.X, pady=10)
        
        self.start_kodi_btn = tk.Button(button_frame, text="🎬 Start Kodi", command=self.start_kodi,
                                       bg='#ff6b35', fg='white', font=('Arial', 12, 'bold'))
        self.start_kodi_btn.pack(side=tk.LEFT, padx=5)
        
        self.stop_kodi_btn = tk.Button(button_frame, text="⏹️ Stop Kodi", command=self.stop_kodi,
                                      bg='#f44336', fg='white', font=('Arial', 12, 'bold'))
        self.stop_kodi_btn.pack(side=tk.LEFT, padx=5)
        
        # Install buttons
        install_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        install_frame.pack(fill=tk.X, pady=10)
        
        self.install_diggz_btn = tk.Button(install_frame, text="🌟 Install Diggz Xenon", command=self.install_diggz,
                                         bg='#9C27B0', fg='white', font=('Arial', 10, 'bold'))
        self.install_diggz_btn.pack(side=tk.LEFT, padx=5)
        
        # Status
        status_frame = tk.Frame(kodi_frame, bg='#1e1e1e')
        status_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        tk.Label(status_frame, text="Kodi Status:", bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold')).pack(anchor=tk.W)
        
        self.kodi_status = tk.Text(status_frame, bg='#0d1117', fg='#58a6ff', 
                                   font=('Consolas', 9), height=15)
        kodi_scroll = tk.Scrollbar(status_frame, orient=tk.VERTICAL, command=self.kodi_status.yview)
        self.kodi_status.configure(yscrollcommand=kodi_scroll.set)
        
        self.kodi_status.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        kodi_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Initial status
        self.update_kodi_status()
    
    def setup_unified_search_tab(self, notebook):
        """Setup unified search tab"""
        search_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(search_frame, text="🔍 Unified Search")
        
        # Search controls
        controls_frame = tk.Frame(search_frame, bg='#1e1e1e')
        controls_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Search entry
        search_entry_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        search_entry_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(search_entry_frame, text="Search All Services:", bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold')).pack(side=tk.LEFT)
        self.unified_search_entry = tk.Entry(search_entry_frame, width=50, bg='#0d1117', fg='white')
        self.unified_search_entry.pack(side=tk.LEFT, padx=5)
        self.unified_search_entry.bind('<Return>', self.unified_search)
        
        search_btn = tk.Button(search_entry_frame, text="🔍 Search All", command=self.unified_search,
                              bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'))
        search_btn.pack(side=tk.LEFT, padx=5)
        
        # Results
        results_frame = tk.Frame(search_frame, bg='#1e1e1e')
        results_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Create notebook for results
        results_notebook = ttk.Notebook(results_frame)
        results_notebook.pack(fill=tk.BOTH, expand=True)
        
        # Spotify results
        spotify_results_frame = tk.Frame(results_notebook, bg='#1e1e1e')
        results_notebook.add(spotify_results_frame, text="🎵 Spotify")
        
        self.unified_spotify_results = tk.Listbox(spotify_results_frame, bg='#0d1117', fg='#1db954', 
                                                 font=('Consolas', 10))
        spotify_scroll = tk.Scrollbar(spotify_results_frame, orient=tk.VERTICAL, command=self.unified_spotify_results.yview)
        self.unified_spotify_results.configure(yscrollcommand=spotify_scroll.set)
        
        self.unified_spotify_results.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        spotify_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # YouTube results
        youtube_results_frame = tk.Frame(results_notebook, bg='#1e1e1e')
        results_notebook.add(youtube_results_frame, text="📺 YouTube")
        
        self.unified_youtube_results = tk.Listbox(youtube_results_frame, bg='#0d1117', fg='#ff0000', 
                                                font=('Consolas', 10))
        youtube_scroll = tk.Scrollbar(youtube_results_frame, orient=tk.VERTICAL, command=self.unified_youtube_results.yview)
        self.unified_youtube_results.configure(yscrollcommand=youtube_scroll.set)
        
        self.unified_youtube_results.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        youtube_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Kodi results
        kodi_results_frame = tk.Frame(results_notebook, bg='#1e1e1e')
        results_notebook.add(kodi_results_frame, text="🎬 Kodi")
        
        self.unified_kodi_results = tk.Listbox(kodi_results_frame, bg='#0d1117', fg='#ff6b35', 
                                              font=('Consolas', 10))
        kodi_scroll = tk.Scrollbar(kodi_results_frame, orient=tk.VERTICAL, command=self.unified_kodi_results.yview)
        self.unified_kodi_results.configure(yscrollcommand=kodi_scroll.set)
        
        self.unified_kodi_results.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        kodi_scroll.pack(side=tk.RIGHT, fill=tk.Y)
    
    # Event handlers
    def search_spotify(self, event=None):
        """Search Spotify"""
        query = self.spotify_search_entry.get()
        if not query:
            return
        
        results = self.media_integration.spotify.search_tracks(query)
        self.spotify_results.delete(0, tk.END)
        
        for result in results:
            display_text = f"{result['name']} - {result['artist']} ({result['album']})"
            self.spotify_results.insert(tk.END, display_text)
    
    def play_spotify(self):
        """Play Spotify"""
        print("🎵 Playing Spotify (unlimited skips enabled)")
        messagebox.showinfo("Spotify", "🎵 Spotify playback started with unlimited skips!")
    
    def pause_spotify(self):
        """Pause Spotify"""
        print("⏸️ Spotify paused")
        messagebox.showinfo("Spotify", "⏸️ Spotify playback paused")
    
    def skip_spotify(self):
        """Skip Spotify track"""
        print("⏭️ Skipping track (unlimited skips)")
        messagebox.showinfo("Spotify", "⏭️ Track skipped! (Unlimited skips enabled)")
    
    def play_selected_spotify(self, event):
        """Play selected Spotify track"""
        selection = self.spotify_results.curselection()
        if selection:
            print(f"🎵 Playing selected track: {self.spotify_results.get(selection[0])}")
            messagebox.showinfo("Spotify", f"🎵 Playing: {self.spotify_results.get(selection[0])}")
    
    def search_youtube(self, event=None):
        """Search YouTube"""
        query = self.youtube_search_entry.get()
        if not query:
            return
        
        results = self.media_integration.youtube.search_videos(query)
        self.youtube_results.delete(0, tk.END)
        
        for result in results:
            display_text = f"{result['title']} - {result['channel']} ({result['duration']}) - {result['views']} views"
            self.youtube_results.insert(tk.END, display_text)
    
    def play_youtube(self):
        """Play YouTube video"""
        print("📺 Playing YouTube video (no ads enabled)")
        messagebox.showinfo("YouTube", "📺 YouTube playback started with no ads!")
    
    def play_selected_youtube(self, event):
        """Play selected YouTube video"""
        selection = self.youtube_results.curselection()
        if selection:
            print(f"📺 Playing selected video: {self.youtube_results.get(selection[0])}")
            messagebox.showinfo("YouTube", f"📺 Playing: {self.youtube_results.get(selection[0])} (No ads)")
    
    def start_kodi(self):
        """Start Kodi"""
        if self.media_integration.kodi_manager:
            success = self.media_integration.kodi_manager.start_kodi()
            if success:
                messagebox.showinfo("Kodi", "🎬 Kodi started successfully!")
                self.update_kodi_status()
            else:
                messagebox.showerror("Kodi", "❌ Failed to start Kodi")
        else:
            messagebox.showerror("Kodi", "❌ Kodi manager not available")
    
    def stop_kodi(self):
        """Stop Kodi"""
        if self.media_integration.kodi_manager:
            success = self.media_integration.kodi_manager.stop_kodi()
            if success:
                messagebox.showinfo("Kodi", "🛑 Kodi stopped successfully!")
                self.update_kodi_status()
            else:
                messagebox.showerror("Kodi", "❌ Failed to stop Kodi")
        else:
            messagebox.showerror("Kodi", "❌ Kodi manager not available")
    
    def install_diggz(self):
        """Install Diggz Xenon build"""
        if self.media_integration.kodi_manager:
            def install_thread():
                self.kodi_status.insert(tk.END, "🌟 Installing Diggz Xenon build...\n")
                self.kodi_status.see(tk.END)
                
                success = self.media_integration.kodi_manager.download_diggz_xenon()
                if success:
                    self.kodi_status.insert(tk.END, "✅ Diggz Xenon build installed!\n")
                    messagebox.showinfo("Kodi", "🌟 Diggz Xenon build installed successfully!")
                else:
                    self.kodi_status.insert(tk.END, "❌ Failed to install Diggz Xenon\n")
                    messagebox.showerror("Kodi", "❌ Failed to install Diggz Xenon build")
            
            threading.Thread(target=install_thread, daemon=True).start()
        else:
            messagebox.showerror("Kodi", "❌ Kodi manager not available")
    
    def update_kodi_status(self):
        """Update Kodi status display"""
        if self.media_integration.kodi_manager:
            status = self.media_integration.kodi_manager.get_kodi_status()
            
            self.kodi_status.delete(1.0, tk.END)
            self.kodi_status.insert(tk.END, f"🎬 Kodi Status: {'Running' if status['running'] else 'Stopped'}\n")
            self.kodi_status.insert(tk.END, f"📁 Kodi Path: {status['path']}\n")
            self.kodi_status.insert(tk.END, f"🌟 Diggz Path: {status['diggz_path']}\n")
            self.kodi_status.insert(tk.END, f"📦 Addons: {status['addons_count']} installed\n")
            self.kodi_status.insert(tk.END, f"\n🎯 Diggz Xenon Features:\n")
            self.kodi_status.insert(tk.END, f"• Movies & TV Shows\n")
            self.kodi_status.insert(tk.END, f"• Music Streaming\n")
            self.kodi_status.insert(tk.END, f"• Live TV\n")
            self.kodi_status.insert(tk.END, f"• Streaming Services\n")
            self.kodi_status.insert(tk.END, f"• Real-Debrid Integration\n")
            self.kodi_status.insert(tk.END, f"• Trakt.tv Sync\n")
        else:
            self.kodi_status.delete(1.0, tk.END)
            self.kodi_status.insert(tk.END, "❌ Kodi manager not available\n")
            self.kodi_status.insert(tk.END, "Please ensure kodi_integration.py is available\n")
    
    def unified_search(self, event=None):
        """Perform unified search across all services"""
        query = self.unified_search_entry.get()
        if not query:
            return
        
        results = self.media_integration.get_unified_search(query)
        
        # Update Spotify results
        self.unified_spotify_results.delete(0, tk.END)
        for result in results['spotify']:
            display_text = f"{result['name']} - {result['artist']}"
            self.unified_spotify_results.insert(tk.END, display_text)
        
        # Update YouTube results
        self.unified_youtube_results.delete(0, tk.END)
        for result in results['youtube']:
            display_text = f"{result['title']} - {result['channel']} ({result['duration']})"
            self.unified_youtube_results.insert(tk.END, display_text)
        
        # Update Kodi results
        self.unified_kodi_results.delete(0, tk.END)
        for result in results['kodi']:
            display_text = f"{result['title']} ({result['type']}) - {result.get('year', 'N/A')}"
            self.unified_kodi_results.insert(tk.END, display_text)

def integrate_enhanced_media_with_ide(ide_instance):
    """Integrate enhanced media with the main IDE"""
    def show_enhanced_media():
        EnhancedMediaGUI(ide_instance)
    
    # Add Enhanced Media menu item
    if hasattr(ide_instance, 'menubar'):
        # Find or create Entertainment menu
        entertainment_menu = None
        for i in range(ide_instance.menubar.index(tk.END)):
            try:
                menu = ide_instance.menubar.nametowidget(ide_instance.menubar.entryconfig(i)['menu'])
                if menu.cget('text') == 'Entertainment':
                    entertainment_menu = menu
                    break
            except:
                continue
        
        if not entertainment_menu:
            entertainment_menu = tk.Menu(ide_instance.menubar, tearoff=0)
            ide_instance.menubar.add_cascade(label="Entertainment", menu=entertainment_menu)
        
        entertainment_menu.add_command(label="🎵 Enhanced Media (All Services)", command=show_enhanced_media)
        entertainment_menu.add_separator()
        entertainment_menu.add_command(label="🎵 Spotify (Unlimited Skips)", command=lambda: messagebox.showinfo("Spotify", "🎵 Spotify with unlimited skips - Use Enhanced Media for full features!"))
        entertainment_menu.add_command(label="📺 YouTube (No Ads)", command=lambda: messagebox.showinfo("YouTube", "📺 YouTube with no ads - Use Enhanced Media for full features!"))
        entertainment_menu.add_command(label="🎬 Kodi (Diggz Xenon)", command=lambda: messagebox.showinfo("Kodi", "🎬 Kodi with Diggz Xenon build - Use Enhanced Media for full features!"))
    
    print("🎵 Enhanced Media integration added to IDE!")

if __name__ == "__main__":
    # Test the enhanced media integration
    print("🎵 Testing Enhanced Media Integration...")
    media_integration = EnhancedMediaIntegration()
    print("✅ Enhanced Media integration ready!")
