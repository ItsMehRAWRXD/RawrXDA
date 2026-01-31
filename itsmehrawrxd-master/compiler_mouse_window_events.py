#!/usr/bin/env python3
"""
Compiler Mouse and Window Event Handling for n0mn0m IDE
Complete mouse and window event management with tab support
"""

import tkinter as tk
from tkinter import ttk
import threading
import time
from typing import Dict, List, Optional, Callable, Any, Tuple
from enum import Enum
from dataclasses import dataclass
import json

class EventType(Enum):
    """Event types for mouse and window events"""
    MOUSE_LEFT_CLICK = "mouse_left_click"
    MOUSE_RIGHT_CLICK = "mouse_right_click"
    MOUSE_MIDDLE_CLICK = "mouse_middle_click"
    MOUSE_DOUBLE_CLICK = "mouse_double_click"
    MOUSE_MOTION = "mouse_motion"
    MOUSE_ENTER = "mouse_enter"
    MOUSE_LEAVE = "mouse_leave"
    MOUSE_WHEEL_UP = "mouse_wheel_up"
    MOUSE_WHEEL_DOWN = "mouse_wheel_down"
    KEY_PRESS = "key_press"
    KEY_RELEASE = "key_release"
    KEY_TAB = "key_tab"
    KEY_SHIFT_TAB = "key_shift_tab"
    WINDOW_FOCUS_IN = "window_focus_in"
    WINDOW_FOCUS_OUT = "window_focus_out"
    WINDOW_RESIZE = "window_resize"
    WINDOW_MOVE = "window_move"
    WINDOW_CLOSE = "window_close"
    TAB_SWITCH = "tab_switch"
    TAB_CLOSE = "tab_close"
    TAB_NEW = "tab_new"

@dataclass
class EventData:
    """Event data structure"""
    event_type: EventType
    widget: tk.Widget
    x: int = 0
    y: int = 0
    button: int = 0
    keysym: str = ""
    char: str = ""
    delta: int = 0
    timestamp: float = 0.0
    modifiers: List[str] = None
    tab_info: Optional[Dict] = None
    
    def __post_init__(self):
        if self.modifiers is None:
            self.modifiers = []
        if self.timestamp == 0.0:
            self.timestamp = time.time()

