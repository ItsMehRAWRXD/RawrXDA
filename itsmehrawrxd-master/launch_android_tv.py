#!/usr/bin/env python3
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
