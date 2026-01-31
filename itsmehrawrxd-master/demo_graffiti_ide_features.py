#!/usr/bin/env python3
"""
Demo Script for Graffiti IDE Features
Shows off all the graffiti, themes, and compilation gears
"""

import tkinter as tk
from tkinter import ttk, messagebox
import time
import threading

def demo_graffiti_ide():
    """Demo all Graffiti IDE features"""
    print("🔥 GRAFFITI IDE FEATURES DEMO 🔥")
    print("=" * 50)
    
    # Create demo window
    demo_window = tk.Tk()
    demo_window.title("🔥 GRAFFITI IDE FEATURES DEMO 🔥")
    demo_window.geometry("1000x700")
    demo_window.configure(bg='black')
    
    # Title
    title_label = tk.Label(
        demo_window,
        text="🔥 GRAFFITI IDE - ULTIMATE HACKER'S WORKSPACE 🔥",
        bg='black',
        fg='#ff6600',
        font=('Consolas', 16, 'bold')
    )
    title_label.pack(pady=20)
    
    # Features list
    features_frame = tk.Frame(demo_window, bg='black')
    features_frame.pack(fill='both', expand=True, padx=20, pady=10)
    
    # Feature buttons
    buttons_frame = tk.Frame(features_frame, bg='black')
    buttons_frame.pack(fill='x', pady=10)
    
    # Graffiti Wall Demo
    tk.Button(
        buttons_frame,
        text="🎨 DEMO GRAFFITI WALL",
        bg='#00ff00',
        fg='black',
        font=('Consolas', 12, 'bold'),
        command=lambda: demo_graffiti_wall(demo_window)
    ).pack(side='left', padx=10, pady=5)
    
    # Compilation Gears Demo
    tk.Button(
        buttons_frame,
        text="⚙️ DEMO COMPILATION GEARS",
        bg='#ff6600',
        fg='white',
        font=('Consolas', 12, 'bold'),
        command=lambda: demo_compilation_gears(demo_window)
    ).pack(side='left', padx=10, pady=5)
    
    # Forum Themes Demo
    tk.Button(
        buttons_frame,
        text="🎭 DEMO FORUM THEMES",
        bg='#ff0066',
        fg='white',
        font=('Consolas', 12, 'bold'),
        command=lambda: demo_forum_themes(demo_window)
    ).pack(side='left', padx=10, pady=5)
    
    # AI Compiler Demo
    tk.Button(
        buttons_frame,
        text="🤖 DEMO AI COMPILER",
        bg='#0066ff',
        fg='white',
        font=('Consolas', 12, 'bold'),
        command=lambda: demo_ai_compiler(demo_window)
    ).pack(side='left', padx=10, pady=5)
    
    # Feature descriptions
    desc_frame = tk.Frame(features_frame, bg='#1a1a1a', relief='raised', bd=2)
    desc_frame.pack(fill='both', expand=True, pady=10)
    
    desc_text = tk.Text(
        desc_frame,
        bg='#1a1a1a',
        fg='#00ff00',
        font=('Consolas', 10),
        wrap='word'
    )
    desc_text.pack(fill='both', expand=True, padx=10, pady=10)
    
    # Feature descriptions
    descriptions = """🔥 GRAFFITI IDE FEATURES 🔥

🎨 GRAFFITI WALL:
• Spray-painted code wall background
• Real code snippets from legendary hack forums
• Assembly, C++, Python, JavaScript graffiti
• Animated spray paint effects
• Brick wall texture with code overlays

⚙️ COMPILATION GEARS:
• Spinning mechanical gears during compilation
• Code spitters that shoot out assembly code
• Real-time compilation animation
• Multiple gear systems with different speeds
• Code particles with physics simulation

🎭 FORUM THEMES:
• UnknownCheats.me dark theme
• HackForums.net orange theme
• Game-Deception matrix green
• vBulletin classic blue
• WordPress classic clean
• mIRC retro blue/yellow
• CS cheat servers green/red
• OGC Elite orange/black
• iC JAPS blue/gold
• Death Elf red/black

🤖 AI COMPILER MANAGER:
• Ollama integration for local AI models
• Tabby for real-time code completion
• LocalAI for OpenAI-compatible API
• Continue for context-aware chat
• Code analysis with CodeT5-style prompts
• Online compiler integration
• Judge0 and Piston execution engines

💀 ELITE FEATURES:
• Transparent floating panels
• Real-time code graffiti spraying
• Memory viewer with hex dumps
• Process hacker interface
• Network traffic monitor
• AI copilot with suggestions
• Hack console with elite styling

🚀 READY TO HACK THE MATRIX! 🚀"""
    
    desc_text.insert('1.0', descriptions)
    desc_text.config(state='disabled')
    
    # Start demo
    demo_window.mainloop()