class TabManager:
    """Enhanced tab manager with keyboard navigation"""
    
    def __init__(self, parent_widget):
        self.parent = parent_widget
        self.tabs = {}
        self.active_tab = None
        self.tab_order = []
        self.tab_index = 0
        
        # Tab navigation
        self.tab_navigation_enabled = True
        self.cycle_tabs = True
        
        print("📑 Tab Manager initialized")
    
    def create_tab(self, tab_id: str, title: str, content_widget: tk.Widget) -> bool:
        """Create a new tab"""
        
        try:
            self.tabs[tab_id] = {
                'id': tab_id,
                'title': title,
                'content': content_widget,
                'created': time.time(),
                'active': False
            }
            
            self.tab_order.append(tab_id)
            
            if not self.active_tab:
                self.set_active_tab(tab_id)
            
            print(f"✅ Tab created: {title}")
            return True
            
        except Exception as e:
            print(f"❌ Error creating tab: {e}")
            return False
    
    def set_active_tab(self, tab_id: str) -> bool:
        """Set active tab"""
        
        try:
            if tab_id not in self.tabs:
                return False
            
            # Hide all tabs
            for tid, tab_info in self.tabs.items():
                tab_info['content'].pack_forget()
                tab_info['active'] = False
            
            # Show active tab
            self.tabs[tab_id]['content'].pack(fill='both', expand=True)
            self.tabs[tab_id]['active'] = True
            
            self.active_tab = tab_id
            self.tab_index = self.tab_order.index(tab_id)
            
            print(f"✅ Active tab set: {self.tabs[tab_id]['title']}")
            return True
            
        except Exception as e:
            print(f"❌ Error setting active tab: {e}")
            return False
    
    def close_tab(self, tab_id: str) -> bool:
        """Close tab"""
        
        try:
            if tab_id not in self.tabs:
                return False
            
            # Remove from tabs
            tab_info = self.tabs[tab_id]
            del self.tabs[tab_id]
            
            # Remove from order
            if tab_id in self.tab_order:
                self.tab_order.remove(tab_id)
            
            # Destroy content widget
            tab_info['content'].destroy()
            
            # Set new active tab if needed
            if self.active_tab == tab_id:
                if self.tab_order:
                    new_active = self.tab_order[min(self.tab_index, len(self.tab_order) - 1)]
                    self.set_active_tab(new_active)
                else:
                    self.active_tab = None
            
            print(f"✅ Tab closed: {tab_info['title']}")
            return True
            
        except Exception as e:
            print(f"❌ Error closing tab: {e}")
            return False
    
    def next_tab(self) -> bool:
        """Switch to next tab"""
        
        if not self.tab_order:
            return False
        
        try:
            if self.tab_index < len(self.tab_order) - 1:
                self.tab_index += 1
            elif self.cycle_tabs:
                self.tab_index = 0
            
            if self.tab_index < len(self.tab_order):
                next_tab_id = self.tab_order[self.tab_index]
                return self.set_active_tab(next_tab_id)
            
            return False
            
        except Exception as e:
            print(f"❌ Error switching to next tab: {e}")
            return False
    
    def previous_tab(self) -> bool:
        """Switch to previous tab"""
        
        if not self.tab_order:
            return False
        
        try:
            if self.tab_index > 0:
                self.tab_index -= 1
            elif self.cycle_tabs:
                self.tab_index = len(self.tab_order) - 1
            
            if self.tab_index >= 0:
                prev_tab_id = self.tab_order[self.tab_index]
                return self.set_active_tab(prev_tab_id)
            
            return False
            
        except Exception as e:
            print(f"❌ Error switching to previous tab: {e}")
            return False
    
    def get_tab_info(self, tab_id: str) -> Optional[Dict]:
        """Get tab information"""
        
        return self.tabs.get(tab_id)
    
    def get_all_tabs(self) -> Dict[str, Dict]:
        """Get all tabs"""
        
        return self.tabs.copy()
    
    def get_active_tab(self) -> Optional[str]:
        """Get active tab ID"""
        
        return self.active_tab

