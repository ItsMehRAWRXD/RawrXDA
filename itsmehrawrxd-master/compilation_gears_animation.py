#!/usr/bin/env python3
"""
Compilation Gears Animation System
Animated gears that spit out code during compilation
"""

import tkinter as tk
from tkinter import ttk, messagebox
import math
import time
import threading
import random
from typing import List, Dict, Tuple

class CompilationGear:
    """Individual spinning gear for compilation animation"""
    
    def __init__(self, canvas, x, y, radius, teeth_count, color, rotation_speed=1.0):
        self.canvas = canvas
        self.x = x
        self.y = y
        self.radius = radius
        self.teeth_count = teeth_count
        self.color = color
        self.rotation_speed = rotation_speed
        self.angle = 0
        self.gear_id = None
        self.teeth_ids = []
        self.is_spinning = False
        
        self.draw_gear()
    
    def draw_gear(self):
        """Draw the gear with teeth"""
        # Main gear circle
        self.gear_id = self.canvas.create_oval(
            self.x - self.radius, self.y - self.radius,
            self.x + self.radius, self.y + self.radius,
            fill=self.color, outline="#333333", width=2,
            tags="gear"
        )
        
        # Draw teeth
        self.draw_teeth()
        
        # Center hole
        self.canvas.create_oval(
            self.x - self.radius * 0.3, self.y - self.radius * 0.3,
            self.x + self.radius * 0.3, self.y + self.radius * 0.3,
            fill="#1a1a1a", outline="#333333", width=1,
            tags="gear"
        )
    
    def draw_teeth(self):
        """Draw gear teeth"""
        self.teeth_ids = []
        for i in range(self.teeth_count):
            angle = (2 * math.pi * i) / self.teeth_count + self.angle
            tooth_length = self.radius * 0.2
            
            # Tooth outer point
            tooth_x = self.x + (self.radius + tooth_length) * math.cos(angle)
            tooth_y = self.y + (self.radius + tooth_length) * math.sin(angle)
            
            # Tooth inner points
            inner_angle1 = angle - math.pi / self.teeth_count
            inner_angle2 = angle + math.pi / self.teeth_count
            
            inner_x1 = self.x + (self.radius - tooth_length * 0.5) * math.cos(inner_angle1)
            inner_y1 = self.y + (self.radius - tooth_length * 0.5) * math.sin(inner_angle1)
            
            inner_x2 = self.x + (self.radius - tooth_length * 0.5) * math.cos(inner_angle2)
            inner_y2 = self.y + (self.radius - tooth_length * 0.5) * math.sin(inner_angle2)
            
            # Draw tooth
            tooth_id = self.canvas.create_polygon(
                tooth_x, tooth_y, inner_x1, inner_y1, inner_x2, inner_y2,
                fill=self.color, outline="#333333", width=1,
                tags="gear"
            )
            self.teeth_ids.append(tooth_id)
    
    def update_rotation(self):
        """Update gear rotation"""
        if self.is_spinning:
            self.angle += self.rotation_speed * 0.1
            if self.angle >= 2 * math.pi:
                self.angle = 0
            
            # Redraw teeth with new rotation
            for tooth_id in self.teeth_ids:
                self.canvas.delete(tooth_id)
            self.draw_teeth()
    
    def start_spinning(self):
        """Start gear spinning"""
        self.is_spinning = True
    
    def stop_spinning(self):
        """Stop gear spinning"""
        self.is_spinning = False

class CodeSpitter:
    """Code spitter that shoots out code fragments"""
    
    def __init__(self, canvas, gear_x, gear_y):
        self.canvas = canvas
        self.gear_x = gear_x
        self.gear_y = gear_y
        self.active_code_particles = []
        self.code_snippets = [
            "mov eax, 1",
            "push ebp",
            "call 0x401000",
            "xor eax, eax",
            "int 0x80",
            "ret",
            "nop",
            "jmp $+2",
            "add esp, 4",
            "pop ebp",
            "cmp eax, 0",
            "je skip",
            "lea edx, [esp+8]",
            "syscall",
            "hlt"
        ]
    
    def spit_code(self, count=5):
        """Spit out code fragments"""
        for _ in range(count):
            code = random.choice(self.code_snippets)
            particle = CodeParticle(
                self.canvas,
                self.gear_x + random.randint(-20, 20),
                self.gear_y + random.randint(-20, 20),
                code
            )
            self.active_code_particles.append(particle)
    
    def update_particles(self):
        """Update all active code particles"""
        for particle in self.active_code_particles[:]:
            if particle.update():
                self.active_code_particles.remove(particle)

