#!/usr/bin/env python3
"""
Free TV App - Access to Premium Content
Scraping, Torrents, Streaming Services Integration
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
import re
from typing import Dict, List, Optional, Any
import urllib.parse
import hashlib

class ContentScraper:
    """Scrapes content from various sources"""
    
    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
        })
    
    def search_torrents(self, query):
        """Search for torrents"""
        torrent_sites = [
            'https://thepiratebay.org/search.php?q=',
            'https://1337x.to/search/',
            'https://rarbg.to/torrents.php?search=',
            'https://yts.mx/browse-movies/'
        ]
        
        results = []
        for site in torrent_sites:
            try:
                url = site + urllib.parse.quote(query)
                response = self.session.get(url, timeout=10)
                if response.status_code == 200:
                    # Parse torrent results (simplified)
                    results.extend(self.parse_torrent_results(response.text, site))
            except Exception as e:
                print(f"Error scraping {site}: {e}")
        
        return results
    
    def parse_torrent_results(self, html, site):
        """Parse torrent results from HTML"""
        # Simplified parsing - in reality would use BeautifulSoup
        results = []
        if 'piratebay' in site:
            results.append({
                'title': 'Sample Torrent',
                'size': '1.2 GB',
                'seeds': '150',
                'leech': '25',
                'magnet': 'magnet:?xt=urn:btih:example',
                'site': 'The Pirate Bay'
            })
        return results
    
    def search_streaming_sites(self, query):
        """Search streaming sites"""
        streaming_sites = [
            'https://fmovies.to/search?keyword=',
            'https://123moviesfree.net/search/',
            'https://putlocker.is/search/',
            'https://solarmovie.one/search/'
        ]
        
        results = []
        for site in streaming_sites:
            try:
                url = site + urllib.parse.quote(query)
                response = self.session.get(url, timeout=10)
                if response.status_code == 200:
                    results.extend(self.parse_streaming_results(response.text, site))
            except Exception as e:
                print(f"Error scraping {site}: {e}")
        
        return results
    
    def parse_streaming_results(self, html, site):
        """Parse streaming results from HTML"""
        results = []
        if 'fmovies' in site:
            results.append({
                'title': 'Sample Movie/Show',
                'year': '2023',
                'quality': 'HD',
                'url': 'https://example.com/watch',
                'site': 'FMovies'
            })
        return results

class TorrentClient:
    """Torrent client integration"""
    
    def __init__(self):
        self.active_torrents = []
        self.download_path = os.path.expanduser("~/Downloads/Torrents")
        os.makedirs(self.download_path, exist_ok=True)
    
    def download_torrent(self, magnet_link, title):
        """Download torrent using magnet link"""
        try:
            # Use qbittorrent or transmission if available
            if self.check_qbittorrent():
                return self.download_with_qbittorrent(magnet_link, title)
            elif self.check_transmission():
                return self.download_with_transmission(magnet_link, title)
            else:
                # Fallback to browser magnet link
                webbrowser.open(magnet_link)
                return True
        except Exception as e:
            print(f"Error downloading torrent: {e}")
            return False
    
    def check_qbittorrent(self):
        """Check if qBittorrent is available"""
        try:
            subprocess.run(['qbittorrent', '--version'], 
                         capture_output=True, check=True)
            return True
        except:
            return False
    
    def check_transmission(self):
        """Check if Transmission is available"""
        try:
            subprocess.run(['transmission-cli', '--version'], 
                         capture_output=True, check=True)
            return True
        except:
            return False
    
    def download_with_qbittorrent(self, magnet_link, title):
        """Download using qBittorrent"""
        try:
            # This would use qBittorrent API in real implementation
            print(f"Adding torrent to qBittorrent: {title}")
            return True
        except Exception as e:
            print(f"Error with qBittorrent: {e}")
            return False
    
    def download_with_transmission(self, magnet_link, title):
        """Download using Transmission"""
        try:
            # This would use Transmission CLI in real implementation
            print(f"Adding torrent to Transmission: {title}")
            return True
        except Exception as e:
            print(f"Error with Transmission: {e}")
            return False

class StreamingService:
    """Streaming service integration"""
    
    def __init__(self):
        self.services = {
            'netflix': {'name': 'Netflix', 'base_url': 'https://netflix.com'},
            'disney': {'name': 'Disney+', 'base_url': 'https://disneyplus.com'},
            'hulu': {'name': 'Hulu', 'base_url': 'https://hulu.com'},
            'hbo': {'name': 'HBO Max', 'base_url': 'https://hbomax.com'},
            'amazon': {'name': 'Amazon Prime', 'base_url': 'https://primevideo.com'},
            'crunchyroll': {'name': 'Crunchyroll', 'base_url': 'https://crunchyroll.com'},
            'funimation': {'name': 'Funimation', 'base_url': 'https://funimation.com'}
        }
    
    def get_content_info(self, service, content_id):
        """Get content information from streaming service"""
        # This would integrate with actual APIs
        return {
            'title': 'Sample Content',
            'description': 'Sample description',
            'rating': '8.5/10',
            'duration': '120 min',
            'genres': ['Action', 'Drama'],
            'available': True
        }
    
    def get_streaming_url(self, service, content_id):
        """Get streaming URL (simplified)"""
        # In reality, this would require authentication and API access
        return f"https://{service}.com/watch/{content_id}"

class FreeTVApp:
    """Main Free TV Application"""
    
    def __init__(self):
        self.scraper = ContentScraper()
        self.torrent_client = TorrentClient()
        self.streaming_service = StreamingService()
        
        # Content categories
        self.categories = {
            'movies': ['Action', 'Comedy', 'Drama', 'Horror', 'Sci-Fi', 'Thriller'],
            'tv_shows': ['Drama', 'Comedy', 'Reality', 'Documentary', 'Anime'],
            'anime': ['Shounen', 'Shoujo', 'Seinen', 'Josei', 'Mecha', 'Slice of Life'],
            'sports': ['UFC', 'Boxing', 'Football', 'Basketball', 'Soccer'],
            'cartoons': ['Disney', 'Pixar', 'Cartoon Network', 'Adult Swim']
        }
        
        # Premium content sources
        self.premium_sources = {
            'netflix': ['Stranger Things', 'The Witcher', 'Ozark', 'Money Heist'],
            'disney': ['The Mandalorian', 'WandaVision', 'Loki', 'Falcon and Winter Soldier'],
            'hbo': ['Game of Thrones', 'Euphoria', 'Succession', 'The Last of Us'],
            'crunchyroll': ['Attack on Titan', 'Demon Slayer', 'One Piece', 'My Hero Academia'],
            'ufc': ['UFC 300', 'UFC 299', 'UFC 298', 'UFC 297']
        }
    
    def search_content(self, query, category='all'):
        """Search for content across all sources"""
        results = {
            'torrents': self.scraper.search_torrents(query),
            'streaming': self.scraper.search_streaming_sites(query),
            'premium': self.search_premium_content(query)
        }
        return results
    
    def search_premium_content(self, query):
        """Search premium content sources"""
        results = []
        for service, content_list in self.premium_sources.items():
            for content in content_list:
                if query.lower() in content.lower():
                    results.append({
                        'title': content,
                        'service': service,
                        'type': 'premium',
                        'available': True
                    })
        return results
    
    def get_content_sources(self, title):
        """Get all available sources for specific content"""
        sources = []
        
        # Check torrents
        torrent_results = self.scraper.search_torrents(title)
        sources.extend([{'type': 'torrent', 'data': t} for t in torrent_results])
        
        # Check streaming sites
        streaming_results = self.scraper.search_streaming_sites(title)
        sources.extend([{'type': 'streaming', 'data': s} for s in streaming_results])
        
        # Check premium services
        premium_results = self.search_premium_content(title)
        sources.extend([{'type': 'premium', 'data': p} for p in premium_results])
        
        return sources

class FreeTVGUI:
    """GUI for Free TV App"""
    
    def __init__(self, parent_ide):
        self.parent_ide = parent_ide
        self.free_tv_app = FreeTVApp()
        self.setup_gui()
    
    def setup_gui(self):
        """Setup Free TV GUI"""
        # Create main window
        self.tv_window = tk.Toplevel(self.parent_ide.root)
        self.tv_window.title("📺 Free TV App - Premium Content Access")
        self.tv_window.geometry("1200x800")
        self.tv_window.configure(bg='#2d2d30')
        
        # Title
        title_frame = tk.Frame(self.tv_window, bg='#2d2d30')
        title_frame.pack(fill=tk.X, padx=10, pady=10)
        
        title_label = tk.Label(title_frame, 
                              text="📺 Free TV App - Premium Content Access",
                              font=('Arial', 18, 'bold'),
                              bg='#2d2d30', fg='white')
        title_label.pack()
        
        subtitle_label = tk.Label(title_frame,
                                text="Movies • TV Shows • Anime • UFC • Cartoons • Premium Streaming",
                                font=('Arial', 12),
                                bg='#2d2d30', fg='#cccccc')
        subtitle_label.pack()
        
        # Main content
        content_frame = tk.Frame(self.tv_window, bg='#2d2d30')
        content_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Create notebook for different sections
        notebook = ttk.Notebook(content_frame)
        notebook.pack(fill=tk.BOTH, expand=True)
        
        # Search tab
        self.setup_search_tab(notebook)
        
        # Categories tab
        self.setup_categories_tab(notebook)
        
        # Premium content tab
        self.setup_premium_tab(notebook)
        
        # Torrents tab
        self.setup_torrents_tab(notebook)
        
        # Streaming tab
        self.setup_streaming_tab(notebook)
    
    def setup_search_tab(self, notebook):
        """Setup search tab"""
        search_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(search_frame, text="🔍 Search")
        
        # Search controls
        search_controls = tk.Frame(search_frame, bg='#1e1e1e')
        search_controls.pack(fill=tk.X, padx=10, pady=10)
        
        # Search entry
        search_entry_frame = tk.Frame(search_controls, bg='#1e1e1e')
        search_entry_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(search_entry_frame, text="Search Content:", bg='#1e1e1e', fg='white', 
                font=('Arial', 10, 'bold')).pack(side=tk.LEFT)
        
        self.search_entry = tk.Entry(search_entry_frame, width=50, bg='#0d1117', fg='white')
        self.search_entry.pack(side=tk.LEFT, padx=5)
        self.search_entry.bind('<Return>', self.search_content)
        
        search_btn = tk.Button(search_entry_frame, text="🔍 Search", command=self.search_content,
                              bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'))
        search_btn.pack(side=tk.LEFT, padx=5)
        
        # Category filter
        category_frame = tk.Frame(search_controls, bg='#1e1e1e')
        category_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(category_frame, text="Category:", bg='#1e1e1e', fg='white', 
                font=('Arial', 10, 'bold')).pack(side=tk.LEFT)
        
        self.category_var = tk.StringVar(value="all")
        category_combo = ttk.Combobox(category_frame, textvariable=self.category_var,
                                     values=["all", "movies", "tv_shows", "anime", "sports", "cartoons"],
                                     width=15)
        category_combo.pack(side=tk.LEFT, padx=5)
        
        # Results
        results_frame = tk.Frame(search_frame, bg='#1e1e1e')
        results_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        tk.Label(results_frame, text="Search Results:", bg='#1e1e1e', fg='white', 
                font=('Arial', 10, 'bold')).pack(anchor=tk.W)
        
        # Results listbox
        self.results_listbox = tk.Listbox(results_frame, bg='#0d1117', fg='#58a6ff', 
                                         font=('Consolas', 10), height=20)
        results_scroll = tk.Scrollbar(results_frame, orient=tk.VERTICAL, command=self.results_listbox.yview)
        self.results_listbox.configure(yscrollcommand=results_scroll.set)
        
        self.results_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        results_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.results_listbox.bind('<Double-Button-1>', self.on_result_select)
    
    def setup_categories_tab(self, notebook):
        """Setup categories tab"""
        categories_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(categories_frame, text="📂 Categories")
        
        # Categories grid
        categories_grid = tk.Frame(categories_frame, bg='#1e1e1e')
        categories_grid.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Movies
        movies_frame = tk.LabelFrame(categories_grid, text="🎬 Movies", 
                                   bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        movies_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        movies_list = tk.Listbox(movies_frame, bg='#0d1117', fg='#ff6b35', 
                                font=('Consolas', 9), height=8)
        movies_list.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        for genre in self.free_tv_app.categories['movies']:
            movies_list.insert(tk.END, f"• {genre}")
        
        # TV Shows
        tv_frame = tk.LabelFrame(categories_grid, text="📺 TV Shows", 
                                bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        tv_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        tv_list = tk.Listbox(tv_frame, bg='#0d1117', fg='#4CAF50', 
                            font=('Consolas', 9), height=8)
        tv_list.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        for genre in self.free_tv_app.categories['tv_shows']:
            tv_list.insert(tk.END, f"• {genre}")
        
        # Anime
        anime_frame = tk.LabelFrame(categories_grid, text="🎌 Anime", 
                                   bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        anime_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        anime_list = tk.Listbox(anime_frame, bg='#0d1117', fg='#e91e63', 
                               font=('Consolas', 9), height=8)
        anime_list.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        for genre in self.free_tv_app.categories['anime']:
            anime_list.insert(tk.END, f"• {genre}")
        
        # Sports
        sports_frame = tk.LabelFrame(categories_grid, text="🥊 Sports", 
                                    bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        sports_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        sports_list = tk.Listbox(sports_frame, bg='#0d1117', fg='#ff9800', 
                                font=('Consolas', 9), height=8)
        sports_list.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        for sport in self.free_tv_app.categories['sports']:
            sports_list.insert(tk.END, f"• {sport}")
    
    def setup_premium_tab(self, notebook):
        """Setup premium content tab"""
        premium_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(premium_frame, text="💎 Premium")
        
        # Premium services
        services_frame = tk.Frame(premium_frame, bg='#1e1e1e')
        services_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Netflix
        netflix_frame = tk.LabelFrame(services_frame, text="🔴 Netflix", 
                                     bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        netflix_frame.pack(fill=tk.X, padx=5, pady=5)
        
        netflix_list = tk.Listbox(netflix_frame, bg='#0d1117', fg='#e50914', 
                                 font=('Consolas', 9), height=6)
        netflix_list.pack(fill=tk.X, padx=5, pady=5)
        
        for content in self.free_tv_app.premium_sources['netflix']:
            netflix_list.insert(tk.END, f"• {content}")
        
        # Disney+
        disney_frame = tk.LabelFrame(services_frame, text="🏰 Disney+", 
                                   bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        disney_frame.pack(fill=tk.X, padx=5, pady=5)
        
        disney_list = tk.Listbox(disney_frame, bg='#0d1117', fg='#113ccf', 
                                font=('Consolas', 9), height=6)
        disney_list.pack(fill=tk.X, padx=5, pady=5)
        
        for content in self.free_tv_app.premium_sources['disney']:
            disney_list.insert(tk.END, f"• {content}")
        
        # HBO Max
        hbo_frame = tk.LabelFrame(services_frame, text="🎭 HBO Max", 
                                 bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        hbo_frame.pack(fill=tk.X, padx=5, pady=5)
        
        hbo_list = tk.Listbox(hbo_frame, bg='#0d1117', fg='#8b5cf6', 
                             font=('Consolas', 9), height=6)
        hbo_list.pack(fill=tk.X, padx=5, pady=5)
        
        for content in self.free_tv_app.premium_sources['hbo']:
            hbo_list.insert(tk.END, f"• {content}")
        
        # Crunchyroll
        crunchy_frame = tk.LabelFrame(services_frame, text="🍙 Crunchyroll", 
                                     bg='#1e1e1e', fg='white', font=('Arial', 10, 'bold'))
        crunchy_frame.pack(fill=tk.X, padx=5, pady=5)
        
        crunchy_list = tk.Listbox(crunchy_frame, bg='#0d1117', fg='#ff6600', 
                                  font=('Consolas', 9), height=6)
        crunchy_list.pack(fill=tk.X, padx=5, pady=5)
        
        for content in self.free_tv_app.premium_sources['crunchyroll']:
            crunchy_list.insert(tk.END, f"• {content}")
    
    def setup_torrents_tab(self, notebook):
        """Setup torrents tab"""
        torrents_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(torrents_frame, text="🌊 Torrents")
        
        # Torrent controls
        controls_frame = tk.Frame(torrents_frame, bg='#1e1e1e')
        controls_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Search torrents
        search_frame = tk.Frame(controls_frame, bg='#1e1e1e')
        search_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(search_frame, text="Search Torrents:", bg='#1e1e1e', fg='white', 
                font=('Arial', 10, 'bold')).pack(side=tk.LEFT)
        
        self.torrent_search_entry = tk.Entry(search_frame, width=40, bg='#0d1117', fg='white')
        self.torrent_search_entry.pack(side=tk.LEFT, padx=5)
        self.torrent_search_entry.bind('<Return>', self.search_torrents)
        
        search_btn = tk.Button(search_frame, text="🔍 Search", command=self.search_torrents,
                              bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'))
        search_btn.pack(side=tk.LEFT, padx=5)
        
        # Torrent results
        results_frame = tk.Frame(torrents_frame, bg='#1e1e1e')
        results_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        tk.Label(results_frame, text="Torrent Results:", bg='#1e1e1e', fg='white', 
                font=('Arial', 10, 'bold')).pack(anchor=tk.W)
        
        self.torrent_results = tk.Listbox(results_frame, bg='#0d1117', fg='#58a6ff', 
                                         font=('Consolas', 9), height=15)
        torrent_scroll = tk.Scrollbar(results_frame, orient=tk.VERTICAL, command=self.torrent_results.yview)
        self.torrent_results.configure(yscrollcommand=torrent_scroll.set)
        
        self.torrent_results.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        torrent_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.torrent_results.bind('<Double-Button-1>', self.download_torrent)
        
        # Download controls
        download_frame = tk.Frame(torrents_frame, bg='#1e1e1e')
        download_frame.pack(fill=tk.X, padx=10, pady=10)
        
        download_btn = tk.Button(download_frame, text="⬇️ Download Selected", 
                                command=self.download_selected_torrent,
                                bg='#ff9800', fg='white', font=('Arial', 10, 'bold'))
        download_btn.pack(side=tk.LEFT, padx=5)
        
        clear_btn = tk.Button(download_frame, text="🗑️ Clear Results", 
                             command=self.clear_torrent_results,
                             bg='#f44336', fg='white', font=('Arial', 10, 'bold'))
        clear_btn.pack(side=tk.LEFT, padx=5)
    
    def setup_streaming_tab(self, notebook):
        """Setup streaming tab"""
        streaming_frame = tk.Frame(notebook, bg='#1e1e1e')
        notebook.add(streaming_frame, text="📺 Streaming")
        
        # Streaming sites
        sites_frame = tk.Frame(streaming_frame, bg='#1e1e1e')
        sites_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Popular streaming sites
        sites = [
            {'name': 'FMovies', 'url': 'https://fmovies.to', 'color': '#4CAF50'},
            {'name': '123Movies', 'url': 'https://123moviesfree.net', 'color': '#2196F3'},
            {'name': 'Putlocker', 'url': 'https://putlocker.is', 'color': '#FF9800'},
            {'name': 'SolarMovie', 'url': 'https://solarmovie.one', 'color': '#9C27B0'},
            {'name': 'GoMovies', 'url': 'https://gomovies.sx', 'color': '#E91E63'},
            {'name': 'YesMovies', 'url': 'https://yesmovies.ag', 'color': '#00BCD4'}
        ]
        
        for i, site in enumerate(sites):
            site_frame = tk.Frame(sites_frame, bg='#1e1e1e')
            site_frame.pack(fill=tk.X, padx=5, pady=5)
            
            site_btn = tk.Button(site_frame, text=f"🌐 {site['name']}", 
                                command=lambda url=site['url']: webbrowser.open(url),
                                bg=site['color'], fg='white', font=('Arial', 10, 'bold'))
            site_btn.pack(side=tk.LEFT, padx=5)
            
            info_label = tk.Label(site_frame, text=f"Free streaming - {site['url']}", 
                                 bg='#1e1e1e', fg='#cccccc', font=('Arial', 9))
            info_label.pack(side=tk.LEFT, padx=10)
    
    # Event handlers
    def search_content(self, event=None):
        """Search for content"""
        query = self.search_entry.get()
        if not query:
            return
        
        category = self.category_var.get()
        
        def search_thread():
            self.results_listbox.delete(0, tk.END)
            self.results_listbox.insert(tk.END, f"🔍 Searching for: {query}...")
            
            results = self.free_tv_app.search_content(query, category)
            
            self.results_listbox.delete(0, tk.END)
            
            # Add torrent results
            if results['torrents']:
                self.results_listbox.insert(tk.END, "🌊 TORRENTS:")
                for torrent in results['torrents']:
                    display_text = f"  • {torrent['title']} ({torrent['size']}) - {torrent['seeds']} seeds"
                    self.results_listbox.insert(tk.END, display_text)
            
            # Add streaming results
            if results['streaming']:
                self.results_listbox.insert(tk.END, "")
                self.results_listbox.insert(tk.END, "📺 STREAMING:")
                for stream in results['streaming']:
                    display_text = f"  • {stream['title']} ({stream['year']}) - {stream['quality']}"
                    self.results_listbox.insert(tk.END, display_text)
            
            # Add premium results
            if results['premium']:
                self.results_listbox.insert(tk.END, "")
                self.results_listbox.insert(tk.END, "💎 PREMIUM:")
                for premium in results['premium']:
                    display_text = f"  • {premium['title']} - {premium['service']}"
                    self.results_listbox.insert(tk.END, display_text)
        
        threading.Thread(target=search_thread, daemon=True).start()
    
    def on_result_select(self, event):
        """Handle result selection"""
        selection = self.results_listbox.curselection()
        if selection:
            selected_text = self.results_listbox.get(selection[0])
            print(f"Selected: {selected_text}")
            messagebox.showinfo("Content Selected", f"Selected: {selected_text}\n\nThis would open the content in your preferred player.")
    
    def search_torrents(self, event=None):
        """Search for torrents"""
        query = self.torrent_search_entry.get()
        if not query:
            return
        
        def search_thread():
            self.torrent_results.delete(0, tk.END)
            self.torrent_results.insert(tk.END, f"🔍 Searching torrents for: {query}...")
            
            torrents = self.free_tv_app.scraper.search_torrents(query)
            
            self.torrent_results.delete(0, tk.END)
            
            for torrent in torrents:
                display_text = f"{torrent['title']} | {torrent['size']} | Seeds: {torrent['seeds']} | Leech: {torrent['leech']}"
                self.torrent_results.insert(tk.END, display_text)
        
        threading.Thread(target=search_thread, daemon=True).start()
    
    def download_torrent(self, event):
        """Download selected torrent"""
        selection = self.torrent_results.curselection()
        if selection:
            selected_text = self.torrent_results.get(selection[0])
            print(f"Downloading torrent: {selected_text}")
            messagebox.showinfo("Torrent Download", f"Downloading: {selected_text}\n\nThis would start the torrent download.")
    
    def download_selected_torrent(self):
        """Download selected torrent"""
        selection = self.torrent_results.curselection()
        if selection:
            self.download_torrent(None)
        else:
            messagebox.showwarning("No Selection", "Please select a torrent to download.")
    
    def clear_torrent_results(self):
        """Clear torrent results"""
        self.torrent_results.delete(0, tk.END)

def integrate_free_tv_with_ide(ide_instance):
    """Integrate Free TV App with the main IDE"""
    def show_free_tv():
        FreeTVGUI(ide_instance)
    
    # Add Free TV to Entertainment menu
    if hasattr(ide_instance, 'menubar'):
        # Find Entertainment menu
        entertainment_menu = None
        for i in range(ide_instance.menubar.index(tk.END)):
            try:
                menu = ide_instance.menubar.nametowidget(ide_instance.menubar.entryconfig(i)['menu'])
                if menu.cget('text') == 'Entertainment':
                    entertainment_menu = menu
                    break
            except:
                continue
        
        if entertainment_menu:
            entertainment_menu.add_separator()
            entertainment_menu.add_command(label="📺 Free TV App", command=show_free_tv)
    
    print("📺 Free TV App integration added to IDE!")

if __name__ == "__main__":
    # Test the Free TV App
    print("📺 Testing Free TV App...")
    free_tv_app = FreeTVApp()
    print("✅ Free TV App ready!")
    print("🎬 Categories:", list(free_tv_app.categories.keys()))
    print("💎 Premium Sources:", list(free_tv_app.premium_sources.keys()))