class MouseWindowEventHandler:
    """
    Enhanced mouse and window event handling system with tab support
    Advanced event management with gesture recognition and tab navigation
    """
    
    def __init__(self, root_widget: tk.Widget):
        self.root = root_widget
        self.event_handlers = {}
        self.event_history = []
        self.gesture_patterns = {}
        self.automation_rules = {}
        self.event_filters = {}
        self.event_statistics = {}
        
        # Tab management
        self.tab_manager = TabManager(root_widget)
        
        # Event timing
        self.last_click_time = 0
        self.click_threshold = 0.5  # Double click threshold
        self.drag_threshold = 5     # Drag threshold in pixels
        
        # Gesture recognition
        self.mouse_trail = []
        self.max_trail_length = 100
        
        # Event recording
        self.recording = False
        self.recorded_events = []
        
        # Performance monitoring
        self.event_counts = {}
        self.performance_metrics = {}
        
        # Tab navigation settings
        self.tab_navigation_keys = {
            'next_tab': ['<Control-Tab>', '<Control-Right>'],
            'prev_tab': ['<Control-Shift-Tab>', '<Control-Left>'],
            'close_tab': ['<Control-w>'],
            'new_tab': ['<Control-t>']
        }
        
        print("🖱️ Enhanced Mouse and Window Event Handler initialized")
    
    def setup_tab_navigation(self):
        """Setup tab navigation key bindings"""
        
        # Bind tab navigation keys
        for action, key_combinations in self.tab_navigation_keys.items():
            for key_combo in key_combinations:
                self.root.bind(key_combo, lambda e, action=action: self._handle_tab_navigation(e, action))
        
        print("⌨️ Tab navigation key bindings setup")
    
    def _handle_tab_navigation(self, event, action: str):
        """Handle tab navigation key events"""
        
        try:
            if action == 'next_tab':
                success = self.tab_manager.next_tab()
                if success:
                    self._create_tab_event(EventType.TAB_SWITCH, 'next')
            
            elif action == 'prev_tab':
                success = self.tab_manager.previous_tab()
                if success:
                    self._create_tab_event(EventType.TAB_SWITCH, 'previous')
            
            elif action == 'close_tab':
                if self.tab_manager.active_tab:
                    success = self.tab_manager.close_tab(self.tab_manager.active_tab)
                    if success:
                        self._create_tab_event(EventType.TAB_CLOSE, self.tab_manager.active_tab)
            
            elif action == 'new_tab':
                # Create new tab - this would be implemented by the IDE
                self._create_tab_event(EventType.TAB_NEW, 'new')
            
        except Exception as e:
            print(f"❌ Tab navigation error: {e}")
    
    def _create_tab_event(self, event_type: EventType, tab_action: str):
        """Create tab-related event"""
        
        event_data = EventData(
            event_type=event_type,
            widget=self.root,
            tab_info={
                'action': tab_action,
                'active_tab': self.tab_manager.active_tab,
                'tab_count': len(self.tab_manager.tabs)
            }
        )
        
        self._process_event(event_data)
    
    def bind_event(self, event_type: EventType, handler: Callable, 
                   widget: Optional[tk.Widget] = None, 
                   priority: int = 0) -> str:
        """Bind event handler with priority"""
        
        if widget is None:
            widget = self.root
        
        handler_id = f"{event_type.value}_{int(time.time())}_{priority}"
        
        self.event_handlers[handler_id] = {
            'event_type': event_type,
            'handler': handler,
            'widget': widget,
            'priority': priority,
            'active': True
        }
        
        # Bind to actual widget
        self._bind_to_widget(widget, event_type)
        
        print(f"✅ Event bound: {event_type.value} -> {handler_id}")
        return handler_id
    
    def unbind_event(self, handler_id: str) -> bool:
        """Unbind event handler"""
        
        if handler_id in self.event_handlers:
            self.event_handlers[handler_id]['active'] = False
            del self.event_handlers[handler_id]
            print(f"✅ Event unbound: {handler_id}")
            return True
        return False
    
    def _bind_to_widget(self, widget: tk.Widget, event_type: EventType):
        """Bind event to actual widget"""
        
        event_map = {
            EventType.MOUSE_LEFT_CLICK: '<Button-1>',
            EventType.MOUSE_RIGHT_CLICK: '<Button-3>',
            EventType.MOUSE_MIDDLE_CLICK: '<Button-2>',
            EventType.MOUSE_DOUBLE_CLICK: '<Double-Button-1>',
            EventType.MOUSE_MOTION: '<Motion>',
            EventType.MOUSE_ENTER: '<Enter>',
            EventType.MOUSE_LEAVE: '<Leave>',
            EventType.MOUSE_WHEEL_UP: '<MouseWheel>',
            EventType.MOUSE_WHEEL_DOWN: '<MouseWheel>',
            EventType.KEY_PRESS: '<KeyPress>',
            EventType.KEY_RELEASE: '<KeyRelease>',
            EventType.KEY_TAB: '<Tab>',
            EventType.KEY_SHIFT_TAB: '<Shift-Tab>',
            EventType.WINDOW_FOCUS_IN: '<FocusIn>',
            EventType.WINDOW_FOCUS_OUT: '<FocusOut>',
            EventType.WINDOW_RESIZE: '<Configure>',
            EventType.WINDOW_MOVE: '<Configure>',
            EventType.WINDOW_CLOSE: '<WM_DELETE_WINDOW>'
        }
        
        tk_event = event_map.get(event_type)
        if tk_event:
            widget.bind(tk_event, lambda e: self._handle_event(e, event_type))
    
    def _handle_event(self, tk_event, event_type: EventType):
        """Handle incoming tkinter events"""
        
        start_time = time.time()
        
        # Create event data
        event_data = self._create_event_data(tk_event, event_type)
        
        # Process the event
        self._process_event(event_data)
        
        # Update performance metrics
        processing_time = time.time() - start_time
        self._update_performance_metrics(event_type, processing_time)
    
    def _process_event(self, event_data: EventData):
        """Process event data"""
        
        # Update statistics
        self._update_statistics(event_data.event_type)
        
        # Add to history
        self._add_to_history(event_data)
        
        # Update mouse trail for gesture recognition
        if event_data.event_type in [EventType.MOUSE_MOTION, EventType.MOUSE_LEFT_CLICK]:
            self._update_mouse_trail(event_data)
        
        # Check for gestures
        gesture = self._recognize_gesture(event_data)
        if gesture:
            self._handle_gesture(gesture)
        
        # Apply event filters
        if self._should_filter_event(event_data):
            return
        
        # Execute handlers
        self._execute_handlers(event_data)
        
        # Record event if recording
        if self.recording:
            self._record_event(event_data)
    
    def _create_event_data(self, tk_event, event_type: EventType) -> EventData:
        """Create EventData from tkinter event"""
        
        # Extract modifiers
        modifiers = []
        if hasattr(tk_event, 'state'):
            if tk_event.state & 0x1: modifiers.append('Shift')
            if tk_event.state & 0x4: modifiers.append('Ctrl')
            if tk_event.state & 0x8: modifiers.append('Alt')
            if tk_event.state & 0x10: modifiers.append('Cmd')
        
        # Create event data
        event_data = EventData(
            event_type=event_type,
            widget=tk_event.widget,
            x=getattr(tk_event, 'x', 0),
            y=getattr(tk_event, 'y', 0),
            button=getattr(tk_event, 'num', 0),
            keysym=getattr(tk_event, 'keysym', ''),
            char=getattr(tk_event, 'char', ''),
            delta=getattr(tk_event, 'delta', 0),
            modifiers=modifiers
        )
        
        return event_data
    
    def _update_statistics(self, event_type: EventType):
        """Update event statistics"""
        
        if event_type not in self.event_counts:
            self.event_counts[event_type] = 0
        self.event_counts[event_type] += 1
    
    def _add_to_history(self, event_data: EventData):
        """Add event to history"""
        
        self.event_history.append(event_data)
        
        # Keep history size manageable
        if len(self.event_history) > 1000:
            self.event_history = self.event_history[-500:]
    
    def _update_mouse_trail(self, event_data: EventData):
        """Update mouse trail for gesture recognition"""
        
        self.mouse_trail.append((event_data.x, event_data.y, event_data.timestamp))
        
        # Keep trail size manageable
        if len(self.mouse_trail) > self.max_trail_length:
            self.mouse_trail = self.mouse_trail[-self.max_trail_length:]
    
    def _recognize_gesture(self, event_data: EventData) -> Optional[str]:
        """Recognize mouse gestures"""
        
        if len(self.mouse_trail) < 5:
            return None
        
        # Simple gesture recognition
        recent_trail = self.mouse_trail[-10:]
        
        # Check for circle gesture (switch tabs)
        if self._is_circle_gesture(recent_trail):
            return 'circle'
        
        # Check for swipe gesture (next/previous tab)
        if self._is_swipe_gesture(recent_trail):
            return 'swipe'
        
        # Check for zigzag gesture (close tab)
        if self._is_zigzag_gesture(recent_trail):
            return 'zigzag'
        
        return None
    
    def _is_circle_gesture(self, trail: List[Tuple[int, int, float]]) -> bool:
        """Check if trail forms a circle"""
        
        if len(trail) < 8:
            return False
        
        # Simple circle detection
        center_x = sum(point[0] for point in trail) / len(trail)
        center_y = sum(point[1] for point in trail) / len(trail)
        
        distances = [((point[0] - center_x) ** 2 + (point[1] - center_y) ** 2) ** 0.5 
                    for point in trail]
        
        avg_distance = sum(distances) / len(distances)
        variance = sum((d - avg_distance) ** 2 for d in distances) / len(distances)
        
        return variance < avg_distance * 0.3
    
    def _is_swipe_gesture(self, trail: List[Tuple[int, int, float]]) -> bool:
        """Check if trail forms a swipe"""
        
        if len(trail) < 3:
            return False
        
        start_x, start_y = trail[0][0], trail[0][1]
        end_x, end_y = trail[-1][0], trail[-1][1]
        
        total_distance = ((end_x - start_x) ** 2 + (end_y - start_y) ** 2) ** 0.5
        if total_distance < 20:
            return False
        
        # Check if intermediate points are close to the line
        max_deviation = 0
        for i, (x, y, _) in enumerate(trail[1:-1]):
            t = i / (len(trail) - 1)
            expected_x = start_x + t * (end_x - start_x)
            expected_y = start_y + t * (end_y - start_y)
            deviation = ((x - expected_x) ** 2 + (y - expected_y) ** 2) ** 0.5
            max_deviation = max(max_deviation, deviation)
        
        return max_deviation < 15
    
    def _is_zigzag_gesture(self, trail: List[Tuple[int, int, float]]) -> bool:
        """Check if trail forms a zigzag"""
        
        if len(trail) < 6:
            return False
        
        direction_changes = 0
        for i in range(2, len(trail)):
            dx1 = trail[i-1][0] - trail[i-2][0]
            dy1 = trail[i-1][1] - trail[i-2][1]
            dx2 = trail[i][0] - trail[i-1][0]
            dy2 = trail[i][1] - trail[i-1][1]
            
            if dx1 * dx2 < 0 or dy1 * dy2 < 0:
                direction_changes += 1
        
        return direction_changes >= 3
    
    def _handle_gesture(self, gesture: str):
        """Handle recognized gesture"""
        
        print(f"🎯 Gesture recognized: {gesture}")
        
        # Map gestures to tab actions
        gesture_actions = {
            'circle': lambda: self.tab_manager.next_tab(),
            'swipe': lambda: self.tab_manager.next_tab(),
            'zigzag': lambda: self.tab_manager.close_tab(self.tab_manager.active_tab) if self.tab_manager.active_tab else None
        }
        
        if gesture in gesture_actions:
            try:
                action = gesture_actions[gesture]
                if action:
                    action()
                    self._create_tab_event(EventType.TAB_SWITCH, f'gesture_{gesture}')
            except Exception as e:
                print(f"⚠️ Gesture action error: {e}")
        
        # Execute gesture handlers
        for handler_id, handler_info in self.event_handlers.items():
            if (handler_info['active'] and 
                handler_info['event_type'].value == f"gesture_{gesture}"):
                try:
                    handler_info['handler'](gesture)
                except Exception as e:
                    print(f"⚠️ Gesture handler error: {e}")
    
    def _should_filter_event(self, event_data: EventData) -> bool:
        """Check if event should be filtered"""
        
        for filter_id, filter_func in self.event_filters.items():
            try:
                if filter_func(event_data):
                    return True
            except Exception as e:
                print(f"⚠️ Event filter error: {e}")
        
        return False
    
    def _execute_handlers(self, event_data: EventData):
        """Execute event handlers in priority order"""
        
        # Get handlers for this event type
        handlers = [
            (handler_id, handler_info)
            for handler_id, handler_info in self.event_handlers.items()
            if (handler_info['active'] and 
                handler_info['event_type'] == event_data.event_type)
        ]
        
        # Sort by priority (higher priority first)
        handlers.sort(key=lambda x: x[1]['priority'], reverse=True)
        
        # Execute handlers
        for handler_id, handler_info in handlers:
            try:
                handler_info['handler'](event_data)
            except Exception as e:
                print(f"⚠️ Event handler error: {e}")
    
    def _record_event(self, event_data: EventData):
        """Record event for playback"""
        
        self.recorded_events.append({
            'timestamp': event_data.timestamp,
            'event_type': event_data.event_type.value,
            'x': event_data.x,
            'y': event_data.y,
            'button': event_data.button,
            'keysym': event_data.keysym,
            'char': event_data.char,
            'modifiers': event_data.modifiers,
            'tab_info': event_data.tab_info
        })
    
    def _update_performance_metrics(self, event_type: EventType, processing_time: float):
        """Update performance metrics"""
        
        if event_type not in self.performance_metrics:
            self.performance_metrics[event_type] = {
                'total_time': 0,
                'count': 0,
                'avg_time': 0,
                'max_time': 0,
                'min_time': float('inf')
            }
        
        metrics = self.performance_metrics[event_type]
        metrics['total_time'] += processing_time
        metrics['count'] += 1
        metrics['avg_time'] = metrics['total_time'] / metrics['count']
        metrics['max_time'] = max(metrics['max_time'], processing_time)
        metrics['min_time'] = min(metrics['min_time'], processing_time)
    
    def get_tab_manager(self) -> TabManager:
        """Get tab manager instance"""
        
        return self.tab_manager
    
    def get_statistics(self) -> Dict[str, Any]:
        """Get event statistics"""
        
        return {
            'event_counts': self.event_counts,
            'performance_metrics': self.performance_metrics,
            'active_handlers': len([h for h in self.event_handlers.values() if h['active']]),
            'total_events': len(self.event_history),
            'recording': self.recording,
            'recorded_events': len(self.recorded_events),
            'tab_count': len(self.tab_manager.tabs),
            'active_tab': self.tab_manager.active_tab
        }

