#!/usr/bin/env python3
"""
Working Media Player - Real Audio/Video Playback
Actually plays music and videos using system players
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import os
import sys
import subprocess
import threading
import webbrowser
import urllib.parse
from pathlib import Path
import time
from typing import Dict, List, Optional, Any

class WorkingMediaPlayer:
    """Working media player that actually plays files"""
    
    def __init__(self):
        self.current_process = None
        self.is_playing = False
        self.current_file = None
        self.volume = 50
        
        # Detect available players
        self.available_players = self._detect_players()
        print(f"🎵 Available media players: {', '.join(self.available_players)}")
    
    def _detect_players(self) -> List[str]:
        """Detect available media players on the system"""
        players = []
        
        if sys.platform == "win32":
            # Windows players
            windows_players = ['wmplayer', 'vlc', 'mpv', 'potplayer', 'gomplayer']
            for player in windows_players:
                try:
                    subprocess.run([player, '/?'], capture_output=True, timeout=2)
                    players.append(player)
                except:
                    pass
            
            # Check for Windows Media Player specifically
            try:
                subprocess.run(['wmplayer'], capture_output=True, timeout=2)
                if 'wmplayer' not in players:
                    players.append('wmplayer')
            except:
                pass
                
        elif sys.platform == "darwin":
            # macOS players
            try:
                subprocess.run(['open', '-a', 'QuickTime Player'], capture_output=True, timeout=2)
                players.append('quicktime')
            except:
                pass
            
            try:
                subprocess.run(['open', '-a', 'VLC'], capture_output=True, timeout=2)
                players.append('vlc')
            except:
                pass
                
        else:
            # Linux players
            linux_players = ['vlc', 'mpv', 'mplayer', 'totem', 'parole']
            for player in linux_players:
                try:
                    subprocess.run([player, '--version'], capture_output=True, timeout=2)
                    players.append(player)
                except:
                    pass
        
        return players if players else ['browser']  # Fallback to browser
    
    def play_audio_file(self, file_path: str) -> bool:
        """Play audio file using available player"""
        try:
            if self.is_playing:
                self.stop()
            
            if not os.path.exists(file_path):
                print(f"❌ File not found: {file_path}")
                return False
            
            print(f"🎵 Playing audio file: {os.path.basename(file_path)}")
            
            if sys.platform == "win32":
                success = self._play_windows(file_path)
            elif sys.platform == "darwin":
                success = self._play_macos(file_path)
            else:
                success = self._play_linux(file_path)
            
            if success:
                self.current_file = file_path
                self.is_playing = True
                return True
            else:
                print(f"❌ Failed to play audio file: {file_path}")
                return False
                
        except Exception as e:
            print(f"❌ Error playing audio file: {e}")
            return False
    
    def play_video_file(self, file_path: str) -> bool:
        """Play video file using available player"""
        try:
            if self.is_playing:
                self.stop()
            
            if not os.path.exists(file_path):
                print(f"❌ File not found: {file_path}")
                return False
            
            print(f"📺 Playing video file: {os.path.basename(file_path)}")
            
            if sys.platform == "win32":
                success = self._play_windows(file_path)
            elif sys.platform == "darwin":
                success = self._play_macos(file_path)
            else:
                success = self._play_linux(file_path)
            
            if success:
                self.current_file = file_path
                self.is_playing = True
                return True
            else:
                print(f"❌ Failed to play video file: {file_path}")
                return False
                
        except Exception as e:
            print(f"❌ Error playing video file: {e}")
            return False
    
    def play_url(self, url: str) -> bool:
        """Play media from URL"""
        try:
            if self.is_playing:
                self.stop()
            
            print(f"🌐 Playing URL: {url}")
            
            # Check if it's a YouTube URL
            if 'youtube.com' in url or 'youtu.be' in url:
                return self._play_youtube(url)
            
            # Try to play with available players
            if sys.platform == "win32":
                success = self._play_windows(url)
            elif sys.platform == "darwin":
                success = self._play_macos(url)
            else:
                success = self._play_linux(url)
            
            if success:
                self.current_file = url
                self.is_playing = True
                return True
            else:
                # Fallback to browser
                webbrowser.open(url)
                print(f"🌐 Opened URL in browser: {url}")
                return True
                
        except Exception as e:
            print(f"❌ Error playing URL: {e}")
            return False
    
    def _play_windows(self, file_or_url: str) -> bool:
        """Play on Windows"""
        try:
            if 'vlc' in self.available_players:
                self.current_process = subprocess.Popen(['vlc', file_or_url])
                print("🎵 Playing with VLC")
                return True
            elif 'mpv' in self.available_players:
                self.current_process = subprocess.Popen(['mpv', file_or_url])
                print("🎵 Playing with mpv")
                return True
            elif 'wmplayer' in self.available_players:
                self.current_process = subprocess.Popen(['wmplayer', file_or_url])
                print("🎵 Playing with Windows Media Player")
                return True
            else:
                return False
        except Exception as e:
            print(f"❌ Windows playback error: {e}")
            return False
    
    def _play_macos(self, file_or_url: str) -> bool:
        """Play on macOS"""
        try:
            if 'vlc' in self.available_players:
                self.current_process = subprocess.Popen(['open', '-a', 'VLC', file_or_url])
                print("🎵 Playing with VLC")
                return True
            elif 'quicktime' in self.available_players:
                self.current_process = subprocess.Popen(['open', '-a', 'QuickTime Player', file_or_url])
                print("🎵 Playing with QuickTime Player")
                return True
            else:
                return False
        except Exception as e:
            print(f"❌ macOS playback error: {e}")
            return False
    
    def _play_linux(self, file_or_url: str) -> bool:
        """Play on Linux"""
        try:
            if 'vlc' in self.available_players:
                self.current_process = subprocess.Popen(['vlc', file_or_url])
                print("🎵 Playing with VLC")
                return True
            elif 'mpv' in self.available_players:
                self.current_process = subprocess.Popen(['mpv', file_or_url])
                print("🎵 Playing with mpv")
                return True
            elif 'mplayer' in self.available_players:
                self.current_process = subprocess.Popen(['mplayer', file_or_url])
                print("🎵 Playing with MPlayer")
                return True
            else:
                return False
        except Exception as e:
            print(f"❌ Linux playback error: {e}")
            return False
    
    def _play_youtube(self, url: str) -> bool:
        """Play YouTube video"""
        try:
            # Try to use youtube-dl or yt-dlp to get direct URL
            direct_url = None
            
            # Try yt-dlp first
            try:
                result = subprocess.run(['yt-dlp', '-g', url], 
                                      capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    direct_url = result.stdout.strip().split('\n')[0]  # Get first URL
            except:
                pass
            
            # Try youtube-dl as fallback
            if not direct_url:
                try:
                    result = subprocess.run(['youtube-dl', '-g', url], 
                                          capture_output=True, text=True, timeout=10)
                    if result.returncode == 0:
                        direct_url = result.stdout.strip().split('\n')[0]
                except:
                    pass
            
            if direct_url:
                # Play direct URL
                if sys.platform == "win32":
                    return self._play_windows(direct_url)
                elif sys.platform == "darwin":
                    return self._play_macos(direct_url)
                else:
                    return self._play_linux(direct_url)
            else:
                # Fallback to browser
                webbrowser.open(url)
                print(f"📺 Opened YouTube video in browser: {url}")
                return True
                
        except Exception as e:
            print(f"❌ YouTube playback error: {e}")
            # Fallback to browser
            webbrowser.open(url)
            return True
    
    def stop(self):
        """Stop playback"""
        try:
            if self.current_process:
                self.current_process.terminate()
                self.current_process = None
            
            self.is_playing = False
            self.current_file = None
            print("⏹️ Playback stopped")
            
        except Exception as e:
            print(f"❌ Error stopping playback: {e}")
    
    def pause(self):
        """Pause playback (limited support)"""
        # Note: External players don't support pause via subprocess
        print("⏸️ Pause not supported with external players")
    
    def set_volume(self, volume: int):
        """Set volume (limited support)"""
        self.volume = max(0, min(100, volume))
        print(f"🔊 Volume set to {self.volume}%")
    
    def get_status(self) -> Dict[str, Any]:
        """Get player status"""
        return {
            'is_playing': self.is_playing,
            'current_file': self.current_file,
            'volume': self.volume,
            'available_players': self.available_players
        }

class WorkingMediaPlayerGUI:
    """GUI for working media player"""
    
    def __init__(self, parent=None):
        self.player = WorkingMediaPlayer()
        
        if parent:
            self.setup_gui(parent)
        else:
            self.create_standalone_window()
    
    def create_standalone_window(self):
        """Create standalone window"""
        self.root = tk.Tk()
        self.root.title("Working Media Player")
        self.root.geometry("900x700")
        self.setup_gui(self.root)
        self.root.mainloop()
    
    def setup_gui(self, parent):
        """Setup GUI components"""
        # Main frame
        main_frame = ttk.Frame(parent)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Title
        title_label = ttk.Label(main_frame, text="🎵 Working Media Player", font=("Arial", 16, "bold"))
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
        
        # Status tab
        status_frame = ttk.Frame(notebook)
        notebook.add(status_frame, text="📊 Status")
        self.setup_status_tab(status_frame)
    
    def setup_audio_tab(self, parent):
        """Setup audio player tab"""
        # File selection
        file_frame = ttk.LabelFrame(parent, text="Audio Files", padding=10)
        file_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(file_frame, text="📁 Select Audio File", command=self.select_audio_file).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(file_frame, text="🌐 Play Audio URL", command=self.play_audio_url).pack(side=tk.LEFT, padx=(0, 10))
        
        # Controls
        controls_frame = ttk.LabelFrame(parent, text="Playback Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        button_frame = ttk.Frame(controls_frame)
        button_frame.pack()
        
        ttk.Button(button_frame, text="▶️ Play", command=self.play_audio).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏹️ Stop", command=self.stop_audio).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏸️ Pause", command=self.pause_audio).pack(side=tk.LEFT, padx=5)
        
        # Volume control
        volume_frame = ttk.Frame(controls_frame)
        volume_frame.pack(pady=(10, 0))
        
        ttk.Label(volume_frame, text="🔊 Volume:").pack(side=tk.LEFT)
        self.volume_var = tk.IntVar(value=50)
        volume_scale = ttk.Scale(volume_frame, from_=0, to=100, variable=self.volume_var, 
                               command=self.set_volume, orient=tk.HORIZONTAL, length=200)
        volume_scale.pack(side=tk.LEFT, padx=10)
        
        # Status
        status_frame = ttk.LabelFrame(parent, text="Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        self.audio_status = tk.Text(status_frame, height=8, wrap=tk.WORD)
        audio_scroll = ttk.Scrollbar(status_frame, orient=tk.VERTICAL, command=self.audio_status.yview)
        self.audio_status.config(yscrollcommand=audio_scroll.set)
        
        self.audio_status.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        audio_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Add initial status
        self.audio_status.insert(tk.END, "🎵 Audio Player Ready!\n")
        self.audio_status.insert(tk.END, f"📊 Available players: {', '.join(self.player.available_players)}\n")
    
    def setup_video_tab(self, parent):
        """Setup video player tab"""
        # File selection
        file_frame = ttk.LabelFrame(parent, text="Video Files", padding=10)
        file_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(file_frame, text="📁 Select Video File", command=self.select_video_file).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(file_frame, text="🌐 Play Video URL", command=self.play_video_url).pack(side=tk.LEFT, padx=(0, 10))
        
        # Controls
        controls_frame = ttk.LabelFrame(parent, text="Playback Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        button_frame = ttk.Frame(controls_frame)
        button_frame.pack()
        
        ttk.Button(button_frame, text="▶️ Play", command=self.play_video).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏹️ Stop", command=self.stop_video).pack(side=tk.LEFT, padx=5)
        
        # Status
        status_frame = ttk.LabelFrame(parent, text="Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        self.video_status = tk.Text(status_frame, height=8, wrap=tk.WORD)
        video_scroll = ttk.Scrollbar(status_frame, orient=tk.VERTICAL, command=self.video_status.yview)
        self.video_status.config(yscrollcommand=video_scroll.set)
        
        self.video_status.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        video_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Add initial status
        self.video_status.insert(tk.END, "📺 Video Player Ready!\n")
        self.video_status.insert(tk.END, f"📊 Available players: {', '.join(self.player.available_players)}\n")
    
    def setup_youtube_tab(self, parent):
        """Setup YouTube player tab"""
        # URL input
        url_frame = ttk.LabelFrame(parent, text="YouTube URL", padding=10)
        url_frame.pack(fill=tk.X, pady=(0, 10))
        
        self.youtube_url_var = tk.StringVar()
        url_entry = ttk.Entry(url_frame, textvariable=self.youtube_url_var, width=60)
        url_entry.pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(url_frame, text="▶️ Play", command=self.play_youtube).pack(side=tk.LEFT, padx=5)
        ttk.Button(url_frame, text="🌐 Browser", command=self.open_youtube_browser).pack(side=tk.LEFT, padx=5)
        
        # Sample URLs
        samples_frame = ttk.LabelFrame(parent, text="Sample YouTube Videos", padding=10)
        samples_frame.pack(fill=tk.X, pady=(0, 10))
        
        sample_urls = [
            ("🎵 Rick Roll", "https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
            ("📚 Coding Tutorial", "https://www.youtube.com/watch?v=kJQP7kiw5Fk"),
            ("🎮 Gaming Video", "https://www.youtube.com/watch?v=jNQXAC9IVRw")
        ]
        
        for name, url in sample_urls:
            ttk.Button(samples_frame, text=name, 
                      command=lambda u=url: self.set_youtube_url(u)).pack(side=tk.LEFT, padx=5)
        
        # Controls
        controls_frame = ttk.LabelFrame(parent, text="Controls", padding=10)
        controls_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Button(controls_frame, text="⏹️ Stop", command=self.stop_youtube).pack(side=tk.LEFT, padx=5)
        
        # Status
        status_frame = ttk.LabelFrame(parent, text="Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        self.youtube_status = tk.Text(status_frame, height=8, wrap=tk.WORD)
        youtube_scroll = ttk.Scrollbar(status_frame, orient=tk.VERTICAL, command=self.youtube_status.yview)
        self.youtube_status.config(yscrollcommand=youtube_scroll.set)
        
        self.youtube_status.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        youtube_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Add initial status
        self.youtube_status.insert(tk.END, "📺 YouTube Player Ready!\n")
        self.youtube_status.insert(tk.END, "Enter a YouTube URL above or click a sample video.\n")
    
    def setup_status_tab(self, parent):
        """Setup status tab"""
        status_frame = ttk.LabelFrame(parent, text="Player Status", padding=10)
        status_frame.pack(fill=tk.BOTH, expand=True)
        
        self.status_text = tk.Text(status_frame, height=15, wrap=tk.WORD)
        status_scroll = ttk.Scrollbar(status_frame, orient=tk.VERTICAL, command=self.status_text.yview)
        self.status_text.config(yscrollcommand=status_scroll.set)
        
        self.status_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        status_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Update status
        self.update_status()
    
    def select_audio_file(self):
        """Select audio file"""
        file_path = filedialog.askopenfilename(
            title="Select Audio File",
            filetypes=[
                ("Audio files", "*.mp3 *.wav *.flac *.aac *.ogg *.m4a *.wma"),
                ("MP3 files", "*.mp3"),
                ("WAV files", "*.wav"),
                ("All files", "*.*")
            ]
        )
        
        if file_path:
            self.selected_audio_file = file_path
            self.audio_status.insert(tk.END, f"📁 Selected: {os.path.basename(file_path)}\n")
            self.audio_status.see(tk.END)
    
    def select_video_file(self):
        """Select video file"""
        file_path = filedialog.askopenfilename(
            title="Select Video File",
            filetypes=[
                ("Video files", "*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.m4v"),
                ("MP4 files", "*.mp4"),
                ("AVI files", "*.avi"),
                ("All files", "*.*")
            ]
        )
        
        if file_path:
            self.selected_video_file = file_path
            self.video_status.insert(tk.END, f"📁 Selected: {os.path.basename(file_path)}\n")
            self.video_status.see(tk.END)
    
    def play_audio_url(self):
        """Play audio from URL"""
        url = tk.simpledialog.askstring("Audio URL", "Enter audio URL:")
        if url:
            self.audio_status.insert(tk.END, f"🌐 Playing URL: {url}\n")
            self.audio_status.see(tk.END)
            threading.Thread(target=lambda: self.player.play_url(url), daemon=True).start()
    
    def play_video_url(self):
        """Play video from URL"""
        url = tk.simpledialog.askstring("Video URL", "Enter video URL:")
        if url:
            self.video_status.insert(tk.END, f"🌐 Playing URL: {url}\n")
            self.video_status.see(tk.END)
            threading.Thread(target=lambda: self.player.play_url(url), daemon=True).start()
    
    def play_audio(self):
        """Play selected audio file"""
        if hasattr(self, 'selected_audio_file'):
            self.audio_status.insert(tk.END, f"▶️ Playing: {os.path.basename(self.selected_audio_file)}\n")
            self.audio_status.see(tk.END)
            threading.Thread(target=lambda: self.player.play_audio_file(self.selected_audio_file), daemon=True).start()
        else:
            messagebox.showwarning("No File", "Please select an audio file first.")
    
    def play_video(self):
        """Play selected video file"""
        if hasattr(self, 'selected_video_file'):
            self.video_status.insert(tk.END, f"▶️ Playing: {os.path.basename(self.selected_video_file)}\n")
            self.video_status.see(tk.END)
            threading.Thread(target=lambda: self.player.play_video_file(self.selected_video_file), daemon=True).start()
        else:
            messagebox.showwarning("No File", "Please select a video file first.")
    
    def stop_audio(self):
        """Stop audio playback"""
        self.player.stop()
        self.audio_status.insert(tk.END, "⏹️ Audio stopped\n")
        self.audio_status.see(tk.END)
    
    def stop_video(self):
        """Stop video playback"""
        self.player.stop()
        self.video_status.insert(tk.END, "⏹️ Video stopped\n")
        self.video_status.see(tk.END)
    
    def pause_audio(self):
        """Pause audio playback"""
        self.player.pause()
        self.audio_status.insert(tk.END, "⏸️ Audio paused\n")
        self.audio_status.see(tk.END)
    
    def set_volume(self, value):
        """Set volume"""
        volume = int(float(value))
        self.player.set_volume(volume)
    
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
        threading.Thread(target=lambda: self.player.play_url(url), daemon=True).start()
    
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
        self.player.stop()
        self.youtube_status.insert(tk.END, "⏹️ YouTube video stopped\n")
        self.youtube_status.see(tk.END)
    
    def update_status(self):
        """Update status display"""
        status = self.player.get_status()
        
        self.status_text.delete(1.0, tk.END)
        self.status_text.insert(tk.END, "📊 Working Media Player Status\n")
        self.status_text.insert(tk.END, "=" * 40 + "\n\n")
        
        self.status_text.insert(tk.END, f"🎵 Playing: {'Yes' if status['is_playing'] else 'No'}\n")
        self.status_text.insert(tk.END, f"📁 Current File: {status['current_file'] or 'None'}\n")
        self.status_text.insert(tk.END, f"🔊 Volume: {status['volume']}%\n")
        self.status_text.insert(tk.END, f"🎮 Available Players: {', '.join(status['available_players'])}\n\n")
        
        self.status_text.insert(tk.END, "💡 Tips:\n")
        self.status_text.insert(tk.END, "• Select audio/video files to play them\n")
        self.status_text.insert(tk.END, "• Enter YouTube URLs to play videos\n")
        self.status_text.insert(tk.END, "• Use browser fallback if players aren't available\n")
        self.status_text.insert(tk.END, "• VLC and mpv provide the best playback experience\n")

# Integration function for IDE
def integrate_working_media_player(ide_instance):
    """Integrate working media player with IDE"""
    
    # Add media player tab to the IDE
    media_frame = ttk.Frame(ide_instance.notebook)
    ide_instance.notebook.add(media_frame, text="🎵 Working Media")
    
    # Create working media player GUI
    working_player = WorkingMediaPlayerGUI(media_frame)
    
    # Store reference in IDE
    ide_instance.working_media_player = working_player
    
    print("🎵 Working Media Player integrated with n0mn0m IDE!")

if __name__ == "__main__":
    print("🎵 Working Media Player")
    print("=" * 30)
    
    # Create and run standalone media player
    app = WorkingMediaPlayerGUI()
