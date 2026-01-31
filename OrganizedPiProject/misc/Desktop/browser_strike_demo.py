#!/usr/bin/env python3
"""
Browser Strike - Assault on Boredom (Python Demo)
A demonstration of the game engine concepts in Python
"""

import random
import math
from typing import List, Tuple, NamedTuple
from enum import Enum

class Material(Enum):
    WOOD = "Wood"
    METAL = "Metal"
    CONCRETE = "Concrete"
    STONE = "Stone"

class CameraMode(Enum):
    FIRST_PERSON = "First Person"
    THIRD_PERSON = "Third Person"

class Vec3:
    def __init__(self, x: float = 0, y: float = 0, z: float = 0):
        self.x, self.y, self.z = x, y, z
    
    def __add__(self, other):
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)
    
    def __sub__(self, other):
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)
    
    def __mul__(self, scale):
        return Vec3(self.x * scale, self.y * scale, self.z * scale)
    
    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)
    
    def normalize(self):
        length = self.length()
        return self * (1.0/length) if length > 0 else Vec3()
    
    def __str__(self):
        return f"({self.x:.1f}, {self.y:.1f}, {self.z:.1f})"

class Wall:
    def __init__(self, pos: Vec3, size: Vec3, material: Material, owner_id: int = -1):
        self.pos = pos
        self.size = size
        self.material = material
        self.owner_id = owner_id

class Spawn:
    def __init__(self, pos: Vec3, team_id: int = 0):
        self.pos = pos
        self.team_id = team_id

class LootSpawn:
    def __init__(self, pos: Vec3, item_type: str, quantity: int = 1):
        self.pos = pos
        self.item_type = item_type
        self.quantity = quantity

class Map:
    def __init__(self, name: str = "Generated Map"):
        self.walls: List[Wall] = []
        self.spawns: List[Spawn] = []
        self.loot_spawns: List[LootSpawn] = []
        self.size = Vec3(100, 20, 100)
        self.name = name

class Camera:
    def __init__(self):
        self.mode = CameraMode.FIRST_PERSON
        self.eye = Vec3(0, 2, 5)
        self.target = Vec3(0, 0, 0)
        self.yaw = 0.0
        self.pitch = 0.0
        self.third_person_dist = 4.0
    
    def update_first_person(self, pos: Vec3, yaw_rad: float, pitch_rad: float):
        self.mode = CameraMode.FIRST_PERSON
        self.yaw = yaw_rad
        self.pitch = pitch_rad
        self.eye = pos
        self.target = pos + Vec3(
            math.cos(yaw_rad) * math.cos(pitch_rad),
            math.sin(pitch_rad),
            math.sin(yaw_rad) * math.cos(pitch_rad)
        )
    
    def update_third_person(self, pos: Vec3, yaw_rad: float):
        self.mode = CameraMode.THIRD_PERSON
        self.yaw = yaw_rad
        behind = pos - Vec3(math.cos(yaw_rad), 0, math.sin(yaw_rad)) * self.third_person_dist
        self.eye = behind + Vec3(0, 2, 0)
        self.target = pos + Vec3(0, 1, 0)
    
    def toggle_mode(self):
        self.mode = CameraMode.THIRD_PERSON if self.mode == CameraMode.FIRST_PERSON else CameraMode.FIRST_PERSON

class SimpleMapGenerator:
    def generate(self, seed: int, width: int, height: int) -> Map:
        random.seed(seed)
        map_obj = Map("Simple Arena")
        map_obj.size = Vec3(width, 20, height)
        
        # Generate random walls
        for i in range(15):
            pos = Vec3(random.randint(0, 80), 0, random.randint(0, 80))
            size = Vec3(random.randint(4, 12), 3, random.randint(4, 12))
            material = random.choice(list(Material))
            map_obj.walls.append(Wall(pos, size, material))
        
        # Generate spawn points
        for i in range(8):
            pos = Vec3(random.randint(0, 80), 1, random.randint(0, 80))
            team_id = i % 2
            map_obj.spawns.append(Spawn(pos, team_id))
        
        # Generate loot spawns
        loot_types = ["ammo", "health", "armor", "weapon"]
        for i in range(10):
            pos = Vec3(random.randint(0, 80), 0.5, random.randint(0, 80))
            item_type = loot_types[i % len(loot_types)]
            quantity = 1 + (i % 3)
            map_obj.loot_spawns.append(LootSpawn(pos, item_type, quantity))
        
        return map_obj

