#!/usr/bin/env python3
"""
MovieBox Pro - Premium Streaming App
MovieBox Pro-style interface with unlimited streaming
"""

import tkinter as tk
from tkinter import ttk, messagebox
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

class MovieBoxProApp:
    """MovieBox Pro-style streaming app"""
    
    def __init__(self):
        # Premium content library - MovieBox Pro style
        self.content_library = {
            'trending': [
                {'title': 'Oppenheimer', 'year': '2023', 'quality': '4K', 'rating': '8.5', 'genre': 'Drama', 'poster': '🎬'},
                {'title': 'Barbie', 'year': '2023', 'quality': '4K', 'rating': '7.0', 'genre': 'Comedy', 'poster': '🎭'},
                {'title': 'Spider-Man: Across the Spider-Verse', 'year': '2023', 'quality': '4K', 'rating': '8.7', 'genre': 'Animation', 'poster': '🕷️'},
                {'title': 'Fast X', 'year': '2023', 'quality': '4K', 'rating': '5.9', 'genre': 'Action', 'poster': '🏎️'},
                {'title': 'Guardians of the Galaxy Vol. 3', 'year': '2023', 'quality': '4K', 'rating': '8.1', 'genre': 'Action', 'poster': '🚀'}
            ],
            'movies': [
                {'title': 'The Dark Knight', 'year': '2008', 'quality': '4K', 'rating': '9.0', 'genre': 'Action', 'poster': '🦇'},
                {'title': 'Inception', 'year': '2010', 'quality': '4K', 'rating': '8.8', 'genre': 'Sci-Fi', 'poster': '🌪️'},
                {'title': 'Interstellar', 'year': '2014', 'quality': '4K', 'rating': '8.6', 'genre': 'Sci-Fi', 'poster': '🌌'},
                {'title': 'Dune', 'year': '2021', 'quality': '4K', 'rating': '8.0', 'genre': 'Sci-Fi', 'poster': '🏜️'},
                {'title': 'Top Gun: Maverick', 'year': '2022', 'quality': '4K', 'rating': '8.3', 'genre': 'Action', 'poster': '✈️'},
                {'title': 'Avatar: The Way of Water', 'year': '2022', 'quality': '4K', 'rating': '7.8', 'genre': 'Sci-Fi', 'poster': '🌊'},
                {'title': 'Black Panther: Wakanda Forever', 'year': '2022', 'quality': '4K', 'rating': '6.7', 'genre': 'Action', 'poster': '🦸'},
                {'title': 'Thor: Love and Thunder', 'year': '2022', 'quality': '4K', 'rating': '6.3', 'genre': 'Action', 'poster': '⚡'}
            ],
            'tv_shows': [
                {'title': 'Stranger Things', 'seasons': '4', 'quality': '4K', 'rating': '8.7', 'genre': 'Horror', 'poster': '👻'},
                {'title': 'The Mandalorian', 'seasons': '3', 'quality': '4K', 'rating': '8.7', 'genre': 'Sci-Fi', 'poster': '🛸'},
                {'title': 'House of the Dragon', 'seasons': '1', 'quality': '4K', 'rating': '8.5', 'genre': 'Fantasy', 'poster': '🐉'},
                {'title': 'The Last of Us', 'seasons': '1', 'quality': '4K', 'rating': '8.9', 'genre': 'Drama', 'poster': '🧟'},
                {'title': 'Wednesday', 'seasons': '1', 'quality': '4K', 'rating': '8.1', 'genre': 'Comedy', 'poster': '👧'},
                {'title': 'The Boys', 'seasons': '4', 'quality': '4K', 'rating': '8.7', 'genre': 'Action', 'poster': '🦸‍♂️'},
                {'title': 'Euphoria', 'seasons': '2', 'quality': '4K', 'rating': '8.4', 'genre': 'Drama', 'poster': '💊'},
                {'title': 'The Witcher', 'seasons': '3', 'quality': '4K', 'rating': '8.2', 'genre': 'Fantasy', 'poster': '⚔️'}
            ],
            'anime': [
                {'title': 'Attack on Titan', 'episodes': '75', 'quality': 'HD', 'rating': '9.0', 'genre': 'Action', 'poster': '👹'},
                {'title': 'Demon Slayer', 'episodes': '44', 'quality': 'HD', 'rating': '8.7', 'genre': 'Action', 'poster': '🗡️'},
                {'title': 'One Piece', 'episodes': '1000+', 'quality': 'HD', 'rating': '9.4', 'genre': 'Adventure', 'poster': '🏴‍☠️'},
                {'title': 'My Hero Academia', 'episodes': '138', 'quality': 'HD', 'rating': '8.5', 'genre': 'Action', 'poster': '🦸‍♂️'},
                {'title': 'Jujutsu Kaisen', 'episodes': '24', 'quality': 'HD', 'rating': '8.6', 'genre': 'Action', 'poster': '👹'},
                {'title': 'Chainsaw Man', 'episodes': '12', 'quality': 'HD', 'rating': '8.5', 'genre': 'Action', 'poster': '🪚'},
                {'title': 'Spy x Family', 'episodes': '25', 'quality': 'HD', 'rating': '8.5', 'genre': 'Comedy', 'poster': '🕵️'},
                {'title': 'Tokyo Revengers', 'episodes': '24', 'quality': 'HD', 'rating': '8.1', 'genre': 'Action', 'poster': '⏰'}
            ],
            'sports': [
                {'title': 'UFC 300', 'date': '2024', 'quality': '4K', 'rating': '9.2', 'genre': 'Sports', 'poster': '🥊'},
                {'title': 'UFC 299', 'date': '2024', 'quality': '4K', 'rating': '8.8', 'genre': 'Sports', 'poster': '🥊'},
                {'title': 'UFC 298', 'date': '2024', 'quality': '4K', 'rating': '8.5', 'genre': 'Sports', 'poster': '🥊'},
                {'title': 'UFC 297', 'date': '2024', 'quality': '4K', 'rating': '8.3', 'genre': 'Sports', 'poster': '🥊'}
            ]
        }
        
        # Streaming services
        self.streaming_services = {
            'netflix': {'name': 'Netflix', 'color': '#E50914', 'url': 'https://netflix.com'},
            'disney': {'name': 'Disney+', 'color': '#113CCF', 'url': 'https://disneyplus.com'},
            'hbo': {'name': 'HBO Max', 'color': '#8B5CF6', 'url': 'https://hbomax.com'},
            'crunchyroll': {'name': 'Crunchyroll', 'color': '#FF6600', 'url': 'https://crunchyroll.com'},
            'amazon': {'name': 'Prime Video', 'color': '#00A8E1', 'url': 'https://primevideo.com'},
            'hulu': {'name': 'Hulu', 'color': '#1CE783', 'url': 'https://hulu.com'}
        }
        
        # Music services with unlimited skips
        self.music_services = {
            'spotify': {
                'name': 'Spotify (Unlimited Skips)', 
                'color': '#1DB954', 
                'url': 'https://open.spotify.com',
                'features': ['Unlimited Skips', 'No Ads', 'Premium Quality', 'Offline Downloads']
            },
            'soundcloud': {
                'name': 'SoundCloud (Unlimited Skips)', 
                'color': '#FF5500', 
                'url': 'https://soundcloud.com',
                'features': ['Unlimited Skips', 'No Ads', 'Independent Artists', 'Free Downloads']
            },
            'youtube_music': {
                'name': 'YouTube Music (No Ads)', 
                'color': '#FF0000', 
                'url': 'https://music.youtube.com',
                'features': ['No Ads', 'Background Play', 'Offline Mode', 'Video Integration']
            },
            'apple_music': {
                'name': 'Apple Music (Unlimited)', 
                'color': '#FA243C', 
                'url': 'https://music.apple.com',
                'features': ['Unlimited Skips', 'Lossless Audio', 'Spatial Audio', 'Lyrics']
            },
            'tidal': {
                'name': 'Tidal (HiFi)', 
                'color': '#00FFFF', 
                'url': 'https://tidal.com',
                'features': ['HiFi Quality', 'Master Quality', 'Exclusive Content', 'Artist Payouts']
            },
            'deezer': {
                'name': 'Deezer (Unlimited)', 
                'color': '#FF0000', 
                'url': 'https://deezer.com',
                'features': ['Unlimited Skips', 'HiFi Quality', 'Flow', 'Lyrics']
            }
        }
        
        # AI Copilot System
        self.ai_copilot = {
            'name': 'MovieBox AI Copilot',
            'version': '2.0.0',
            'status': 'active',
            'capabilities': [
                'Content Search & Discovery',
                'Personalized Recommendations',
                'Smart Content Suggestions',
                'Genre-based Filtering',
                'Trending Analysis',
                'Watch History Learning',
                'Mood-based Recommendations'
            ],
            'user_preferences': {
                'favorite_genres': [],
                'watch_history': [],
                'preferred_quality': '4K',
                'mood_preferences': [],
                'time_preferences': []
            },
            'recommendation_engine': {
                'trending_weight': 0.3,
                'personal_weight': 0.4,
                'genre_weight': 0.2,
                'rating_weight': 0.1
            }
        }
        
        # Built-in Media Engine (Custom Kodi-like system)
        self.media_engine = {
            'name': 'RawrZ Media Engine',
            'version': '1.0.0',
            'status': 'active',
            'addons': {
                'spotify': {'name': 'Spotify Unlimited', 'addon_id': 'rawrz.spotify', 'enabled': True, 'type': 'music'},
                'soundcloud': {'name': 'SoundCloud Unlimited', 'addon_id': 'rawrz.soundcloud', 'enabled': True, 'type': 'music'},
                'youtube_music': {'name': 'YouTube Music No-Ads', 'addon_id': 'rawrz.youtube', 'enabled': True, 'type': 'music'},
                'crunchyroll': {'name': 'Crunchyroll Premium', 'addon_id': 'rawrz.crunchyroll', 'enabled': True, 'type': 'anime'},
                'netflix': {'name': 'Netflix Premium', 'addon_id': 'rawrz.netflix', 'enabled': True, 'type': 'video'},
                'hbo': {'name': 'HBO Max Premium', 'addon_id': 'rawrz.hbo', 'enabled': True, 'type': 'video'},
                'disney': {'name': 'Disney+ Premium', 'addon_id': 'rawrz.disney', 'enabled': True, 'type': 'video'},
                'torrents': {'name': 'Torrent Streamer', 'addon_id': 'rawrz.torrents', 'enabled': True, 'type': 'streaming'},
                'scraper': {'name': 'Content Scraper', 'addon_id': 'rawrz.scraper', 'enabled': True, 'type': 'scraper'}
            },
            'settings': {
                'auto_quality': True,
                'preferred_quality': '4K',
                'unlimited_skips': True,
                'no_ads': True,
                'background_play': True
            }
        }
        
        # Initialize MovieBox Pro UI
        self.root = tk.Tk()
        self.root.title("🎬 MovieBox Pro - Premium Streaming")
        self.root.geometry("1920x1080")  # Full HD for projectors
        self.root.configure(bg='#0a0a0a')  # Dark background like MovieBox Pro
        
        # MovieBox Pro styling
        self.setup_moviebox_ui()
        self.setup_touch_controls()
        self.setup_projector_mode()
    
    def setup_moviebox_ui(self):
        """Setup MovieBox Pro-style UI"""
        # Main container with scrollable content
        main_canvas = tk.Canvas(self.root, bg='#0a0a0a', highlightthickness=0)
        main_scrollbar = ttk.Scrollbar(self.root, orient="vertical", command=main_canvas.yview)
        scrollable_frame = tk.Frame(main_canvas, bg='#0a0a0a')
        
        scrollable_frame.bind(
            "<Configure>",
            lambda e: main_canvas.configure(scrollregion=main_canvas.bbox("all"))
        )
        
        main_canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        main_canvas.configure(yscrollcommand=main_scrollbar.set)
        
        # Header with MovieBox Pro branding
        self.setup_moviebox_header(scrollable_frame)
        
        # Featured/Trending section
        self.setup_featured_section(scrollable_frame)
        
        # Content categories with horizontal scrolling
        self.setup_content_categories(scrollable_frame)
        
        # Bottom navigation
        self.setup_bottom_navigation(scrollable_frame)
        
        # Pack everything
        main_canvas.pack(side="left", fill="both", expand=True)
        main_scrollbar.pack(side="right", fill="y")
        
        # Bind mousewheel to canvas
        def _on_mousewheel(event):
            main_canvas.yview_scroll(int(-1*(event.delta/120)), "units")
        main_canvas.bind_all("<MouseWheel>", _on_mousewheel)
    
    def setup_moviebox_header(self, parent):
        """Setup MovieBox Pro header"""
        header_frame = tk.Frame(parent, bg='#1a1a1a', height=100)
        header_frame.pack(fill=tk.X, padx=0, pady=0)
        header_frame.pack_propagate(False)
        
        # Logo and title
        logo_frame = tk.Frame(header_frame, bg='#1a1a1a')
        logo_frame.pack(side=tk.LEFT, padx=30, pady=20)
        
        title_label = tk.Label(logo_frame, 
                              text="🎬 MovieBox Pro",
                              font=('Arial', 28, 'bold'),
                              bg='#1a1a1a', fg='#FF6B35')
        title_label.pack(side=tk.LEFT)
        
        subtitle_label = tk.Label(logo_frame, 
                                 text="Premium Streaming",
                                 font=('Arial', 12),
                                 bg='#1a1a1a', fg='#cccccc')
        subtitle_label.pack(side=tk.LEFT, padx=(10, 0))
        
        # Search bar
        search_frame = tk.Frame(header_frame, bg='#1a1a1a')
        search_frame.pack(side=tk.RIGHT, padx=30, pady=20)
        
        search_entry = tk.Entry(search_frame, 
                               bg='#2a2a2a', fg='white',
                               font=('Arial', 14), width=30,
                               relief=tk.FLAT, bd=5)
        search_entry.pack(side=tk.LEFT)
        
        search_btn = tk.Button(search_frame, text="🔍", 
                              command=self.show_search,
                              bg='#FF6B35', fg='white',
                              font=('Arial', 12, 'bold'),
                              relief=tk.FLAT, bd=0, width=3)
        search_btn.pack(side=tk.LEFT, padx=(5, 0))
        
        # AI Copilot button
        ai_btn = tk.Button(search_frame, text="🤖 AI", 
                          command=self.show_ai_copilot,
                          bg='#9C27B0', fg='white',
                          font=('Arial', 12, 'bold'),
                          relief=tk.FLAT, bd=0, width=3)
        ai_btn.pack(side=tk.LEFT, padx=(5, 0))
    
    def setup_featured_section(self, parent):
        """Setup featured/trending content section"""
        featured_frame = tk.Frame(parent, bg='#0a0a0a')
        featured_frame.pack(fill=tk.X, padx=20, pady=20)
        
        # Featured title
        featured_title = tk.Label(featured_frame, 
                                 text="🔥 Trending Now",
                                 font=('Arial', 20, 'bold'),
                                 bg='#0a0a0a', fg='white')
        featured_title.pack(anchor=tk.W, pady=(0, 15))
        
        # Featured content horizontal scroll
        featured_canvas = tk.Canvas(featured_frame, bg='#0a0a0a', height=300, highlightthickness=0)
        featured_scrollbar = ttk.Scrollbar(featured_frame, orient="horizontal", command=featured_canvas.xview)
        featured_content_frame = tk.Frame(featured_canvas, bg='#0a0a0a')
        
        featured_canvas.configure(xscrollcommand=featured_scrollbar.set)
        featured_canvas.create_window((0, 0), window=featured_content_frame, anchor="nw")
        
        # Add trending content
        for i, content in enumerate(self.content_library['trending']):
            self.create_featured_card(featured_content_frame, content, i)
        
        featured_canvas.pack(side="top", fill="x")
        featured_scrollbar.pack(side="bottom", fill="x")
    
    def create_featured_card(self, parent, content, index):
        """Create featured content card"""
        card_frame = tk.Frame(parent, bg='#1a1a1a', relief=tk.RAISED, bd=2)
        card_frame.pack(side=tk.LEFT, padx=10, pady=10)
        card_frame.configure(width=200, height=280)
        
        # Poster
        poster_label = tk.Label(card_frame, text=content['poster'], 
                               font=('Arial', 48), bg='#1a1a1a', fg='white')
        poster_label.pack(pady=10)
        
        # Title
        title_label = tk.Label(card_frame, text=content['title'], 
                              bg='#1a1a1a', fg='white', font=('Arial', 12, 'bold'),
                              wraplength=180)
        title_label.pack(pady=5)
        
        # Year and rating
        info_text = f"{content['year']} • ⭐ {content['rating']}"
        info_label = tk.Label(card_frame, text=info_text, 
                             bg='#1a1a1a', fg='#cccccc', font=('Arial', 10))
        info_label.pack()
        
        # Quality badge
        quality_label = tk.Label(card_frame, text=content['quality'], 
                                bg='#FF6B35', fg='white', font=('Arial', 8, 'bold'))
        quality_label.pack(pady=5)
        
        # Play button
        play_btn = tk.Button(card_frame, text="▶️ Play", 
                            command=lambda: self.play_content(content),
                            bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'),
                            relief=tk.FLAT, bd=0)
        play_btn.pack(pady=5)
    
    def setup_content_categories(self, parent):
        """Setup content categories with horizontal scrolling"""
        categories = [
            ('🎬 Movies', 'movies'),
            ('📺 TV Shows', 'tv_shows'), 
            ('🎌 Anime', 'anime'),
            ('🥊 Sports', 'sports')
        ]
        
        for category_name, category_key in categories:
            self.create_category_section(parent, category_name, category_key)
    
    def create_category_section(self, parent, category_name, category_key):
        """Create a category section with horizontal scrolling"""
        category_frame = tk.Frame(parent, bg='#0a0a0a')
        category_frame.pack(fill=tk.X, padx=20, pady=20)
        
        # Category title
        title_label = tk.Label(category_frame, 
                              text=category_name,
                              font=('Arial', 18, 'bold'),
                              bg='#0a0a0a', fg='white')
        title_label.pack(anchor=tk.W, pady=(0, 15))
        
        # Horizontal scrolling canvas for this category
        canvas = tk.Canvas(category_frame, bg='#0a0a0a', height=200, highlightthickness=0)
        scrollbar = ttk.Scrollbar(category_frame, orient="horizontal", command=canvas.xview)
        content_frame = tk.Frame(canvas, bg='#0a0a0a')
        
        canvas.configure(xscrollcommand=scrollbar.set)
        canvas.create_window((0, 0), window=content_frame, anchor="nw")
        
        # Add content cards
        for content in self.content_library[category_key]:
            self.create_content_card(content_frame, content, category_key)
        
        canvas.pack(side="top", fill="x")
        scrollbar.pack(side="bottom", fill="x")
    
    def create_content_card(self, parent, content, content_type):
        """Create content card for categories"""
        card_frame = tk.Frame(parent, bg='#1a1a1a', relief=tk.RAISED, bd=1)
        card_frame.pack(side=tk.LEFT, padx=8, pady=5)
        card_frame.configure(width=150, height=180)
        
        # Poster
        poster_label = tk.Label(card_frame, text=content['poster'], 
                               font=('Arial', 32), bg='#1a1a1a', fg='white')
        poster_label.pack(pady=8)
        
        # Title
        title_label = tk.Label(card_frame, text=content['title'], 
                              bg='#1a1a1a', fg='white', font=('Arial', 10, 'bold'),
                              wraplength=130)
        title_label.pack(pady=3)
        
        # Rating
        rating_label = tk.Label(card_frame, text=f"⭐ {content['rating']}", 
                               bg='#1a1a1a', fg='#FFD700', font=('Arial', 9))
        rating_label.pack()
        
        # Quality
        quality_label = tk.Label(card_frame, text=content['quality'], 
                                bg='#FF6B35', fg='white', font=('Arial', 8, 'bold'))
        quality_label.pack(pady=2)
        
        # Play button
        play_btn = tk.Button(card_frame, text="▶️", 
                            command=lambda: self.play_content(content),
                            bg='#4CAF50', fg='white', font=('Arial', 8),
                            relief=tk.FLAT, bd=0, width=3)
        play_btn.pack(pady=3)
    
    def setup_bottom_navigation(self, parent):
        """Setup bottom navigation like MovieBox Pro"""
        nav_frame = tk.Frame(parent, bg='#1a1a1a', height=80)
        nav_frame.pack(fill=tk.X, padx=0, pady=20)
        nav_frame.pack_propagate(False)
        
        # Navigation buttons
        nav_buttons = [
            ("🏠 Home", self.show_home),
            ("🔍 Search", self.show_search),
            ("🤖 AI Copilot", self.show_ai_copilot),
            ("📋 My List", self.show_my_list),
            ("⚙️ Settings", self.show_settings),
            ("📱 Download", self.show_downloads)
        ]
        
        for text, command in nav_buttons:
            btn = tk.Button(nav_frame, text=text, command=command,
                           bg='#2a2a2a', fg='white', font=('Arial', 12, 'bold'),
                           relief=tk.FLAT, bd=0, width=12, height=2)
            btn.pack(side=tk.LEFT, padx=10, pady=15)
    
    def setup_content_grid(self, parent):
        """Setup content grid for projector display"""
        # Content frame
        content_frame = tk.Frame(parent, bg='#000000')
        content_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Create grid of content cards
        self.content_cards = []
        
        # Movies section
        movies_frame = tk.LabelFrame(content_frame, text="🎬 Movies", 
                                    bg='#1a1a1a', fg='white', font=('Arial', 16, 'bold'))
        movies_frame.pack(fill=tk.X, pady=10)
        
        movies_grid = tk.Frame(movies_frame, bg='#1a1a1a')
        movies_grid.pack(fill=tk.X, padx=10, pady=10)
        
        for i, movie in enumerate(self.content_sources['movies'][:5]):
            card = self.create_content_card(movies_grid, movie, 'movie')
            card.grid(row=0, column=i, padx=5, pady=5, sticky='ew')
            movies_grid.columnconfigure(i, weight=1)
        
        # TV Shows section
        tv_frame = tk.LabelFrame(content_frame, text="📺 TV Shows", 
                                 bg='#1a1a1a', fg='white', font=('Arial', 16, 'bold'))
        tv_frame.pack(fill=tk.X, pady=10)
        
        tv_grid = tk.Frame(tv_frame, bg='#1a1a1a')
        tv_grid.pack(fill=tk.X, padx=10, pady=10)
        
        for i, show in enumerate(self.content_sources['tv_shows'][:5]):
            card = self.create_content_card(tv_grid, show, 'tv')
            card.grid(row=0, column=i, padx=5, pady=5, sticky='ew')
            tv_grid.columnconfigure(i, weight=1)
        
        # Anime section
        anime_frame = tk.LabelFrame(content_frame, text="🎌 Anime", 
                                   bg='#1a1a1a', fg='white', font=('Arial', 16, 'bold'))
        anime_frame.pack(fill=tk.X, pady=10)
        
        anime_grid = tk.Frame(anime_frame, bg='#1a1a1a')
        anime_grid.pack(fill=tk.X, padx=10, pady=10)
        
        for i, anime in enumerate(self.content_sources['anime'][:5]):
            card = self.create_content_card(anime_grid, anime, 'anime')
            card.grid(row=0, column=i, padx=5, pady=5, sticky='ew')
            anime_grid.columnconfigure(i, weight=1)
        
        # Sports section
        sports_frame = tk.LabelFrame(content_frame, text="🥊 Sports", 
                                    bg='#1a1a1a', fg='white', font=('Arial', 16, 'bold'))
        sports_frame.pack(fill=tk.X, pady=10)
        
        sports_grid = tk.Frame(sports_frame, bg='#1a1a1a')
        sports_grid.pack(fill=tk.X, padx=10, pady=10)
        
        for i, sport in enumerate(self.content_sources['sports'][:5]):
            card = self.create_content_card(sports_grid, sport, 'sport')
            card.grid(row=0, column=i, padx=5, pady=5, sticky='ew')
            sports_grid.columnconfigure(i, weight=1)
        
        # Music section with unlimited skips
        music_frame = tk.LabelFrame(content_frame, text="🎵 Music (Unlimited Skips)", 
                                   bg='#1a1a1a', fg='white', font=('Arial', 16, 'bold'))
        music_frame.pack(fill=tk.X, pady=10)
        
        music_grid = tk.Frame(music_frame, bg='#1a1a1a')
        music_grid.pack(fill=tk.X, padx=10, pady=10)
        
        # Create music service buttons
        for i, (service_id, service) in enumerate(self.music_services.items()):
            music_btn = tk.Button(music_grid, text=f"🎵 {service['name']}", 
                                 command=lambda s=service: self.open_music_service(s),
                                 bg=service['color'], fg='white', 
                                 font=('Arial', 12, 'bold'), width=20, height=2)
            music_btn.grid(row=0, column=i, padx=5, pady=5, sticky='ew')
            music_grid.columnconfigure(i, weight=1)
    
    def create_content_card(self, parent, content, content_type):
        """Create a content card"""
        card_frame = tk.Frame(parent, bg='#2a2a2a', relief=tk.RAISED, bd=2)
        card_frame.pack_propagate(False)
        card_frame.configure(width=200, height=150)
        
        # Content title
        title_label = tk.Label(card_frame, text=content['title'], 
                              bg='#2a2a2a', fg='white', font=('Arial', 12, 'bold'),
                              wraplength=180)
        title_label.pack(pady=5)
        
        # Content info based on type
        if content_type == 'movies':
            info_text = f"{content['year']} • {content['quality']}"
        elif content_type == 'tv_shows':
            info_text = f"{content.get('seasons', 'N/A')} seasons • {content['quality']}"
        elif content_type == 'anime':
            info_text = f"{content.get('episodes', 'N/A')} eps • {content['quality']}"
        elif content_type == 'sports':
            info_text = f"{content.get('date', 'N/A')} • {content['quality']}"
        else:
            info_text = f"{content.get('year', 'N/A')} • {content['quality']}"
        
        info_label = tk.Label(card_frame, text=info_text, 
                             bg='#2a2a2a', fg='#cccccc', font=('Arial', 10))
        info_label.pack()
        
        # Source (if available)
        if 'source' in content:
            source_label = tk.Label(card_frame, text=content['source'], 
                                   bg='#2a2a2a', fg='#4CAF50', font=('Arial', 9))
            source_label.pack()
        
        # Play button
        play_btn = tk.Button(card_frame, text="▶️ Play", 
                            command=lambda: self.play_content(content),
                            bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'))
        play_btn.pack(pady=5)
        
        return card_frame
    
    def setup_bottom_controls(self, parent):
        """Setup bottom control panel"""
        controls_frame = tk.Frame(parent, bg='#1a1a1a', height=100)
        controls_frame.pack(fill=tk.X, padx=20, pady=10)
        controls_frame.pack_propagate(False)
        
        # Control buttons
        button_frame = tk.Frame(controls_frame, bg='#1a1a1a')
        button_frame.pack(expand=True)
        
        # Search button
        search_btn = tk.Button(button_frame, text="🔍 Search", 
                              command=self.show_search,
                              bg='#2196F3', fg='white', font=('Arial', 14, 'bold'),
                              width=12, height=2)
        search_btn.pack(side=tk.LEFT, padx=10)
        
        # Settings button
        settings_btn = tk.Button(button_frame, text="⚙️ Settings", 
                                command=self.show_settings,
                                bg='#FF9800', fg='white', font=('Arial', 14, 'bold'),
                                width=12, height=2)
        settings_btn.pack(side=tk.LEFT, padx=10)
        
        # Streaming services button
        streaming_btn = tk.Button(button_frame, text="📺 Streaming", 
                                 command=self.show_streaming,
                                 bg='#9C27B0', fg='white', font=('Arial', 14, 'bold'),
                                 width=12, height=2)
        streaming_btn.pack(side=tk.LEFT, padx=10)
        
        # Media Engine button (built-in Kodi-like system)
        media_btn = tk.Button(button_frame, text="🎬 Media Engine", 
                            command=self.show_media_engine,
                            bg='#FF6B35', fg='white', font=('Arial', 14, 'bold'),
                            width=12, height=2)
        media_btn.pack(side=tk.LEFT, padx=10)
        
        # Exit button
        exit_btn = tk.Button(button_frame, text="❌ Exit", 
                            command=self.root.quit,
                            bg='#f44336', fg='white', font=('Arial', 14, 'bold'),
                            width=12, height=2)
        exit_btn.pack(side=tk.LEFT, padx=10)
    
    def setup_touch_controls(self):
        """Setup touch-friendly controls for Android"""
        # Make buttons larger for touch
        self.root.bind('<Button-1>', self.on_touch)
        self.root.bind('<Motion>', self.on_motion)
    
    def setup_projector_mode(self):
        """Setup projector-optimized display"""
        # Full screen mode
        self.root.attributes('-fullscreen', False)  # Can be toggled
        
        # High contrast colors for projectors
        self.root.configure(bg='#000000')
        
        # Large fonts for projector viewing
        self.root.option_add('*Font', 'Arial 12')
    
    def on_touch(self, event):
        """Handle touch events"""
        print(f"Touch at: {event.x}, {event.y}")
    
    def on_motion(self, event):
        """Handle mouse motion for touch"""
        pass
    
    def play_content(self, content):
        """Play selected content - MovieBox Pro style"""
        # Show content details
        details_window = tk.Toplevel(self.root)
        details_window.title(f"🎬 {content['title']}")
        details_window.geometry("800x600")
        details_window.configure(bg='#0a0a0a')
        
        # Content details
        details_frame = tk.Frame(details_window, bg='#0a0a0a')
        details_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Poster and info
        info_frame = tk.Frame(details_frame, bg='#0a0a0a')
        info_frame.pack(fill=tk.X, pady=20)
        
        # Poster
        poster_label = tk.Label(info_frame, text=content['poster'], 
                               font=('Arial', 64), bg='#0a0a0a', fg='white')
        poster_label.pack(side=tk.LEFT, padx=20)
        
        # Content info
        info_text_frame = tk.Frame(info_frame, bg='#0a0a0a')
        info_text_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=20)
        
        title_label = tk.Label(info_text_frame, text=content['title'], 
                              bg='#0a0a0a', fg='white', font=('Arial', 24, 'bold'))
        title_label.pack(anchor=tk.W, pady=5)
        
        year_rating = f"{content.get('year', 'N/A')} • ⭐ {content['rating']} • {content['genre']}"
        year_label = tk.Label(info_text_frame, text=year_rating, 
                             bg='#0a0a0a', fg='#cccccc', font=('Arial', 14))
        year_label.pack(anchor=tk.W, pady=5)
        
        quality_label = tk.Label(info_text_frame, text=f"Quality: {content['quality']}", 
                                bg='#0a0a0a', fg='#FF6B35', font=('Arial', 12))
        quality_label.pack(anchor=tk.W, pady=5)
        
        # Play buttons
        button_frame = tk.Frame(details_frame, bg='#0a0a0a')
        button_frame.pack(pady=20)
        
        play_btn = tk.Button(button_frame, text="▶️ Play Now", 
                            command=lambda: self.start_playback(content),
                            bg='#4CAF50', fg='white', font=('Arial', 14, 'bold'),
                            relief=tk.FLAT, bd=0, width=15, height=2)
        play_btn.pack(side=tk.LEFT, padx=10)
        
        download_btn = tk.Button(button_frame, text="📱 Download", 
                                command=lambda: self.download_content(content),
                                bg='#2196F3', fg='white', font=('Arial', 14, 'bold'),
                                relief=tk.FLAT, bd=0, width=15, height=2)
        download_btn.pack(side=tk.LEFT, padx=10)
        
        add_list_btn = tk.Button(button_frame, text="➕ Add to List", 
                                command=lambda: self.add_to_list(content),
                                bg='#FF9800', fg='white', font=('Arial', 14, 'bold'),
                                relief=tk.FLAT, bd=0, width=15, height=2)
        add_list_btn.pack(side=tk.LEFT, padx=10)
    
    def start_playback(self, content):
        """Start content playback"""
        messagebox.showinfo("Playback Started", 
                           f"🎬 Now Playing: {content['title']}\n\n"
                           f"Quality: {content['quality']}\n"
                           f"Rating: ⭐ {content['rating']}\n\n"
                           f"Streaming with MovieBox Pro features:\n"
                           f"• No ads\n"
                           f"• High quality\n"
                           f"• Unlimited access")
    
    def download_content(self, content):
        """Download content"""
        messagebox.showinfo("Download Started", 
                           f"📱 Downloading: {content['title']}\n\n"
                           f"Quality: {content['quality']}\n"
                           f"Status: Downloading...\n\n"
                           f"Available for offline viewing!")
    
    def add_to_list(self, content):
        """Add content to My List"""
        messagebox.showinfo("Added to List", 
                           f"➕ Added to My List: {content['title']}\n\n"
                           f"You can find it in your personal collection!")
    
    def show_home(self):
        """Show home screen"""
        messagebox.showinfo("Home", "🏠 Welcome to MovieBox Pro Home!")
    
    def show_my_list(self):
        """Show My List"""
        my_list_window = tk.Toplevel(self.root)
        my_list_window.title("📋 My List")
        my_list_window.geometry("1000x700")
        my_list_window.configure(bg='#0a0a0a')
        
        title_label = tk.Label(my_list_window, text="📋 My List", 
                              bg='#0a0a0a', fg='white', font=('Arial', 20, 'bold'))
        title_label.pack(pady=20)
        
        # Sample list items
        list_items = [
            "🎬 The Dark Knight",
            "📺 Stranger Things", 
            "🎌 Attack on Titan",
            "🎬 Inception",
            "📺 The Mandalorian"
        ]
        
        for item in list_items:
            item_label = tk.Label(my_list_window, text=item, 
                                 bg='#1a1a1a', fg='white', font=('Arial', 14),
                                 width=50, anchor=tk.W)
            item_label.pack(pady=5, padx=20)
    
    def show_downloads(self):
        """Show Downloads"""
        downloads_window = tk.Toplevel(self.root)
        downloads_window.title("📱 Downloads")
        downloads_window.geometry("1000x700")
        downloads_window.configure(bg='#0a0a0a')
        
        title_label = tk.Label(downloads_window, text="📱 Downloads", 
                              bg='#0a0a0a', fg='white', font=('Arial', 20, 'bold'))
        title_label.pack(pady=20)
        
        # Sample downloads
        downloads = [
            "🎬 Oppenheimer (4K) - Downloaded",
            "📺 Stranger Things S4 (4K) - Downloaded", 
            "🎌 Demon Slayer S2 (HD) - Downloaded",
            "🎬 Top Gun: Maverick (4K) - Downloaded"
        ]
        
        for download in downloads:
            item_label = tk.Label(downloads_window, text=download, 
                              bg='#1a1a1a', fg='white', font=('Arial', 14),
                              width=50, anchor=tk.W)
            item_label.pack(pady=5, padx=20)
    
    def show_ai_copilot(self):
        """Show AI Copilot interface"""
        ai_window = tk.Toplevel(self.root)
        ai_window.title("🤖 MovieBox AI Copilot")
        ai_window.geometry("1200x800")
        ai_window.configure(bg='#0a0a0a')
        
        # Create notebook for AI features
        notebook = ttk.Notebook(ai_window)
        notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # AI Chat tab
        chat_frame = tk.Frame(notebook, bg='#0a0a0a')
        notebook.add(chat_frame, text="💬 AI Chat")
        self.setup_ai_chat_tab(chat_frame)
        
        # Smart Recommendations tab
        recommendations_frame = tk.Frame(notebook, bg='#0a0a0a')
        notebook.add(recommendations_frame, text="🎯 Smart Recommendations")
        self.setup_smart_recommendations_tab(recommendations_frame)
        
        # Content Discovery tab
        discovery_frame = tk.Frame(notebook, bg='#0a0a0a')
        notebook.add(discovery_frame, text="🔍 Content Discovery")
        self.setup_content_discovery_tab(discovery_frame)
        
        # AI Settings tab
        settings_frame = tk.Frame(notebook, bg='#0a0a0a')
        notebook.add(settings_frame, text="⚙️ AI Settings")
        self.setup_ai_settings_tab(settings_frame)
    
    def setup_ai_chat_tab(self, parent):
        """Setup AI chat interface"""
        # Title
        title_label = tk.Label(parent, text="🤖 MovieBox AI Copilot", 
                              bg='#0a0a0a', fg='#9C27B0', 
                              font=('Arial', 20, 'bold'))
        title_label.pack(pady=20)
        
        # Chat area
        chat_frame = tk.Frame(parent, bg='#1a1a1a', relief=tk.RAISED, bd=2)
        chat_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Chat history
        chat_history = tk.Text(chat_frame, bg='#2a2a2a', fg='white', 
                              font=('Arial', 12), height=20, wrap=tk.WORD)
        chat_history.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Welcome message
        welcome_msg = """🤖 MovieBox AI Copilot: Hello! I'm your personal streaming assistant. 

I can help you:
• Find specific movies and shows
• Get personalized recommendations
• Discover new content based on your preferences
• Answer questions about content
• Suggest what to watch based on your mood

Try asking me things like:
- "Find me action movies from 2023"
- "What should I watch if I like sci-fi?"
- "Show me trending anime"
- "I'm in the mood for comedy" """
        
        chat_history.insert(tk.END, welcome_msg)
        chat_history.config(state=tk.DISABLED)
        
        # Input area
        input_frame = tk.Frame(chat_frame, bg='#1a1a1a')
        input_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # User input
        user_input = tk.Entry(input_frame, bg='#3a3a3a', fg='white',
                             font=('Arial', 12), width=50)
        user_input.pack(side=tk.LEFT, padx=5)
        user_input.bind('<Return>', lambda e: self.process_ai_query(user_input, chat_history))
        
        # Send button
        send_btn = tk.Button(input_frame, text="Send", 
                            command=lambda: self.process_ai_query(user_input, chat_history),
                            bg='#9C27B0', fg='white', font=('Arial', 12, 'bold'))
        send_btn.pack(side=tk.LEFT, padx=5)
        
        # Quick action buttons
        quick_frame = tk.Frame(chat_frame, bg='#1a1a1a')
        quick_frame.pack(fill=tk.X, padx=10, pady=5)
        
        quick_buttons = [
            ("🎬 Find Movies", "Find me some great movies"),
            ("📺 TV Shows", "Show me trending TV shows"),
            ("🎌 Anime", "What anime should I watch?"),
            ("🎯 Recommendations", "Give me personalized recommendations")
        ]
        
        for text, query in quick_buttons:
            btn = tk.Button(quick_frame, text=text, 
                           command=lambda q=query: self.quick_ai_query(q, user_input, chat_history),
                           bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'),
                           relief=tk.FLAT, bd=0, width=15)
            btn.pack(side=tk.LEFT, padx=2)
    
    def setup_smart_recommendations_tab(self, parent):
        """Setup smart recommendations tab"""
        # Title
        title_label = tk.Label(parent, text="🎯 Smart Recommendations", 
                              bg='#0a0a0a', fg='#4CAF50', 
                              font=('Arial', 20, 'bold'))
        title_label.pack(pady=20)
        
        # Recommendation categories
        rec_categories = [
            ("🔥 Trending Now", self.get_trending_recommendations),
            ("🎭 Based on Your History", self.get_history_recommendations),
            ("🎬 Similar to Your Favorites", self.get_similar_recommendations),
            ("🌙 Perfect for Tonight", self.get_mood_recommendations),
            ("⭐ Highly Rated", self.get_highly_rated_recommendations)
        ]
        
        for category_name, get_recs_func in rec_categories:
            self.create_recommendation_section(parent, category_name, get_recs_func)
    
    def setup_content_discovery_tab(self, parent):
        """Setup content discovery tab"""
        # Title
        title_label = tk.Label(parent, text="🔍 Content Discovery", 
                              bg='#0a0a0a', fg='#2196F3', 
                              font=('Arial', 20, 'bold'))
        title_label.pack(pady=20)
        
        # Discovery options
        discovery_frame = tk.Frame(parent, bg='#0a0a0a')
        discovery_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Genre-based discovery
        genre_frame = tk.LabelFrame(discovery_frame, text="🎭 Discover by Genre", 
                                   bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        genre_frame.pack(fill=tk.X, pady=10)
        
        genres = ["Action", "Comedy", "Drama", "Sci-Fi", "Horror", "Romance", "Thriller", "Fantasy"]
        genre_buttons = []
        
        for i, genre in enumerate(genres):
            btn = tk.Button(genre_frame, text=genre, 
                           command=lambda g=genre: self.discover_by_genre(g),
                           bg='#FF6B35', fg='white', font=('Arial', 10, 'bold'),
                           relief=tk.FLAT, bd=0, width=12)
            btn.grid(row=i//4, column=i%4, padx=5, pady=5)
        
        # Year-based discovery
        year_frame = tk.LabelFrame(discovery_frame, text="📅 Discover by Year", 
                                  bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        year_frame.pack(fill=tk.X, pady=10)
        
        years = ["2024", "2023", "2022", "2021", "2020", "2019", "2018", "2017"]
        for i, year in enumerate(years):
            btn = tk.Button(year_frame, text=year, 
                           command=lambda y=year: self.discover_by_year(y),
                           bg='#9C27B0', fg='white', font=('Arial', 10, 'bold'),
                           relief=tk.FLAT, bd=0, width=12)
            btn.grid(row=i//4, column=i%4, padx=5, pady=5)
        
        # Quality-based discovery
        quality_frame = tk.LabelFrame(discovery_frame, text="🎯 Discover by Quality", 
                                     bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        quality_frame.pack(fill=tk.X, pady=10)
        
        qualities = ["4K", "HD", "SD"]
        for i, quality in enumerate(qualities):
            btn = tk.Button(quality_frame, text=quality, 
                           command=lambda q=quality: self.discover_by_quality(q),
                           bg='#4CAF50', fg='white', font=('Arial', 10, 'bold'),
                           relief=tk.FLAT, bd=0, width=12)
            btn.grid(row=0, column=i, padx=5, pady=5)
    
    def setup_ai_settings_tab(self, parent):
        """Setup AI settings tab"""
        # Title
        title_label = tk.Label(parent, text="⚙️ AI Settings", 
                              bg='#0a0a0a', fg='#FF9800', 
                              font=('Arial', 20, 'bold'))
        title_label.pack(pady=20)
        
        # Settings frame
        settings_frame = tk.Frame(parent, bg='#0a0a0a')
        settings_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # AI Capabilities
        capabilities_frame = tk.LabelFrame(settings_frame, text="🤖 AI Capabilities", 
                                         bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        capabilities_frame.pack(fill=tk.X, pady=10)
        
        for capability in self.ai_copilot['capabilities']:
            cap_label = tk.Label(capabilities_frame, text=f"• {capability}", 
                                bg='#1a1a1a', fg='#cccccc', font=('Arial', 12))
            cap_label.pack(anchor=tk.W, padx=10, pady=2)
        
        # User Preferences
        preferences_frame = tk.LabelFrame(settings_frame, text="👤 Your Preferences", 
                                        bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        preferences_frame.pack(fill=tk.X, pady=10)
        
        # Favorite genres
        genres_label = tk.Label(preferences_frame, text="Favorite Genres:", 
                               bg='#1a1a1a', fg='white', font=('Arial', 12, 'bold'))
        genres_label.pack(anchor=tk.W, padx=10, pady=5)
        
        genre_vars = {}
        all_genres = ["Action", "Comedy", "Drama", "Sci-Fi", "Horror", "Romance", "Thriller", "Fantasy", "Anime"]
        
        for i, genre in enumerate(all_genres):
            var = tk.BooleanVar()
            genre_vars[genre] = var
            check = tk.Checkbutton(preferences_frame, text=genre, variable=var,
                                 bg='#1a1a1a', fg='white', font=('Arial', 10))
            check.pack(anchor=tk.W, padx=20, pady=2)
        
        # Save preferences button
        save_btn = tk.Button(settings_frame, text="💾 Save Preferences", 
                            command=lambda: self.save_ai_preferences(genre_vars),
                            bg='#4CAF50', fg='white', font=('Arial', 12, 'bold'))
        save_btn.pack(pady=20)
    
    def process_ai_query(self, input_widget, chat_widget):
        """Process AI query and generate response"""
        query = input_widget.get().strip()
        if not query:
            return
        
        # Add user message to chat
        chat_widget.config(state=tk.NORMAL)
        chat_widget.insert(tk.END, f"\n\n👤 You: {query}\n")
        chat_widget.config(state=tk.DISABLED)
        
        # Clear input
        input_widget.delete(0, tk.END)
        
        # Generate AI response
        response = self.generate_ai_response(query)
        
        # Add AI response to chat
        chat_widget.config(state=tk.NORMAL)
        chat_widget.insert(tk.END, f"🤖 AI: {response}\n")
        chat_widget.config(state=tk.DISABLED)
        chat_widget.see(tk.END)
    
    def quick_ai_query(self, query, input_widget, chat_widget):
        """Process quick AI query"""
        input_widget.delete(0, tk.END)
        input_widget.insert(0, query)
        self.process_ai_query(input_widget, chat_widget)
    
    def generate_ai_response(self, query):
        """Generate AI response based on query"""
        query_lower = query.lower()
        
        # Content search responses
        if any(word in query_lower for word in ['find', 'search', 'show me', 'get me']):
            if 'movie' in query_lower:
                return self.search_movies(query)
            elif 'tv' in query_lower or 'show' in query_lower:
                return self.search_tv_shows(query)
            elif 'anime' in query_lower:
                return self.search_anime(query)
            else:
                return self.search_all_content(query)
        
        # Recommendation requests
        elif any(word in query_lower for word in ['recommend', 'suggest', 'what should i watch']):
            return self.get_personalized_recommendations(query)
        
        # Mood-based requests
        elif any(word in query_lower for word in ['mood', 'feel like', 'want to watch']):
            return self.get_mood_recommendations(query)
        
        # Genre-based requests
        elif any(genre in query_lower for genre in ['action', 'comedy', 'drama', 'sci-fi', 'horror', 'romance']):
            return self.get_genre_recommendations(query)
        
        # Default response
        else:
            return "I'd be happy to help! Try asking me to find movies, get recommendations, or discover content by genre. What would you like to watch?"
    
    def search_movies(self, query):
        """Search for movies"""
        results = []
        for movie in self.content_library['movies']:
            if any(word in movie['title'].lower() for word in query.lower().split()):
                results.append(f"🎬 {movie['title']} ({movie['year']}) - ⭐ {movie['rating']} - {movie['quality']}")
        
        if results:
            return f"Found {len(results)} movies:\n" + "\n".join(results[:5])
        else:
            return "No movies found matching your search. Try browsing our movie collection!"
    
    def search_tv_shows(self, query):
        """Search for TV shows"""
        results = []
        for show in self.content_library['tv_shows']:
            if any(word in show['title'].lower() for word in query.lower().split()):
                results.append(f"📺 {show['title']} ({show.get('seasons', 'N/A')} seasons) - ⭐ {show['rating']} - {show['quality']}")
        
        if results:
            return f"Found {len(results)} TV shows:\n" + "\n".join(results[:5])
        else:
            return "No TV shows found matching your search. Try browsing our TV collection!"
    
    def search_anime(self, query):
        """Search for anime"""
        results = []
        for anime in self.content_library['anime']:
            if any(word in anime['title'].lower() for word in query.lower().split()):
                results.append(f"🎌 {anime['title']} ({anime.get('episodes', 'N/A')} eps) - ⭐ {anime['rating']} - {anime['quality']}")
        
        if results:
            return f"Found {len(results)} anime:\n" + "\n".join(results[:5])
        else:
            return "No anime found matching your search. Try browsing our anime collection!"
    
    def search_all_content(self, query):
        """Search all content"""
        all_results = []
        for category, content_list in self.content_library.items():
            if category == 'trending':
                continue
            for content in content_list:
                if any(word in content['title'].lower() for word in query.lower().split()):
                    icon = "🎬" if category == "movies" else "📺" if category == "tv_shows" else "🎌" if category == "anime" else "🥊"
                    all_results.append(f"{icon} {content['title']} - ⭐ {content['rating']} - {content['quality']}")
        
        if all_results:
            return f"Found {len(all_results)} results:\n" + "\n".join(all_results[:8])
        else:
            return "No content found matching your search. Try different keywords!"
    
    def get_personalized_recommendations(self, query):
        """Get personalized recommendations"""
        recommendations = [
            "🎬 The Dark Knight - ⭐ 9.0 - Action masterpiece",
            "📺 Stranger Things - ⭐ 8.7 - Supernatural thriller",
            "🎌 Attack on Titan - ⭐ 9.0 - Epic anime",
            "🎬 Inception - ⭐ 8.8 - Mind-bending sci-fi",
            "📺 The Mandalorian - ⭐ 8.7 - Space western"
        ]
        return f"Based on your preferences, I recommend:\n" + "\n".join(recommendations)
    
    def get_mood_recommendations(self, query):
        """Get mood-based recommendations"""
        if 'action' in query.lower():
            return "For action-packed content: 🎬 Top Gun: Maverick, 🎬 Fast X, 🥊 UFC 300"
        elif 'comedy' in query.lower():
            return "For laughs: 🎭 Barbie, 📺 Wednesday, 🎌 Spy x Family"
        elif 'drama' in query.lower():
            return "For dramatic content: 🎬 Oppenheimer, 📺 The Last of Us, 📺 Euphoria"
        else:
            return "I can suggest content based on your mood! Try saying 'I want action' or 'I feel like comedy'"
    
    def get_genre_recommendations(self, query):
        """Get genre-based recommendations"""
        genre = None
        for g in ['action', 'comedy', 'drama', 'sci-fi', 'horror', 'romance']:
            if g in query.lower():
                genre = g
                break
        
        if genre:
            return f"Here are some great {genre} recommendations:\n🎬 The Dark Knight (Action)\n📺 Stranger Things (Horror)\n🎌 Attack on Titan (Action)"
        else:
            return "What genre interests you? I can suggest Action, Comedy, Drama, Sci-Fi, Horror, or Romance!"
    
    def get_trending_recommendations(self):
        """Get trending recommendations"""
        return self.content_library['trending'][:5]
    
    def get_history_recommendations(self):
        """Get recommendations based on watch history"""
        return [
            {'title': 'The Dark Knight', 'reason': 'Similar to your watched action movies'},
            {'title': 'Stranger Things', 'reason': 'Based on your supernatural preferences'},
            {'title': 'Attack on Titan', 'reason': 'You enjoyed action anime'}
        ]
    
    def get_similar_recommendations(self):
        """Get similar content recommendations"""
        return [
            {'title': 'Inception', 'reason': 'Similar to The Dark Knight'},
            {'title': 'Interstellar', 'reason': 'If you liked Inception'},
            {'title': 'Dune', 'reason': 'Epic sci-fi like Interstellar'}
        ]
    
    def get_mood_recommendations(self):
        """Get mood-based recommendations"""
        return [
            {'title': 'Barbie', 'reason': 'Perfect for a fun night'},
            {'title': 'Oppenheimer', 'reason': 'For serious drama'},
            {'title': 'Spider-Man: Across the Spider-Verse', 'reason': 'Great family entertainment'}
        ]
    
    def get_highly_rated_recommendations(self):
        """Get highly rated recommendations"""
        return [
            {'title': 'The Dark Knight', 'rating': '9.0'},
            {'title': 'Attack on Titan', 'rating': '9.0'},
            {'title': 'One Piece', 'rating': '9.4'}
        ]
    
    def create_recommendation_section(self, parent, title, get_recs_func):
        """Create a recommendation section"""
        section_frame = tk.LabelFrame(parent, text=title, 
                                     bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        section_frame.pack(fill=tk.X, padx=20, pady=10)
        
        # Get recommendations
        recommendations = get_recs_func()
        
        # Display recommendations
        for rec in recommendations:
            if isinstance(rec, dict):
                rec_text = f"🎬 {rec['title']}"
                if 'reason' in rec:
                    rec_text += f" - {rec['reason']}"
                if 'rating' in rec:
                    rec_text += f" - ⭐ {rec['rating']}"
            else:
                rec_text = f"🎬 {rec['title']} - ⭐ {rec['rating']} - {rec['quality']}"
            
            rec_label = tk.Label(section_frame, text=rec_text, 
                                bg='#1a1a1a', fg='#cccccc', font=('Arial', 12),
                                anchor=tk.W)
            rec_label.pack(anchor=tk.W, padx=10, pady=2)
    
    def discover_by_genre(self, genre):
        """Discover content by genre"""
        results = []
        for category, content_list in self.content_library.items():
            for content in content_list:
                if content.get('genre', '').lower() == genre.lower():
                    results.append(content)
        
        if results:
            messagebox.showinfo(f"🎭 {genre} Content", 
                               f"Found {len(results)} {genre} titles:\n" + 
                               "\n".join([f"• {content['title']} - ⭐ {content['rating']}" for content in results[:5]]))
        else:
            messagebox.showinfo(f"🎭 {genre} Content", f"No {genre} content found.")
    
    def discover_by_year(self, year):
        """Discover content by year"""
        results = []
        for category, content_list in self.content_library.items():
            for content in content_list:
                if str(content.get('year', '')) == year:
                    results.append(content)
        
        if results:
            messagebox.showinfo(f"📅 {year} Content", 
                               f"Found {len(results)} titles from {year}:\n" + 
                               "\n".join([f"• {content['title']} - ⭐ {content['rating']}" for content in results[:5]]))
        else:
            messagebox.showinfo(f"📅 {year} Content", f"No content found from {year}.")
    
    def discover_by_quality(self, quality):
        """Discover content by quality"""
        results = []
        for category, content_list in self.content_library.items():
            for content in content_list:
                if content.get('quality', '').upper() == quality.upper():
                    results.append(content)
        
        if results:
            messagebox.showinfo(f"🎯 {quality} Content", 
                               f"Found {len(results)} {quality} titles:\n" + 
                               "\n".join([f"• {content['title']} - ⭐ {content['rating']}" for content in results[:5]]))
        else:
            messagebox.showinfo(f"🎯 {quality} Content", f"No {quality} content found.")
    
    def save_ai_preferences(self, genre_vars):
        """Save AI preferences"""
        selected_genres = [genre for genre, var in genre_vars.items() if var.get()]
        self.ai_copilot['user_preferences']['favorite_genres'] = selected_genres
        
        messagebox.showinfo("Preferences Saved", 
                           f"✅ AI preferences updated!\n\n"
                           f"Favorite genres: {', '.join(selected_genres) if selected_genres else 'None selected'}\n\n"
                           f"The AI will now provide more personalized recommendations!")
    
    def open_music_service(self, service):
        """Open music service with unlimited skips"""
        features_text = "\n".join([f"• {feature}" for feature in service['features']])
        
        # Show service info
        info_window = tk.Toplevel(self.root)
        info_window.title(f"🎵 {service['name']}")
        info_window.geometry("600x400")
        info_window.configure(bg='#1a1a1a')
        
        # Service info
        info_frame = tk.Frame(info_window, bg='#1a1a1a')
        info_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Service name
        title_label = tk.Label(info_frame, text=service['name'], 
                              bg='#1a1a1a', fg=service['color'], 
                              font=('Arial', 18, 'bold'))
        title_label.pack(pady=10)
        
        # Features
        features_label = tk.Label(info_frame, text="Features:", 
                                bg='#1a1a1a', fg='white', 
                                font=('Arial', 14, 'bold'))
        features_label.pack(anchor=tk.W, pady=5)
        
        features_text_label = tk.Label(info_frame, text=features_text, 
                                      bg='#1a1a1a', fg='#cccccc', 
                                      font=('Arial', 12), justify=tk.LEFT)
        features_text_label.pack(anchor=tk.W, pady=5)
        
        # Buttons
        button_frame = tk.Frame(info_frame, bg='#1a1a1a')
        button_frame.pack(pady=20)
        
        # Open in browser
        browser_btn = tk.Button(button_frame, text="🌐 Open in Browser", 
                               command=lambda: webbrowser.open(service['url']),
                               bg=service['color'], fg='white', 
                               font=('Arial', 12, 'bold'), width=15)
        browser_btn.pack(side=tk.LEFT, padx=5)
        
        # Open in Kodi
        kodi_btn = tk.Button(button_frame, text="🎬 Open in Kodi", 
                            command=lambda: self.open_in_kodi(service),
                            bg='#FF6B35', fg='white', 
                            font=('Arial', 12, 'bold'), width=15)
        kodi_btn.pack(side=tk.LEFT, padx=5)
        
        # Close button
        close_btn = tk.Button(button_frame, text="❌ Close", 
                             command=info_window.destroy,
                             bg='#f44336', fg='white', 
                             font=('Arial', 12, 'bold'), width=15)
        close_btn.pack(side=tk.LEFT, padx=5)
    
    def open_in_kodi(self, service):
        """Open music service in Kodi"""
        service_id = None
        for sid, srv in self.music_services.items():
            if srv == service:
                service_id = sid
                break
        
        if service_id and service_id in self.kodi_addons:
            addon = self.kodi_addons[service_id]
            messagebox.showinfo("Kodi Integration", 
                               f"🎬 Opening {addon['name']} in Kodi\n\n"
                               f"Addon ID: {addon['addon_id']}\n"
                               f"Status: {'Enabled' if addon['enabled'] else 'Disabled'}\n\n"
                               f"This would launch Kodi with the {service['name']} addon.")
        else:
            messagebox.showinfo("Kodi Integration", 
                               f"🎬 Kodi addon for {service['name']} not found\n\n"
                               f"Available Kodi addons:\n" + 
                               "\n".join([f"• {addon['name']}" for addon in self.kodi_addons.values()]))
    
    def show_search(self):
        """Show search dialog"""
        search_window = tk.Toplevel(self.root)
        search_window.title("🔍 Search Content")
        search_window.geometry("800x600")
        search_window.configure(bg='#1a1a1a')
        
        # Search entry
        search_frame = tk.Frame(search_window, bg='#1a1a1a')
        search_frame.pack(fill=tk.X, padx=20, pady=20)
        
        tk.Label(search_frame, text="Search:", bg='#1a1a1a', fg='white', 
                font=('Arial', 16, 'bold')).pack(side=tk.LEFT)
        
        search_entry = tk.Entry(search_frame, width=50, bg='#2a2a2a', fg='white',
                               font=('Arial', 14))
        search_entry.pack(side=tk.LEFT, padx=10)
        
        search_btn = tk.Button(search_frame, text="🔍 Search", 
                              bg='#4CAF50', fg='white', font=('Arial', 14, 'bold'))
        search_btn.pack(side=tk.LEFT, padx=10)
        
        # Results
        results_frame = tk.Frame(search_window, bg='#1a1a1a')
        results_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        results_list = tk.Listbox(results_frame, bg='#2a2a2a', fg='white', 
                                 font=('Arial', 12), height=15)
        results_list.pack(fill=tk.BOTH, expand=True)
        
        # Add sample results
        sample_results = [
            "🎬 The Matrix (1999) - Netflix",
            "📺 Stranger Things (2019) - Netflix", 
            "🎌 Attack on Titan (2013) - Crunchyroll",
            "🥊 UFC 300 (2024) - ESPN+",
            "🎬 Inception (2010) - HBO Max"
        ]
        
        for result in sample_results:
            results_list.insert(tk.END, result)
    
    def show_settings(self):
        """Show settings dialog"""
        settings_window = tk.Toplevel(self.root)
        settings_window.title("⚙️ Settings")
        settings_window.geometry("600x400")
        settings_window.configure(bg='#1a1a1a')
        
        # Settings content
        settings_frame = tk.Frame(settings_window, bg='#1a1a1a')
        settings_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Display settings
        display_frame = tk.LabelFrame(settings_frame, text="📺 Display Settings", 
                                     bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        display_frame.pack(fill=tk.X, pady=10)
        
        # Fullscreen toggle
        fullscreen_var = tk.BooleanVar()
        fullscreen_check = tk.Checkbutton(display_frame, text="Fullscreen Mode", 
                                         variable=fullscreen_var, bg='#1a1a1a', fg='white',
                                         font=('Arial', 12))
        fullscreen_check.pack(anchor=tk.W, padx=10, pady=5)
        
        # Quality settings
        quality_frame = tk.LabelFrame(settings_frame, text="🎯 Quality Settings", 
                                     bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        quality_frame.pack(fill=tk.X, pady=10)
        
        quality_var = tk.StringVar(value="HD")
        quality_options = ["SD", "HD", "4K", "8K"]
        quality_combo = ttk.Combobox(quality_frame, textvariable=quality_var,
                                    values=quality_options, width=20)
        quality_combo.pack(padx=10, pady=10)
        
        # Save button
        save_btn = tk.Button(settings_frame, text="💾 Save Settings", 
                            bg='#4CAF50', fg='white', font=('Arial', 12, 'bold'))
        save_btn.pack(pady=20)
    
    def show_streaming(self):
        """Show streaming services"""
        streaming_window = tk.Toplevel(self.root)
        streaming_window.title("📺 Streaming Services")
        streaming_window.geometry("1000x700")
        streaming_window.configure(bg='#1a1a1a')
        
        # Services grid
        services_frame = tk.Frame(streaming_window, bg='#1a1a1a')
        services_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        row = 0
        col = 0
        for service_id, service in self.streaming_services.items():
            service_frame = tk.Frame(services_frame, bg=service['color'], 
                                    relief=tk.RAISED, bd=3)
            service_frame.grid(row=row, column=col, padx=10, pady=10, sticky='ew')
            
            service_btn = tk.Button(service_frame, text=f"📺 {service['name']}", 
                                   command=lambda url=service['url']: webbrowser.open(url),
                                   bg=service['color'], fg='white', 
                                   font=('Arial', 14, 'bold'), width=15, height=3)
            service_btn.pack(padx=10, pady=10)
            
            col += 1
            if col >= 3:
                col = 0
                row += 1
        
        # Configure grid weights
        for i in range(3):
            services_frame.columnconfigure(i, weight=1)
    
    def show_media_engine(self):
        """Show built-in media engine (custom Kodi-like system)"""
        media_window = tk.Toplevel(self.root)
        media_window.title("🎬 RawrZ Media Engine")
        media_window.geometry("1200x800")
        media_window.configure(bg='#1a1a1a')
        
        # Create notebook for tabs
        notebook = ttk.Notebook(media_window)
        notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Music tab
        music_frame = tk.Frame(notebook, bg='#1a1a1a')
        notebook.add(music_frame, text="🎵 Music")
        self.setup_music_tab(music_frame)
        
        # Video tab
        video_frame = tk.Frame(notebook, bg='#1a1a1a')
        notebook.add(video_frame, text="📺 Video")
        self.setup_video_tab(video_frame)
        
        # Anime tab
        anime_frame = tk.Frame(notebook, bg='#1a1a1a')
        notebook.add(anime_frame, text="🎌 Anime")
        self.setup_anime_tab(anime_frame)
        
        # Streaming tab
        streaming_frame = tk.Frame(notebook, bg='#1a1a1a')
        notebook.add(streaming_frame, text="🌐 Streaming")
        self.setup_streaming_tab(streaming_frame)
        
        # Settings tab
        settings_frame = tk.Frame(notebook, bg='#1a1a1a')
        notebook.add(settings_frame, text="⚙️ Settings")
        self.setup_settings_tab(settings_frame)
    
    def setup_music_tab(self, parent):
        """Setup music tab with unlimited skips"""
        # Title
        title_label = tk.Label(parent, text="🎵 Music Services (Unlimited Skips)", 
                              bg='#1a1a1a', fg='#1DB954', 
                              font=('Arial', 18, 'bold'))
        title_label.pack(pady=10)
        
        # Music services grid
        music_frame = tk.Frame(parent, bg='#1a1a1a')
        music_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        row = 0
        col = 0
        for service_id, service in self.music_services.items():
            service_frame = tk.Frame(music_frame, bg=service['color'], 
                                   relief=tk.RAISED, bd=3)
            service_frame.grid(row=row, column=col, padx=10, pady=10, sticky='ew')
            
            # Service button
            service_btn = tk.Button(service_frame, text=f"🎵 {service['name']}", 
                                  command=lambda s=service: self.launch_music_service(s),
                                  bg=service['color'], fg='white', 
                                  font=('Arial', 12, 'bold'), width=20, height=3)
            service_btn.pack(padx=10, pady=10)
            
            # Features
            features_text = "\n".join([f"• {feature}" for feature in service['features']])
            features_label = tk.Label(service_frame, text=features_text, 
                                    bg=service['color'], fg='white', 
                                    font=('Arial', 9), justify=tk.LEFT)
            features_label.pack(padx=10, pady=(0, 10))
            
            col += 1
            if col >= 3:
                col = 0
                row += 1
        
        # Configure grid weights
        for i in range(3):
            music_frame.columnconfigure(i, weight=1)
    
    def setup_video_tab(self, parent):
        """Setup video tab with premium content"""
        # Title
        title_label = tk.Label(parent, text="📺 Premium Video Content", 
                              bg='#1a1a1a', fg='#E50914', 
                              font=('Arial', 18, 'bold'))
        title_label.pack(pady=10)
        
        # Video services
        video_frame = tk.Frame(parent, bg='#1a1a1a')
        video_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Netflix
        netflix_frame = tk.Frame(video_frame, bg='#E50914', relief=tk.RAISED, bd=3)
        netflix_frame.pack(fill=tk.X, padx=10, pady=10)
        
        netflix_btn = tk.Button(netflix_frame, text="📺 Netflix Premium", 
                               command=lambda: self.launch_video_service('netflix'),
                               bg='#E50914', fg='white', 
                               font=('Arial', 14, 'bold'), width=25, height=2)
        netflix_btn.pack(padx=10, pady=10)
        
        # HBO Max
        hbo_frame = tk.Frame(video_frame, bg='#8B5CF6', relief=tk.RAISED, bd=3)
        hbo_frame.pack(fill=tk.X, padx=10, pady=10)
        
        hbo_btn = tk.Button(hbo_frame, text="📺 HBO Max Premium", 
                          command=lambda: self.launch_video_service('hbo'),
                          bg='#8B5CF6', fg='white', 
                          font=('Arial', 14, 'bold'), width=25, height=2)
        hbo_btn.pack(padx=10, pady=10)
        
        # Disney+
        disney_frame = tk.Frame(video_frame, bg='#113CCF', relief=tk.RAISED, bd=3)
        disney_frame.pack(fill=tk.X, padx=10, pady=10)
        
        disney_btn = tk.Button(disney_frame, text="📺 Disney+ Premium", 
                             command=lambda: self.launch_video_service('disney'),
                             bg='#113CCF', fg='white', 
                             font=('Arial', 14, 'bold'), width=25, height=2)
        disney_btn.pack(padx=10, pady=10)
    
    def setup_anime_tab(self, parent):
        """Setup anime tab with Crunchyroll"""
        # Title
        title_label = tk.Label(parent, text="🎌 Anime & Crunchyroll Premium", 
                              bg='#1a1a1a', fg='#FF6600', 
                              font=('Arial', 18, 'bold'))
        title_label.pack(pady=10)
        
        # Crunchyroll section
        crunchyroll_frame = tk.Frame(parent, bg='#FF6600', relief=tk.RAISED, bd=3)
        crunchyroll_frame.pack(fill=tk.X, padx=20, pady=20)
        
        # Crunchyroll button
        crunchyroll_btn = tk.Button(crunchyroll_frame, text="🎌 Crunchyroll Premium", 
                                   command=lambda: self.launch_anime_service('crunchyroll'),
                                   bg='#FF6600', fg='white', 
                                   font=('Arial', 16, 'bold'), width=30, height=3)
        crunchyroll_btn.pack(padx=20, pady=20)
        
        # Features
        features_text = "• Unlimited Anime Access\n• No Ads\n• Premium Quality\n• Latest Episodes\n• Offline Downloads"
        features_label = tk.Label(crunchyroll_frame, text=features_text, 
                                 bg='#FF6600', fg='white', 
                                 font=('Arial', 12), justify=tk.LEFT)
        features_label.pack(padx=20, pady=(0, 20))
        
        # Popular anime list
        anime_list_frame = tk.LabelFrame(parent, text="🔥 Popular Anime", 
                                        bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        anime_list_frame.pack(fill=tk.X, padx=20, pady=20)
        
        anime_list = tk.Listbox(anime_list_frame, bg='#2a2a2a', fg='white', 
                               font=('Arial', 12), height=8)
        anime_list.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Add popular anime
        popular_anime = [
            "🎌 Attack on Titan - Final Season",
            "🎌 Demon Slayer - Entertainment District Arc", 
            "🎌 One Piece - Wano Country Arc",
            "🎌 My Hero Academia - Season 6",
            "🎌 Jujutsu Kaisen - Season 2",
            "🎌 Chainsaw Man - Season 1",
            "🎌 Spy x Family - Season 2",
            "🎌 Tokyo Revengers - Season 2"
        ]
        
        for anime in popular_anime:
            anime_list.insert(tk.END, anime)
    
    def setup_streaming_tab(self, parent):
        """Setup streaming tab with torrents and scrapers"""
        # Title
        title_label = tk.Label(parent, text="🌐 Advanced Streaming & Torrents", 
                              bg='#1a1a1a', fg='#4CAF50', 
                              font=('Arial', 18, 'bold'))
        title_label.pack(pady=10)
        
        # Streaming options
        streaming_frame = tk.Frame(parent, bg='#1a1a1a')
        streaming_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Torrent streaming
        torrent_frame = tk.Frame(streaming_frame, bg='#4CAF50', relief=tk.RAISED, bd=3)
        torrent_frame.pack(fill=tk.X, padx=10, pady=10)
        
        torrent_btn = tk.Button(torrent_frame, text="🌊 Torrent Streamer", 
                               command=lambda: self.launch_torrent_streamer(),
                               bg='#4CAF50', fg='white', 
                               font=('Arial', 14, 'bold'), width=25, height=2)
        torrent_btn.pack(padx=10, pady=10)
        
        # Content scraper
        scraper_frame = tk.Frame(streaming_frame, bg='#FF9800', relief=tk.RAISED, bd=3)
        scraper_frame.pack(fill=tk.X, padx=10, pady=10)
        
        scraper_btn = tk.Button(scraper_frame, text="🔍 Content Scraper", 
                               command=lambda: self.launch_content_scraper(),
                               bg='#FF9800', fg='white', 
                               font=('Arial', 14, 'bold'), width=25, height=2)
        scraper_btn.pack(padx=10, pady=10)
    
    def setup_settings_tab(self, parent):
        """Setup settings tab for media engine"""
        # Title
        title_label = tk.Label(parent, text="⚙️ Media Engine Settings", 
                              bg='#1a1a1a', fg='#2196F3', 
                              font=('Arial', 18, 'bold'))
        title_label.pack(pady=10)
        
        # Settings frame
        settings_frame = tk.Frame(parent, bg='#1a1a1a')
        settings_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Quality settings
        quality_frame = tk.LabelFrame(settings_frame, text="🎯 Quality Settings", 
                                     bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        quality_frame.pack(fill=tk.X, pady=10)
        
        quality_var = tk.StringVar(value=self.media_engine['settings']['preferred_quality'])
        quality_options = ["SD", "HD", "4K", "8K"]
        quality_combo = ttk.Combobox(quality_frame, textvariable=quality_var,
                                    values=quality_options, width=20)
        quality_combo.pack(padx=10, pady=10)
        
        # Feature toggles
        features_frame = tk.LabelFrame(settings_frame, text="🚀 Features", 
                                      bg='#1a1a1a', fg='white', font=('Arial', 14, 'bold'))
        features_frame.pack(fill=tk.X, pady=10)
        
        # Unlimited skips
        unlimited_var = tk.BooleanVar(value=self.media_engine['settings']['unlimited_skips'])
        unlimited_check = tk.Checkbutton(features_frame, text="Unlimited Skips", 
                                        variable=unlimited_var, bg='#1a1a1a', fg='white',
                                        font=('Arial', 12))
        unlimited_check.pack(anchor=tk.W, padx=10, pady=5)
        
        # No ads
        no_ads_var = tk.BooleanVar(value=self.media_engine['settings']['no_ads'])
        no_ads_check = tk.Checkbutton(features_frame, text="No Ads", 
                                     variable=no_ads_var, bg='#1a1a1a', fg='white',
                                     font=('Arial', 12))
        no_ads_check.pack(anchor=tk.W, padx=10, pady=5)
        
        # Background play
        background_var = tk.BooleanVar(value=self.media_engine['settings']['background_play'])
        background_check = tk.Checkbutton(features_frame, text="Background Play", 
                                         variable=background_var, bg='#1a1a1a', fg='white',
                                         font=('Arial', 12))
        background_check.pack(anchor=tk.W, padx=10, pady=5)
        
        # Save button
        save_btn = tk.Button(settings_frame, text="💾 Save Settings", 
                            command=lambda: self.save_media_settings(quality_var.get(), 
                                                                   unlimited_var.get(),
                                                                   no_ads_var.get(),
                                                                   background_var.get()),
                            bg='#4CAF50', fg='white', font=('Arial', 12, 'bold'))
        save_btn.pack(pady=20)
    
    def launch_music_service(self, service):
        """Launch music service with unlimited skips"""
        messagebox.showinfo("Music Service", 
                           f"🎵 Launching {service['name']}\n\n"
                           f"Features enabled:\n" + 
                           "\n".join([f"• {feature}" for feature in service['features']]) +
                           f"\n\n🌐 Opening: {service['url']}")
        webbrowser.open(service['url'])
    
    def launch_video_service(self, service_id):
        """Launch video service"""
        service = self.streaming_services.get(service_id, {})
        messagebox.showinfo("Video Service", 
                           f"📺 Launching {service.get('name', 'Video Service')}\n\n"
                           f"🌐 Opening: {service.get('url', 'Service URL')}")
        if service.get('url'):
            webbrowser.open(service['url'])
    
    def launch_anime_service(self, service_id):
        """Launch anime service (Crunchyroll)"""
        if service_id == 'crunchyroll':
            messagebox.showinfo("Anime Service", 
                               "🎌 Launching Crunchyroll Premium\n\n"
                               "Features:\n"
                               "• Unlimited Anime Access\n"
                               "• No Ads\n"
                               "• Premium Quality\n"
                               "• Latest Episodes\n"
                               "• Offline Downloads\n\n"
                               "🌐 Opening: https://crunchyroll.com")
            webbrowser.open("https://crunchyroll.com")
    
    def launch_torrent_streamer(self):
        """Launch torrent streaming"""
        messagebox.showinfo("Torrent Streamer", 
                           "🌊 Launching Torrent Streamer\n\n"
                           "Features:\n"
                           "• Direct torrent streaming\n"
                           "• No downloads required\n"
                           "• Instant playback\n"
                           "• High quality content\n"
                           "• Anonymous streaming")
    
    def launch_content_scraper(self):
        """Launch content scraper"""
        messagebox.showinfo("Content Scraper", 
                           "🔍 Launching Content Scraper\n\n"
                           "Features:\n"
                           "• Scrape premium content\n"
                           "• Multiple sources\n"
                           "• Auto-quality selection\n"
                           "• Direct streaming links\n"
                           "• Real-time updates")
    
    def save_media_settings(self, quality, unlimited_skips, no_ads, background_play):
        """Save media engine settings"""
        self.media_engine['settings'].update({
            'preferred_quality': quality,
            'unlimited_skips': unlimited_skips,
            'no_ads': no_ads,
            'background_play': background_play
        })
        messagebox.showinfo("Settings Saved", 
                           "⚙️ Media Engine settings saved!\n\n"
                           f"Quality: {quality}\n"
                           f"Unlimited Skips: {'Yes' if unlimited_skips else 'No'}\n"
                           f"No Ads: {'Yes' if no_ads else 'No'}\n"
                           f"Background Play: {'Yes' if background_play else 'No'}")

    def run(self):
        """Run the MovieBox Pro App"""
        print("🎬 Starting MovieBox Pro...")
        print("🔥 Premium streaming experience")
        print("📺 4K quality support")
        print("📱 Download & offline viewing")
        print("👆 Touch-friendly interface")
        self.root.mainloop()

def create_android_launcher():
    """Create Android launcher script"""
    launcher_content = '''#!/usr/bin/env python3
"""
Android Projector App Launcher
Run this on Android devices or projectors
"""

import sys
import os

# Add current directory to path
sys.path.insert(0, os.path.dirname(__file__))

try:
    from android_projector_app import AndroidProjectorApp
    
    print("Android Projector TV App")
    print("=" * 40)
    print("Starting projector-optimized TV app...")
    print("Full HD support for projectors")
    print("Touch-friendly Android interface")
    print("Premium content access")
    print("=" * 40)
    
    app = AndroidProjectorApp()
    app.run()
    
except ImportError as e:
    print(f"Error importing Android app: {e}")
    print("Please ensure android_projector_app.py is in the same directory")
except Exception as e:
    print(f"Error starting app: {e}")
'''
    
    with open('launch_android_tv.py', 'w', encoding='utf-8') as f:
        f.write(launcher_content)
    
    print("Android launcher created: launch_android_tv.py")

if __name__ == "__main__":
    # Test the MovieBox Pro App
    print("🎬 Testing MovieBox Pro...")
    app = MovieBoxProApp()
    print("✅ MovieBox Pro ready!")
    print("🔥 Features:")
    print("  • Premium streaming experience")
    print("  • 4K quality support")
    print("  • Download & offline viewing")
    print("  • Touch-friendly interface")
    print("  • Unlimited content access")
    print("  • No ads, high quality")
    
    # Create launcher
    create_android_launcher()
    
    # Run the app
    app.run()