class CodeParticle:
    """Individual code particle"""
    
    def __init__(self, canvas, x, y, code):
        self.canvas = canvas
        self.x = x
        self.y = y
        self.code = code
        self.velocity_x = random.uniform(-3, 3)
        self.velocity_y = random.uniform(-5, -1)
        self.gravity = 0.2
        self.life = 60  # frames
        self.text_id = self.canvas.create_text(
            x, y, text=code, fill="#00ff00", font=("Consolas", 8, "bold"),
            tags="code_particle"
        )
    
    def update(self):
        """Update particle physics"""
        self.x += self.velocity_x
        self.y += self.velocity_y
        self.velocity_y += self.gravity
        self.life -= 1
        
        # Move text
        self.canvas.coords(self.text_id, self.x, self.y)
        
        # Fade out
        if self.life < 20:
            alpha = self.life / 20.0
            # Simulate fading by changing color intensity
            fade_color = f"#{int(0 * alpha):02x}{int(255 * alpha):02x}{int(0 * alpha):02x}"
            self.canvas.itemconfig(self.text_id, fill=fade_color)
        
        # Remove if dead
        if self.life <= 0:
            self.canvas.delete(self.text_id)
            return True
        
        return False

class CompilationEngine:
    """Main compilation engine with gears and code output"""
    
    def __init__(self, parent):
        self.parent = parent
        
        # Create compilation window
        self.window = tk.Toplevel(parent)
        self.window.title("🔥 COMPILATION ENGINE - GEARS IN MOTION 🔥")
        self.window.geometry("800x600")
        self.window.configure(bg='black')
        
        # Make window stay on top
        self.window.transient(parent)
        self.window.grab_set()
        
        # Create canvas for animation
        self.canvas = tk.Canvas(
            self.window,
            bg='#0a0a0a',
            width=800,
            height=600,
            highlightthickness=0
        )
        self.canvas.pack(fill='both', expand=True)
        
        # Compilation gears
        self.gears = []
        # Code spitters
        self.code_spitters = []
        self.create_gear_system()
        
        # Animation control
        self.animation_running = False
        self.compilation_active = False
        
        # Output display
        self.create_output_display()
        
        # Control buttons
        self.create_controls()
        
        print("🔥 Compilation Engine loaded with spinning gears!")
    
    def create_gear_system(self):
        """Create the gear system"""
        # Main drive gear (large, center)
        main_gear = CompilationGear(
            self.canvas, 400, 300, 80, 12, "#ff6600", 2.0
        )
        self.gears.append(main_gear)
        
        # Secondary gears (medium)
        gear1 = CompilationGear(
            self.canvas, 250, 200, 50, 8, "#00ff00", -1.5
        )
        self.gears.append(gear1)
        
        gear2 = CompilationGear(
            self.canvas, 550, 200, 50, 8, "#0066ff", -1.5
        )
        self.gears.append(gear2)
        
        gear3 = CompilationGear(
            self.canvas, 250, 400, 50, 8, "#ff0066", -1.5
        )
        self.gears.append(gear3)
        
        gear4 = CompilationGear(
            self.canvas, 550, 400, 50, 8, "#ffff00", -1.5
        )
        self.gears.append(gear4)
        
        # Small gears (fast spinning)
        small_gear1 = CompilationGear(
            self.canvas, 350, 250, 30, 6, "#ff00ff", 3.0
        )
        self.gears.append(small_gear1)
        
        small_gear2 = CompilationGear(
            self.canvas, 450, 250, 30, 6, "#00ffff", 3.0
        )
        self.gears.append(small_gear2)
        
        small_gear3 = CompilationGear(
            self.canvas, 350, 350, 30, 6, "#ff8800", 3.0
        )
        self.gears.append(small_gear3)
        
        small_gear4 = CompilationGear(
            self.canvas, 450, 350, 30, 6, "#88ff00", 3.0
        )
        self.gears.append(small_gear4)
        
        # Create code spitters for each gear
        for gear in self.gears:
            spitter = CodeSpitter(self.canvas, gear.x, gear.y)
            self.code_spitters.append(spitter)
    
    def create_output_display(self):
        """Create compilation output display"""
        # Output frame
        output_frame = tk.Frame(self.window, bg='black', height=150)
        output_frame.pack(fill='x', side='bottom', padx=10, pady=10)
        
        # Output label
        tk.Label(
            output_frame,
            text="🔥 COMPILATION OUTPUT 🔥",
            bg='black',
            fg='#ff6600',
            font=('Consolas', 12, 'bold')
        ).pack(anchor='w')
        
        # Output text area
        self.output_text = tk.Text(
            output_frame,
            bg='#1a1a1a',
            fg='#00ff00',
            font=('Consolas', 9),
            height=6,
            wrap='word'
        )
        self.output_text.pack(fill='both', expand=True)
        
        # Scrollbar
        scrollbar = tk.Scrollbar(output_frame)
        scrollbar.pack(side='right', fill='y')
        self.output_text.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.output_text.yview)
    
    def create_controls(self):
        """Create control buttons"""
        control_frame = tk.Frame(self.window, bg='black')
        control_frame.pack(fill='x', padx=10, pady=5)
        
        tk.Button(
            control_frame,
            text="🔥 START COMPILATION",
            bg='#ff6600',
            fg='white',
            font=('Consolas', 10, 'bold'),
            command=self.start_compilation
        ).pack(side='left', padx=5)
        
        tk.Button(
            control_frame,
            text="⏹️ STOP",
            bg='#ff0000',
            fg='white',
            font=('Consolas', 10, 'bold'),
            command=self.stop_compilation
        ).pack(side='left', padx=5)
        
        tk.Button(
            control_frame,
            text="🧹 CLEAR OUTPUT",
            bg='#666666',
            fg='white',
            font=('Consolas', 10, 'bold'),
            command=self.clear_output
        ).pack(side='left', padx=5)
        
        tk.Button(
            control_frame,
            text="💾 SAVE OUTPUT",
            bg='#0066ff',
            fg='white',
            font=('Consolas', 10, 'bold'),
            command=self.save_output
        ).pack(side='right', padx=5)
    
    def start_compilation(self):
        """Start the compilation process"""
        if self.compilation_active:
            return
        
        self.compilation_active = True
        self.animation_running = True
        
        # Clear output
        self.output_text.delete(1.0, tk.END)
        
        # Start gears spinning
        for gear in self.gears:
            gear.start_spinning()
        
        # Start animation loop
        self.animate()
        
        # Start compilation simulation
        self.simulate_compilation()
        
        print("🔥 Compilation started - gears spinning!")
    
    def stop_compilation(self):
        """Stop the compilation process"""
        self.compilation_active = False
        self.animation_running = False
        
        # Stop gears
        for gear in self.gears:
            gear.stop_spinning()
        
        print("⏹️ Compilation stopped")
    
    def animate(self):
        """Animation loop"""
        if self.animation_running:
            # Update all gears
            for gear in self.gears:
                gear.update_rotation()
            
            # Update code spitters
            for spitter in self.code_spitters:
                spitter.update_particles()
            
            # Schedule next frame
            self.canvas.after(50, self.animate)
    
    def simulate_compilation(self):
        """Simulate compilation process"""
        if not self.compilation_active:
            return
        
        # Compilation steps with different outputs
        steps = [
            ("🔥 Starting compilation...", 1000),
            ("⚡ Preprocessing source files...", 1500),
            ("🎯 Parsing syntax...", 2000),
            ("🧠 Semantic analysis...", 2500),
            ("💉 Code optimization...", 3000),
            ("🔧 Linking objects...", 3500),
            ("💀 Generating machine code...", 4000),
            ("🎨 Creating executable...", 4500),
            ("✅ Compilation complete!", 5000)
        ]
        
        def compilation_step(step_index):
            if step_index < len(steps) and self.compilation_active:
                message, delay = steps[step_index]
                
                # Add output message
                self.add_output(message)
                
                # Spit code from random gears
                if step_index > 0:  # Don't spit on first step
                    for _ in range(random.randint(2, 5)):
                        spitter = random.choice(self.code_spitters)
                        spitter.spit_code(random.randint(1, 3))
                
                # Schedule next step
                self.window.after(delay, lambda: compilation_step(step_index + 1))
            else:
                # Compilation complete
                self.compilation_active = False
                self.add_output("\n🎉 Compilation successful! Executable created.")
                self.add_output("💀 Ready to hack the matrix!")
        
        # Start compilation steps
        compilation_step(0)
    
    def add_output(self, message):
        """Add message to output display"""
        timestamp = time.strftime("%H:%M:%S")
        self.output_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.output_text.see(tk.END)
    
    def clear_output(self):
        """Clear output display"""
        self.output_text.delete(1.0, tk.END)
        print("🧹 Output cleared")
    
    def save_output(self):
        """Save compilation output to file"""
        output_content = self.output_text.get(1.0, tk.END)
        try:
            with open("compilation_output.txt", "w") as f:
                f.write(output_content)
            messagebox.showinfo("💾 SAVED", "Compilation output saved to compilation_output.txt")
        except Exception as e:
            messagebox.showerror("❌ ERROR", f"Failed to save output: {e}")