# Integration function
def integrate_mouse_window_events(ide_instance):
    """Integrate mouse and window event handling with IDE"""
    
    if hasattr(ide_instance, 'root'):
        ide_instance.event_handler = MouseWindowEventHandler(ide_instance.root)
        ide_instance.event_handler.setup_tab_navigation()
        print("🖱️ Enhanced mouse and window event handling integrated with IDE")
    else:
        print("⚠️ No root widget found for event handling")

if __name__ == "__main__":
    print("🖱️ Enhanced Compiler Mouse and Window Event Handling")
    print("=" * 60)
    
    # Test the event handler
    root = tk.Tk()
    root.title("Enhanced Event Handler Test")
    root.geometry("600x400")
    
    event_handler = MouseWindowEventHandler(root)
    event_handler.setup_tab_navigation()
    
    # Create test tabs
    notebook = ttk.Notebook(root)
    notebook.pack(fill='both', expand=True, padx=10, pady=10)
    
    for i in range(3):
        frame = ttk.Frame(notebook)
        notebook.add(frame, text=f"Tab {i+1}")
        
        label = ttk.Label(frame, text=f"This is Tab {i+1}\n\nTab Navigation:\nCtrl+Tab: Next Tab\nCtrl+Shift+Tab: Previous Tab\nCtrl+W: Close Tab\nCtrl+T: New Tab")
        label.pack(expand=True)
    
    print("✅ Enhanced event handler ready!")
    print("⌨️ Tab navigation keys:")
    print("   Ctrl+Tab: Next tab")
    print("   Ctrl+Shift+Tab: Previous tab") 
    print("   Ctrl+W: Close tab")
    print("   Ctrl+T: New tab")
    
    root.mainloop()