def demo_graffiti_wall(parent):
    """Demo the graffiti wall"""
    try:
        from graffiti_ide_wall import GraffitiIDE
        graffiti_ide = GraffitiIDE()
        print("🎨 Graffiti Wall demo started!")
    except ImportError:
        messagebox.showwarning("Missing Module", "graffiti_ide_wall.py not found!")
        print("❌ Graffiti Wall module not found")

def demo_compilation_gears(parent):
    """Demo the compilation gears"""
    try:
        from compilation_gears_animation import CompilationEngine
        gears_engine = CompilationEngine(parent)
        print("⚙️ Compilation Gears demo started!")
    except ImportError:
        messagebox.showwarning("Missing Module", "compilation_gears_animation.py not found!")
        print("❌ Compilation Gears module not found")

def demo_forum_themes(parent):
    """Demo forum themes"""
    theme_window = tk.Toplevel(parent)
    theme_window.title("🎭 Forum Themes Demo")
    theme_window.geometry("600x500")
    theme_window.configure(bg='black')
    
    # Theme selector
    theme_label = tk.Label(
        theme_window,
        text="🎭 SELECT FORUM THEME 🎭",
        bg='black',
        fg='#ff6600',
        font=('Consolas', 14, 'bold')
    )
    theme_label.pack(pady=20)
    
    # Theme buttons
    themes = [
        ("UnknownCheats.me", "#0d1117", "#c9d1d9", "#58a6ff"),
        ("HackForums.net", "#1a1a1a", "#e0e0e0", "#ff6b35"),
        ("Game-Deception", "#0f0f0f", "#00ff00", "#00ff00"),
        ("vBulletin Classic", "#ffffff", "#000000", "#0066cc"),
        ("WordPress Classic", "#f9f9f9", "#333333", "#0073aa"),
        ("mIRC Classic", "#000080", "#ffffff", "#ffff00"),
        ("CS Cheat Servers", "#1e1e1e", "#00ff41", "#ff0040"),
        ("OGC Elite", "#0a0a0a", "#ff6600", "#ff6600"),
        ("iC JAPS", "#001122", "#00aaff", "#ffaa00"),
        ("Death Elf", "#2d1b1b", "#ff4444", "#ff6666")
    ]
    
    buttons_frame = tk.Frame(theme_window, bg='black')
    buttons_frame.pack(fill='both', expand=True, padx=20, pady=10)
    
    for i, (name, bg_color, text_color, accent_color) in enumerate(themes):
        btn = tk.Button(
            buttons_frame,
            text=f"🎨 {name}",
            bg=accent_color,
            fg='white' if bg_color.startswith('#0') or bg_color.startswith('#1') else 'black',
            font=('Consolas', 10, 'bold'),
            command=lambda n=name, bg=bg_color, txt=text_color, acc=accent_color: apply_theme(n, bg, txt, acc, theme_window)
        )
        btn.pack(fill='x', pady=2)
    
    print("🎭 Forum Themes demo started!")

def apply_theme(name, bg_color, text_color, accent_color, window):
    """Apply selected theme"""
    window.configure(bg=bg_color)
    messagebox.showinfo(
        "🎨 Theme Applied",
        f"Applied {name} theme!\n"
        f"Background: {bg_color}\n"
        f"Text: {text_color}\n"
        f"Accent: {accent_color}"
    )

def demo_ai_compiler(parent):
    """Demo AI compiler manager"""
    try:
        from local_ai_compiler_manager import LocalAICompilerManager
        
        # Create mock IDE instance
        class MockIDE:
            def __init__(self):
                self.root = parent
                self.notebook = None
        
        mock_ide = MockIDE()
        ai_manager = LocalAICompilerManager(mock_ide)
        print("🤖 AI Compiler Manager demo started!")
    except ImportError:
        messagebox.showwarning("Missing Module", "local_ai_compiler_manager.py not found!")
        print("❌ AI Compiler Manager module not found")

def main():
    """Main demo function"""
    print("🔥 Starting Graffiti IDE Features Demo...")
    print("💀 Loading all elite features...")
    print("🎨 Preparing graffiti wall...")
    print("⚙️ Spinning up compilation gears...")
    print("🎭 Loading forum themes...")
    print("🤖 Initializing AI compiler...")
    print("🚀 Ready to demo!")
    
    demo_graffiti_ide()

if __name__ == "__main__":
    main()