class GraffitiIDEWithGears:
    """Enhanced Graffiti IDE with compilation gears"""
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("🔥 GRAFFITI IDE WITH COMPILATION GEARS 🔥")
        self.root.geometry("1200x800")
        self.root.configure(bg='black')
        
        # Create main interface
        self.create_interface()
        
        print("🔥 Graffiti IDE with Compilation Gears loaded!")
    
    def create_interface(self):
        """Create main IDE interface"""
        # Title
        title_label = tk.Label(
            self.root,
            text="🔥 GRAFFITI IDE - COMPILATION GEARS EDITION 🔥",
            bg='black',
            fg='#ff6600',
            font=('Consolas', 16, 'bold')
        )
        title_label.pack(pady=10)
        
        # Code editor area
        editor_frame = tk.Frame(self.root, bg='black')
        editor_frame.pack(fill='both', expand=True, padx=20, pady=10)
        
        # Code editor
        self.code_editor = tk.Text(
            editor_frame,
            bg='#1a1a1a',
            fg='#00ff00',
            font=('Consolas', 12),
            insertbackground='#00ff00'
        )
        self.code_editor.pack(fill='both', expand=True)
        
        # Add sample code
        sample_code = """// 🔥 ELITE HACKER CODE 🔥
#include <windows.h>
#include <stdio.h>

int main() {
    printf("💀 HACKING THE MATRIX WITH GEARS... 💀\\n");
    
    // Memory manipulation
    DWORD dwAddress = 0x401000;
    BYTE* pBytes = (BYTE*)dwAddress;
    *pBytes = 0x90; // NOP instruction
    
    // Process injection
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 1337);
    if (hProcess) {
        printf("🎯 Process opened! Gears spinning...\\n");
        // Shellcode injection would go here
    }
    
    return 0;
}"""
        
        self.code_editor.insert('1.0', sample_code)
        
        # Control buttons
        button_frame = tk.Frame(self.root, bg='black')
        button_frame.pack(fill='x', padx=20, pady=10)
        
        tk.Button(
            button_frame,
            text="🔥 COMPILE WITH GEARS",
            bg='#ff6600',
            fg='white',
            font=('Consolas', 12, 'bold'),
            command=self.open_compilation_engine
        ).pack(side='left', padx=10)
        
        tk.Button(
            button_frame,
            text="🎨 SPRAY GRAFFITI",
            bg='#00ff00',
            fg='black',
            font=('Consolas', 12, 'bold'),
            command=self.spray_graffiti
        ).pack(side='left', padx=10)
        
        tk.Button(
            button_frame,
            text="💀 RUN EXPLOIT",
            bg='#ff0066',
            fg='white',
            font=('Consolas', 12, 'bold'),
            command=self.run_exploit
        ).pack(side='right', padx=10)
    
    def open_compilation_engine(self):
        """Open the compilation engine with gears"""
        compilation_engine = CompilationEngine(self.root)
        print("🔥 Compilation engine opened!")
    
    def spray_graffiti(self):
        """Spray graffiti on the code"""
        graffiti_code = [
            "// 💀 HACKED BY ELITE 💀",
            "// 🔥 GEARS SPINNING 🔥",
            "// 🎯 TARGET ACQUIRED 🎯",
            "// ⚡ COMPILING MATRIX ⚡",
            "// 💉 INJECTING SHELLCODE 💉",
            "// 🧠 MEMORY SCANNED 🧠",
            "// 🌐 NETWORK COMPROMISED 🌐",
            "// 🔓 ROOT ACCESS GRANTED 🔓"
        ]
        
        selected_graffiti = random.choice(graffiti_code)
        cursor_pos = self.code_editor.index(tk.INSERT)
        self.code_editor.insert(cursor_pos, f"\n{selected_graffiti}\n")
        print(f"🎨 Graffiti sprayed: {selected_graffiti}")
    
    def run_exploit(self):
        """Run the exploit"""
        messagebox.showinfo(
            "💀 EXPLOIT RUNNING",
            "🔥 Compilation gears spinning...\n"
            "⚡ Code being processed...\n"
            "🎯 Target compromised...\n"
            "💀 Matrix hacked successfully!"
        )
        print("💀 Exploit executed!")

def main():
    """Launch the Graffiti IDE with Compilation Gears"""
    print("🔥 Initializing Graffiti IDE with Compilation Gears...")
    print("⚡ Loading spinning gear system...")
    print("🎯 Preparing code spitters...")
    print("💀 Ready to compile with style!")
    
    app = GraffitiIDEWithGears()
    app.root.mainloop()

if __name__ == "__main__":
    main()