class TextRenderer:
    def render_map(self, map_obj: Map):
        print(f"\n=== MAP: {map_obj.name} ===")
        print(f"Walls: {len(map_obj.walls)}")
        print(f"Spawns: {len(map_obj.spawns)}")
        print(f"Loot: {len(map_obj.loot_spawns)}")
        
        print("\nWalls:")
        for i, wall in enumerate(map_obj.walls[:5]):
            print(f"  {i+1}. {wall.material.value} wall at {wall.pos}")
        
        print("\nSpawns:")
        for i, spawn in enumerate(map_obj.spawns[:3]):
            print(f"  {i+1}. Team {spawn.team_id} at {spawn.pos}")
    
    def render_player(self, pos: Vec3, camera: Camera):
        print(f"\n=== PLAYER ===")
        print(f"Position: {pos}")
        print(f"Camera: {camera.mode.value}")
        print(f"Camera Eye: {camera.eye}")
        print(f"Camera Target: {camera.target}")
    
    def render_hud(self, wireframe_mode: bool, camera: Camera):
        print(f"\n=== HUD ===")
        print(f"Mode: {camera.mode.value}")
        print(f"Wireframe: {'ON' if wireframe_mode else 'OFF'}")
        print("Controls: WASD=Move, V=Toggle Camera, W=Toggle Wireframe, Q=Quit")

class GameState:
    def __init__(self):
        self.camera = Camera()
        self.map = None
        self.player_pos = Vec3(0, 1, 0)
        self.player_yaw = 0.0
        self.player_pitch = 0.0
        self.wireframe_mode = True
        self.running = True

class InputHandler:
    def process_input(self, state: GameState):
        print(f"\nEnter command (WASD=move, V=toggle camera, W=toggle wireframe, Q=quit): ", end="")
        command = input().lower()
        
        move_speed = 1.0
        
        if command == 'w':
            if state.wireframe_mode:
                state.wireframe_mode = False
                print("Wireframe mode OFF")
            else:
                state.wireframe_mode = True
                print("Wireframe mode ON")
        elif command == 'a':
            state.player_pos = state.player_pos + Vec3(-move_speed, 0, 0)
            print("Moved left")
        elif command == 's':
            state.player_pos = state.player_pos + Vec3(0, 0, -move_speed)
            print("Moved back")
        elif command == 'd':
            state.player_pos = state.player_pos + Vec3(move_speed, 0, 0)
            print("Moved right")
        elif command == 'v':
            state.camera.toggle_mode()
            print(f"Camera toggled to {state.camera.mode.value}")
        elif command == 'q':
            state.running = False
            print("Quitting...")
        else:
            print("Invalid command")
        
        # Update camera
        if state.camera.mode == CameraMode.FIRST_PERSON:
            state.camera.update_first_person(state.player_pos, state.player_yaw, state.player_pitch)
        else:
            state.camera.update_third_person(state.player_pos, state.player_yaw)

def main():
    print("Browser Strike - Assault on Boredom (Python Demo)")
    print("=" * 60)
    
    # Initialize game state
    state = GameState()
    generator = SimpleMapGenerator()
    renderer = TextRenderer()
    input_handler = InputHandler()
    
    # Generate map
    state.map = generator.generate(12345, 100, 100)
    
    print("\nWelcome to Browser Strike!")
    print("This is a text-based demonstration of the game engine.")
    print("In the full version, you'd see 3D graphics with XQZ wireframe rendering!")
    
    # Game loop
    while state.running:
        # Render current state
        renderer.render_map(state.map)
        renderer.render_player(state.player_pos, state.camera)
        renderer.render_hud(state.wireframe_mode, state.camera)
        
        # Process input
        input_handler.process_input(state)
    
    print("\nThanks for playing Browser Strike!")
    print("The full engine supports:")
    print("  - Third-person camera toggle")
    print("  - Procedural map generation")
    print("  - Multiple game modes")
    print("  - Vehicle system")
    print("  - Networking architecture")
    print("  - XQZ wireframe rendering")

if __name__ == "__main__":
    main()
