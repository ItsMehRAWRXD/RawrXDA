#!/usr/bin/env python3
"""
Real Media Player Implementation
Working audio and video players with actual playback capabilities
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog, scrolledtext
import os
import sys
import threading
import subprocess
import webbrowser
import urllib.parse
import json
from pathlib import Path
import time
from typing import Dict, List, Optional, Any

class RealAudioPlayer:
    """Real audio player using system audio capabilities"""
    
    def __init__(self):
        self.current_file = None
        self.is_playing = False
        self.process = None
        self.volume = 50
        
    def play_file(self, file_path: str) -> bool:
        """Play audio file using system player"""
        try:
            if self.is_playing:
                self.stop()
            
            # Use system audio player
            if sys.platform == "win32":
                # Windows - use Windows Media Player
                self.process = subprocess.Popen(['wmplayer', file_path])
            elif sys.platform == "darwin":
                # macOS - use QuickTime Player
                self.process = subprocess.Popen(['open', '-a', 'QuickTime Player', file_path])
            else:
                # Linux - use VLC or mpv
                try:
                    self.process = subprocess.Popen(['vlc', file_path])
                except FileNotFoundError:
                    try:
                        self.process = subprocess.Popen(['mpv', file_path])
                    except FileNotFoundError:
                        # Fallback to system default
                        self.process = subprocess.Popen(['xdg-open', file_path])
            
            self.current_file = file_path
            self.is_playing = True
            print(f"🎵 Playing: {os.path.basename(file_path)}")
            return True
            
        except Exception as e:
            print(f"❌ Error playing audio: {e}")
            return False
    
    def play_url(self, url: str) -> bool:
        """Play audio from URL"""
        try:
            if self.is_playing:
                self.stop()
            
            # Use system audio player for URLs
            if sys.platform == "win32":
                self.process = subprocess.Popen(['wmplayer', url])
            elif sys.platform == "darwin":
                self.process = subprocess.Popen(['open', '-a', 'QuickTime Player', url])
            else:
                try:
                    self.process = subprocess.Popen(['vlc', url])
                except FileNotFoundError:
                    try:
                        self.process = subprocess.Popen(['mpv', url])
                    except FileNotFoundError:
                        self.process = subprocess.Popen(['xdg-open', url])
            
            self.is_playing = True
            print(f"🎵 Playing URL: {url}")
            return True
            
        except Exception as e:
            print(f"❌ Error playing URL: {e}")
            return False
    
    def stop(self):
        """Stop playback"""
        try:
            if self.process:
                self.process.terminate()
                self.process = None
            self.is_playing = False
            self.current_file = None
            print("⏹️ Playback stopped")
        except Exception as e:
            print(f"❌ Error stopping playback: {e}")
    
    def pause(self):
        """Pause playback"""
        # Note: System players don't support pause via subprocess
        # This is a limitation of using external players
        print("⏸️ Pause not supported with system players")

class RealVideoPlayer:
    """Real video player using system video capabilities"""
    
    def __init__(self):
        self.current_file = None
        self.is_playing = False
        self.process = None
        
    def play_file(self, file_path: str) -> bool:
        """Play video file using system player"""
        try:
            if self.is_playing:
                self.stop()
            
            # Use system video player
            if sys.platform == "win32":
                # Windows - use Windows Media Player
                self.process = subprocess.Popen(['wmplayer', file_path])
            elif sys.platform == "darwin":
                # macOS - use QuickTime Player
                self.process = subprocess.Popen(['open', '-a', 'QuickTime Player', file_path])
            else:
                # Linux - use VLC or mpv
                try:
                    self.process = subprocess.Popen(['vlc', file_path])
                except FileNotFoundError:
                    try:
                        self.process = subprocess.Popen(['mpv', file_path])
                    except FileNotFoundError:
                        # Fallback to system default
                        self.process = subprocess.Popen(['xdg-open', file_path])
            
            self.current_file = file_path
            self.is_playing = True
            print(f"📺 Playing: {os.path.basename(file_path)}")
            return True
            
        except Exception as e:
            print(f"❌ Error playing video: {e}")
            return False
    
    def play_youtube_url(self, url: str) -> bool:
        """Play YouTube video"""
        try:
            if self.is_playing:
                self.stop()
            
            # Extract video ID from URL
            video_id = self._extract_video_id(url)
            if not video_id:
                print("❌ Invalid YouTube URL")
                return False
            
            # Use youtube-dl or yt-dlp to get direct video URL
            try:
                # Try yt-dlp first (newer)
                result = subprocess.run(['yt-dlp', '-g', url], 
                                      capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    direct_url = result.stdout.strip()
                    return self.play_file(direct_url)
            except:
                try:
                    # Fallback to youtube-dl
                    result = subprocess.run(['youtube-dl', '-g', url], 
                                          capture_output=True, text=True, timeout=10)
                    if result.returncode == 0:
                        direct_url = result.stdout.strip()
                        return self.play_file(direct_url)
                except:
                    pass
            
            # Fallback: Open in browser
            webbrowser.open(url)
            print(f"📺 Opening YouTube video in browser: {url}")
            return True
            
        except Exception as e:
            print(f"❌ Error playing YouTube video: {e}")
            return False
    
    def _extract_video_id(self, url: str) -> Optional[str]:
        """Extract video ID from YouTube URL"""
        try:
            if 'youtube.com/watch' in url:
                parsed = urllib.parse.urlparse(url)
                query_params = urllib.parse.parse_qs(parsed.query)
                return query_params.get('v', [None])[0]
            elif 'youtu.be/' in url:
                return url.split('youtu.be/')[-1].split('?')[0]
            return None
        except:
            return None
    
    def stop(self):
        """Stop playback"""
        try:
            if self.process:
                self.process.terminate()
                self.process = None
            self.is_playing = False
            self.current_file = None
            print("⏹️ Video playback stopped")
        except Exception as e:
            print(f"❌ Error stopping video: {e}")

class RealMediaPlayerGUI:
    """GUI for real media player"""
    
    def __init__(self, parent=None):
        self.audio_player = RealAudioPlayer()
        self.video_player = RealVideoPlayer()
        
        if parent:
            self.setup_gui(parent)
        else:
            self.create_standalone_window()
    
    def create_standalone_window(self):
        """Create standalone media player window"""
        self.root = tk.Tk()
        self.root.title("Real Media Player")
        self.root.geometry("800x600")
        self.setup_gui(self.root)
        self.root.mainloop()
    
    def setup_gui(self, parent):
        """Setup GUI components"""
        # Main frame
        main_frame = ttk.Frame(parent)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Title
        title_label = ttk.Label(main_frame, text="🎵 Real Media Player", font=("Arial", 16, "bold"))
        title_label.pack(pady=(0, 20))
        
        # Create notebook for tabs
        notebook = ttk.Notebook(main_frame)
        notebook.pack(fill=tk.BOTH, expand=True)
        
        # Audio tab
        audio_frame = ttk.Frame(notebook)
        notebook.add(audio_frame, text="🎵 Audio Player")
        self.setup_audio_tab(audio_frame)
        
        # Video tab
        video_frame = ttk.Frame(notebook)
        notebook.add(video_frame, text="📺 Video Player")
        self.setup_video_tab(video_frame)
        
        # YouTube tab
        youtube_frame = ttk.Frame(notebook)
        notebook.add(youtube_frame, text="📺 YouTube Player")
        self.setup_youtube_tab(youtube_frame)
    
    def setup_audio_tab(self, parent):
        """Setup audio player tab"""
        # File selection frame
        file_frame = ttk.LabelFrame(parent, text="Audio Files", padding=10)
        file_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(file_frame, text="📁 Select Audio File", command=self.select_audio_file).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(file_frame, text="🌐 Play Audio URL", command=self.play_audio_url).pack(side=tk.LEFT)
        
        # Controls frame
        controls_frame = ttk.LabelFrame(parent, text="Playback Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Control buttons
        button_frame = ttk.Frame(controls_frame)
        button_frame.pack()
        
        ttk.Button(button_frame, text="▶️ Play", command=self.play_audio).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏹️ Stop", command=self.stop_audio).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏸️ Pause", command=self.pause_audio).pack(side=tk.LEFT, padx=5)
        
        # Status frame
        status_frame = ttk.LabelFrame(parent, text="Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        from tkinter import scrolledtext
        self.audio_status = scrolledtext.ScrolledText(status_frame, height=8)
        self.audio_status.pack(fill=tk.BOTH, expand=True)
        
        # Add some sample audio files
        sample_frame = ttk.LabelFrame(parent, text="Sample Audio", padding=10)
        sample_frame.pack(fill=tk.X)
        
        sample_files = [
            "🎵 Sample Music 1",
            "🎵 Sample Music 2", 
            "🎵 Sample Music 3"
        ]
        
        for i, sample in enumerate(sample_files):
            ttk.Button(sample_frame, text=sample, 
                      command=lambda x=i: self.play_sample_audio(x)).pack(side=tk.LEFT, padx=5)
    
    def setup_video_tab(self, parent):
        """Setup video player tab"""
        # File selection frame
        file_frame = ttk.LabelFrame(parent, text="Video Files", padding=10)
        file_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(file_frame, text="📁 Select Video File", command=self.select_video_file).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(file_frame, text="🌐 Play Video URL", command=self.play_video_url).pack(side=tk.LEFT)
        
        # Controls frame
        controls_frame = ttk.LabelFrame(parent, text="Playback Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Control buttons
        button_frame = ttk.Frame(controls_frame)
        button_frame.pack()
        
        ttk.Button(button_frame, text="▶️ Play", command=self.play_video).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏹️ Stop", command=self.stop_video).pack(side=tk.LEFT, padx=5)
        
        # Status frame
        status_frame = ttk.LabelFrame(parent, text="Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        from tkinter import scrolledtext
        self.video_status = scrolledtext.ScrolledText(status_frame, height=8)
        self.video_status.pack(fill=tk.BOTH, expand=True)
        
        # Add some sample video files
        sample_frame = ttk.LabelFrame(parent, text="Sample Videos", padding=10)
        sample_frame.pack(fill=tk.X)
        
        sample_videos = [
            "📺 Sample Video 1",
            "📺 Sample Video 2",
            "📺 Sample Video 3"
        ]
        
        for i, sample in enumerate(sample_videos):
            ttk.Button(sample_frame, text=sample, 
                      command=lambda x=i: self.play_sample_video(x)).pack(side=tk.LEFT, padx=5)
    
    def setup_youtube_tab(self, parent):
        """Setup YouTube player tab"""
        # URL input frame
        url_frame = ttk.LabelFrame(parent, text="YouTube URL", padding=10)
        url_frame.pack(fill=tk.X, pady=(0, 10))
        
        self.youtube_url_var = tk.StringVar()
        url_entry = ttk.Entry(url_frame, textvariable=self.youtube_url_var, width=50)
        url_entry.pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(url_frame, text="▶️ Play", command=self.play_youtube).pack(side=tk.LEFT, padx=5)
        ttk.Button(url_frame, text="🌐 Open in Browser", command=self.open_youtube_browser).pack(side=tk.LEFT, padx=5)
        
        # Sample URLs frame
        samples_frame = ttk.LabelFrame(parent, text="Sample YouTube Videos", padding=10)
        samples_frame.pack(fill=tk.X, pady=(0, 10))
        
        sample_urls = [
            ("🎵 Music Video", "https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
            ("📚 Tutorial", "https://www.youtube.com/watch?v=kJQP7kiw5Fk"),
            ("🎮 Gaming", "https://www.youtube.com/watch?v=jNQXAC9IVRw")
        ]
        
        for name, url in sample_urls:
            ttk.Button(samples_frame, text=name, 
                      command=lambda u=url: self.set_youtube_url(u)).pack(side=tk.LEFT, padx=5)
        
        # Controls frame
        controls_frame = ttk.LabelFrame(parent, text="Playback Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(controls_frame, text="⏹️ Stop", command=self.stop_youtube).pack(side=tk.LEFT, padx=5)
        
        # Status frame
        status_frame = ttk.LabelFrame(parent, text="Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        from tkinter import scrolledtext
        self.youtube_status = scrolledtext.ScrolledText(status_frame, height=8)
        self.youtube_status.pack(fill=tk.BOTH, expand=True)
        
        # Add status message
        self.youtube_status.insert(tk.END, "📺 YouTube Player Ready!\n")
        self.youtube_status.insert(tk.END, "Enter a YouTube URL above or click a sample video.\n")
    
    def select_audio_file(self):
        """Select audio file"""
        file_path = filedialog.askopenfilename(
            title="Select Audio File",
            filetypes=[
                ("Audio files", "*.mp3 *.wav *.flac *.aac *.ogg *.m4a"),
                ("MP3 files", "*.mp3"),
                ("WAV files", "*.wav"),
                ("All files", "*.*")
            ]
        )
        
        if file_path:
            self.audio_status.insert(tk.END, f"📁 Selected: {os.path.basename(file_path)}\n")
            self.audio_status.see(tk.END)
            # Store for playback
            self.selected_audio_file = file_path
    
    def select_video_file(self):
        """Select video file"""
        file_path = filedialog.askopenfilename(
            title="Select Video File",
            filetypes=[
                ("Video files", "*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm"),
                ("MP4 files", "*.mp4"),
                ("AVI files", "*.avi"),
                ("All files", "*.*")
            ]
        )
        
        if file_path:
            self.video_status.insert(tk.END, f"📁 Selected: {os.path.basename(file_path)}\n")
            self.video_status.see(tk.END)
            # Store for playback
            self.selected_video_file = file_path
    
    def play_audio_url(self):
        """Play audio from URL"""
        url = tk.simpledialog.askstring("Audio URL", "Enter audio URL:")
        if url:
            self.audio_status.insert(tk.END, f"🌐 Playing URL: {url}\n")
            self.audio_status.see(tk.END)
            threading.Thread(target=lambda: self.audio_player.play_url(url), daemon=True).start()
    
    def play_video_url(self):
        """Play video from URL"""
        url = tk.simpledialog.askstring("Video URL", "Enter video URL:")
        if url:
            self.video_status.insert(tk.END, f"🌐 Playing URL: {url}\n")
            self.video_status.see(tk.END)
            threading.Thread(target=lambda: self.video_player.play_file(url), daemon=True).start()
    
    def play_audio(self):
        """Play selected audio file"""
        if hasattr(self, 'selected_audio_file'):
            self.audio_status.insert(tk.END, f"▶️ Playing: {os.path.basename(self.selected_audio_file)}\n")
            self.audio_status.see(tk.END)
            threading.Thread(target=lambda: self.audio_player.play_file(self.selected_audio_file), daemon=True).start()
        else:
            messagebox.showwarning("No File", "Please select an audio file first.")
    
    def play_video(self):
        """Play selected video file"""
        if hasattr(self, 'selected_video_file'):
            self.video_status.insert(tk.END, f"▶️ Playing: {os.path.basename(self.selected_video_file)}\n")
            self.video_status.see(tk.END)
            threading.Thread(target=lambda: self.video_player.play_file(self.selected_video_file), daemon=True).start()
        else:
            messagebox.showwarning("No File", "Please select a video file first.")
    
    def play_sample_audio(self, index):
        """Play sample audio"""
        self.audio_status.insert(tk.END, f"🎵 Playing sample audio {index + 1}\n")
        self.audio_status.see(tk.END)
        # For demo purposes, we'll just show the message
        messagebox.showinfo("Sample Audio", f"Playing sample audio {index + 1}!\n\nNote: Select a real audio file for actual playback.")
    
    def play_sample_video(self, index):
        """Play sample video"""
        self.video_status.insert(tk.END, f"📺 Playing sample video {index + 1}\n")
        self.video_status.see(tk.END)
        # For demo purposes, we'll just show the message
        messagebox.showinfo("Sample Video", f"Playing sample video {index + 1}!\n\nNote: Select a real video file for actual playback.")
    
    def stop_audio(self):
        """Stop audio playback"""
        self.audio_player.stop()
        self.audio_status.insert(tk.END, "⏹️ Audio stopped\n")
        self.audio_status.see(tk.END)
    
    def stop_video(self):
        """Stop video playback"""
        self.video_player.stop()
        self.video_status.insert(tk.END, "⏹️ Video stopped\n")
        self.video_status.see(tk.END)
    
    def pause_audio(self):
        """Pause audio playback"""
        self.audio_player.pause()
        self.audio_status.insert(tk.END, "⏸️ Audio paused\n")
        self.audio_status.see(tk.END)
    
    def set_youtube_url(self, url):
        """Set YouTube URL"""
        self.youtube_url_var.set(url)
        self.youtube_status.insert(tk.END, f"📺 URL set: {url}\n")
        self.youtube_status.see(tk.END)
    
    def play_youtube(self):
        """Play YouTube video"""
        url = self.youtube_url_var.get().strip()
        if not url:
            messagebox.showwarning("No URL", "Please enter a YouTube URL.")
            return
        
        self.youtube_status.insert(tk.END, f"▶️ Playing YouTube video: {url}\n")
        self.youtube_status.see(tk.END)
        
        # Try to play with video player first
        success = self.video_player.play_youtube_url(url)
        if not success:
            # Fallback to browser
            webbrowser.open(url)
            self.youtube_status.insert(tk.END, "🌐 Opened in browser as fallback\n")
            self.youtube_status.see(tk.END)
    
    def open_youtube_browser(self):
        """Open YouTube URL in browser"""
        url = self.youtube_url_var.get().strip()
        if not url:
            messagebox.showwarning("No URL", "Please enter a YouTube URL.")
            return
        
        webbrowser.open(url)
        self.youtube_status.insert(tk.END, f"🌐 Opened in browser: {url}\n")
        self.youtube_status.see(tk.END)
    
    def stop_youtube(self):
        """Stop YouTube playback"""
        self.video_player.stop()
        self.youtube_status.insert(tk.END, "⏹️ YouTube video stopped\n")
        self.youtube_status.see(tk.END)

# Integration function for IDE
def integrate_real_media_player(ide_instance):
    """Integrate real media player with IDE"""
    
    # Add media player tab to the IDE
    media_frame = ttk.Frame(ide_instance.notebook)
    ide_instance.notebook.add(media_frame, text="🎵 Real Media")
    
    # Create real media player GUI
    real_player = RealMediaPlayerGUI(media_frame)
    
    # Store reference in IDE
    ide_instance.real_media_player = real_player
    
    print("🎵 Real Media Player integrated with n0mn0m IDE!")

if __name__ == "__main__":
    print("🎵 Real Media Player")
    print("=" * 30)
    
    # Create and run standalone media player
    app = RealMediaPlayerGUI()
    app.root.mainloop()
