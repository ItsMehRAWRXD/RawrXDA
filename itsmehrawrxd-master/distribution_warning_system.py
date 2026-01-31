#!/usr/bin/env python3
"""
Distribution Warning System - DO NOT DISTRIBUTE Notifications
Ensures proper licensing and distribution control throughout the IDE
"""

import tkinter as tk
from tkinter import messagebox, ttk
import time
import threading
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional
import hashlib
import random

class DistributionWarningSystem:
    """
    Comprehensive distribution warning system
    Adds DO NOT DISTRIBUTE notifications throughout the IDE
    """
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.warning_shown = False
        self.warning_acknowledged = False
        self.license_key = None
        self.user_agreement = False
        
        # Warning messages
        self.warning_messages = {
            'startup': [
                "⚠️ DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE",
                "This software is proprietary and confidential.",
                "Distribution, copying, or sharing is strictly prohibited.",
                "Unauthorized use may result in legal action."
            ],
            'export': [
                "🚫 EXPORT RESTRICTED",
                "This software contains export-controlled technology.",
                "International distribution is prohibited without authorization."
            ],
            'build': [
                "🔒 BUILD RESTRICTED",
                "Generated executables contain proprietary code.",
                "Distribution of compiled binaries is prohibited."
            ],
            'network': [
                "🌐 NETWORK ACCESS RESTRICTED",
                "Network features are for authorized users only.",
                "Unauthorized network access is prohibited."
            ]
        }
        
        print("🚫 Distribution Warning System initialized")
    
    def show_startup_warning(self) -> bool:
        """Show startup distribution warning"""
        
        if self.warning_shown:
            return self.warning_acknowledged
        
        # Create warning dialog
        warning_dialog = tk.Toplevel(self.ide.root)
        warning_dialog.title("🚫 DISTRIBUTION WARNING")
        warning_dialog.geometry("700x500")
        warning_dialog.transient(self.ide.root)
        warning_dialog.grab_set()
        
        # Make it modal
        warning_dialog.protocol("WM_DELETE_WINDOW", lambda: None)
        
        # Center dialog
        warning_dialog.geometry("+%d+%d" % (
            self.ide.root.winfo_rootx() + 100,
            self.ide.root.winfo_rooty() + 100
        ))
        
        # Main frame
        main_frame = ttk.Frame(warning_dialog, padding="20")
        main_frame.pack(fill='both', expand=True)
        
        # Warning icon and title
        title_frame = ttk.Frame(main_frame)
        title_frame.pack(fill='x', pady=(0, 20))
        
        warning_label = ttk.Label(title_frame, 
                                 text="🚫 DO NOT DISTRIBUTE", 
                                 font=("Arial", 18, "bold"),
                                 foreground="red")
        warning_label.pack()
        
        subtitle_label = ttk.Label(title_frame, 
                                  text="PROPRIETARY SOFTWARE WARNING",
                                  font=("Arial", 12, "bold"),
                                  foreground="darkred")
        subtitle_label.pack(pady=(5, 0))
        
        # Warning text
        warning_text = tk.Text(main_frame, height=12, width=70, wrap=tk.WORD,
                              font=("Arial", 10), bg="#fff3cd", fg="#856404")
        warning_text.pack(fill='both', expand=True, pady=(0, 20))
        
        # Insert warning content
        warning_content = """
⚠️  DISTRIBUTION RESTRICTION NOTICE

This software is PROPRIETARY and CONFIDENTIAL. 

STRICTLY PROHIBITED:
• Distribution to third parties
• Copying or sharing of source code
• Reverse engineering or decompilation
• Commercial use without authorization
• Uploading to public repositories
• Sharing via file transfer services

LEGAL CONSEQUENCES:
• Violation may result in legal action
• Civil and criminal penalties may apply
• Damages and injunctive relief available
• Attorney fees may be recoverable

AUTHORIZED USE ONLY:
• Personal development projects
• Educational purposes (with permission)
• Internal company use (with license)
• Testing and evaluation (temporary)

By continuing, you acknowledge that you understand and agree to these restrictions.

This warning appears on every startup to ensure compliance.
        """
        
        warning_text.insert(1.0, warning_content)
        warning_text.config(state='disabled')
        
        # Agreement checkbox
        agreement_var = tk.BooleanVar()
        agreement_check = ttk.Checkbutton(main_frame, 
                                         text="I understand and agree to the distribution restrictions",
                                         variable=agreement_var,
                                         font=("Arial", 10, "bold"))
        agreement_check.pack(pady=(0, 20))
        
        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack()
        
        result = {'acknowledged': False}
        
        def acknowledge():
            if agreement_var.get():
                result['acknowledged'] = True
                self.warning_acknowledged = True
                self.warning_shown = True
                warning_dialog.destroy()
            else:
                messagebox.showerror("Agreement Required", 
                                   "You must agree to the distribution restrictions to continue.")
        
        def exit_app():
            messagebox.showwarning("Exit Required", 
                                 "You cannot use this software without agreeing to the restrictions.")
            sys.exit(1)
        
        ttk.Button(button_frame, text="✅ I AGREE - CONTINUE", 
                  command=acknowledge, style="Accent.TButton").pack(side=tk.LEFT, padx=10)
        ttk.Button(button_frame, text="❌ EXIT APPLICATION", 
                  command=exit_app).pack(side=tk.LEFT, padx=10)
        
        # Wait for user response
        warning_dialog.wait_window()
        
        return result['acknowledged']
    
    def show_export_warning(self) -> bool:
        """Show export distribution warning"""
        
        return self._show_warning_dialog(
            "🚫 EXPORT RESTRICTION",
            "Export/Distribution Warning",
            self.warning_messages['export'] + [
                "",
                "This software contains proprietary algorithms and implementations.",
                "International distribution requires proper licensing and authorization.",
                "Violation of export laws may result in severe penalties."
            ]
        )
    
    def show_build_warning(self) -> bool:
        """Show build distribution warning"""
        
        return self._show_warning_dialog(
            "🔒 BUILD RESTRICTION",
            "Build/Distribution Warning", 
            self.warning_messages['build'] + [
                "",
                "Generated executables contain proprietary code and algorithms.",
                "Distribution of compiled binaries is strictly prohibited.",
                "Each build is watermarked and traceable."
            ]
        )
    
    def show_network_warning(self) -> bool:
        """Show network access warning"""
        
        return self._show_warning_dialog(
            "🌐 NETWORK RESTRICTION",
            "Network Access Warning",
            self.warning_messages['network'] + [
                "",
                "Network features are monitored and logged.",
                "Unauthorized access attempts are tracked.",
                "Suspicious activity may result in access termination."
            ]
        )
    
    def _show_warning_dialog(self, title: str, dialog_title: str, messages: List[str]) -> bool:
        """Show a warning dialog"""
        
        dialog = tk.Toplevel(self.ide.root)
        dialog.title(dialog_title)
        dialog.geometry("600x400")
        dialog.transient(self.ide.root)
        dialog.grab_set()
        
        # Main frame
        main_frame = ttk.Frame(dialog, padding="20")
        main_frame.pack(fill='both', expand=True)
        
        # Title
        title_label = ttk.Label(main_frame, text=title, 
                               font=("Arial", 16, "bold"), foreground="red")
        title_label.pack(pady=(0, 20))
        
        # Messages
        message_text = tk.Text(main_frame, height=15, width=60, wrap=tk.WORD,
                              font=("Arial", 10), bg="#f8d7da", fg="#721c24")
        message_text.pack(fill='both', expand=True, pady=(0, 20))
        
        for message in messages:
            message_text.insert(tk.END, message + "\n")
        
        message_text.config(state='disabled')
        
        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack()
        
        result = {'confirmed': False}
        
        def confirm():
            result['confirmed'] = True
            dialog.destroy()
        
        def cancel():
            result['confirmed'] = False
            dialog.destroy()
        
        ttk.Button(button_frame, text="✅ CONTINUE", command=confirm).pack(side=tk.LEFT, padx=10)
        ttk.Button(button_frame, text="❌ CANCEL", command=cancel).pack(side=tk.LEFT, padx=10)
        
        # Wait for response
        dialog.wait_window()
        
        return result['confirmed']
    
    def add_file_headers(self, file_path: str, content: str) -> str:
        """Add distribution warning headers to file content"""
        
        warning_header = f"""/*
 * ===================================================================
 * 🚫 DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE
 * ===================================================================
 * 
 * This software is proprietary and confidential.
 * Distribution, copying, or sharing is strictly prohibited.
 * Unauthorized use may result in legal action.
 * 
 * Copyright (c) {time.strftime('%Y')} - All Rights Reserved
 * Generated: {time.ctime()}
 * 
 * ===================================================================
 */

"""
        
        # Add to beginning of content
        return warning_header + content
    
    def add_watermark(self, content: str) -> str:
        """Add invisible watermark to content"""
        
        # Generate watermark based on content hash
        content_hash = hashlib.sha256(content.encode()).hexdigest()[:8]
        watermark = f"/* WATERMARK: {content_hash} */"
        
        # Insert watermark at random position
        lines = content.split('\n')
        if len(lines) > 10:
            insert_pos = random.randint(5, len(lines) - 5)
            lines.insert(insert_pos, watermark)
            return '\n'.join(lines)
        
        return content + "\n" + watermark
    
    def add_console_warnings(self):
        """Add console warnings that appear during execution"""
        
        warnings = [
            "🚫 DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE",
            "⚠️  Unauthorized distribution is prohibited",
            "🔒 This software is confidential and restricted",
            "📋 Distribution violations may result in legal action"
        ]
        
        for warning in warnings:
            print(warning)
            time.sleep(0.5)  # Slow down to ensure visibility
    
    def add_gui_warnings(self):
        """Add GUI warnings to IDE interface"""
        
        if hasattr(self.ide, 'status_bar'):
            self.ide.status_bar.config(text="🚫 DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE")
        
        if hasattr(self.ide, 'title'):
            original_title = self.ide.title()
            self.ide.title(f"🚫 {original_title} - DO NOT DISTRIBUTE")
    
    def log_distribution_attempt(self, action: str, details: str = ""):
        """Log potential distribution attempts"""
        
        log_entry = {
            'timestamp': time.time(),
            'action': action,
            'details': details,
            'user_agent': self._get_user_identifier()
        }
        
        # Log to file
        log_file = Path("distribution_log.txt")
        with open(log_file, "a") as f:
            f.write(f"{time.ctime()} - {action} - {details}\n")
        
        print(f"🚫 Distribution attempt logged: {action}")
    
    def _get_user_identifier(self) -> str:
        """Get user identifier for logging"""
        
        try:
            import getpass
            return getpass.getuser()
        except:
            return "unknown_user"

# Integration functions
def integrate_distribution_warnings(ide_instance):
    """Integrate distribution warning system with IDE"""
    
    # Create warning system
    warning_system = DistributionWarningSystem(ide_instance)
    
    # Show startup warning
    if not warning_system.show_startup_warning():
        print("🚫 User did not agree to distribution restrictions. Exiting.")
        sys.exit(1)
    
    # Add to IDE instance
    ide_instance.distribution_warning_system = warning_system
    
    # Add GUI warnings
    warning_system.add_gui_warnings()
    
    # Add console warnings
    warning_system.add_console_warnings()
    
    print("🚫 Distribution warning system integrated")

def add_file_distribution_warning(file_path: str):
    """Add distribution warning to a file"""
    
    warning_system = DistributionWarningSystem(None)
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Add warning header
        warned_content = warning_system.add_file_headers(file_path, content)
        
        # Add watermark
        watermarked_content = warning_system.add_watermark(warned_content)
        
        # Write back
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(watermarked_content)
        
        print(f"🚫 Distribution warning added to {file_path}")
        
    except Exception as e:
        print(f"⚠️ Could not add warning to {file_path}: {e}")

if __name__ == "__main__":
    print("🚫 Distribution Warning System")
    print("=" * 50)
    print("This system ensures proper licensing and distribution control.")
    print("Adds DO NOT DISTRIBUTE notifications throughout the IDE.")
    print("✅ Ready for integration!")
