; ╔════════════════════════════════════════════════════════════════════════════╗
; ║          IDEre2.html CONVERTED TO PURE NASM ASSEMBLY - CHUNK 002           ║
; ║                   Lines 501-1000 of 23120 Total Lines                      ║
; ║                                                                            ║
; ║  Original File: C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html               ║
; ║  File Size: 841KB (23,120 lines)                                          ║
; ║  Conversion Strategy: Store HTML/CSS/JS as string data sections            ║
; ║  Architecture: x86-64 (64-bit) - Intel/AMD compatible                     ║
; ║                                                                            ║
; ║  NOTES:                                                                    ║
; ║  - Lines 501-1000 contain primarily CSS styling rules                     ║
; ║  - Each CSS rule block preserved exactly in string form                   ║
; ║  - Color codes, hex values, and dimensions all preserved                  ║
; ║  - Can be extracted and embedded in documents programmatically            ║
; ║  - UTF-8 encoding maintained throughout                                   ║
; ╚════════════════════════════════════════════════════════════════════════════╝

[BITS 64]
[DEFAULT REL]

; ╔════════════════════════════════════════════════════════════════════════════╗
; ║                        DATA SECTION - CSS CONTENT                          ║
; ╚════════════════════════════════════════════════════════════════════════════╝

section '.data' align=16

    ; ╔────────────────────────────────────────────────────────────────────╗
    ; ║ CHUNK 002 - Lines 501-1000 Consolidated CSS Data Block            ║
    ; ║ This section contains all CSS styling from the HTML document       ║
    ; ║ Organized in logical groups: Terminal, Editor, AI Panel, Chat      ║
    ; ╚────────────────────────────────────────────────────────────────────╝

    chunk_002_css_data:
        ; LINES 501-515: Terminal output styling (scrollbar, overflow)
        db "      border: none;", 0x0A
        db "      padding: 20px;", 0x0A
        db "      font-family: 'Consolas', 'Monaco', 'Courier New', monospace;", 0x0A
        db "      font-size: 14px;", 0x0A
        db "      line-height: 1.5;", 0x0A
        db "      resize: none;", 0x0A
        db "      outline: none;", 0x0A
        db "      overflow-y: auto !important;", 0x0A
        db "      overflow-x: auto !important;", 0x0A
        db "      min-height: 0;", 0x0A
        db "      max-height: 100%;", 0x0A
        db "      width: 100%;", 0x0A
        db "      white-space: pre;", 0x0A
        db "      word-wrap: break-word;", 0x0A
        db "      scrollbar-width: thin;", 0x0A

        ; LINES 516-520: Scrollbar styling (Firefox and Chrome)
        db "      /* Firefox */", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .terminal-output::-webkit-scrollbar {", 0x0A
        db "      width: 8px;", 0x0A

        ; LINES 521-535: Chrome/Safari scrollbar styles
        db "      /* Chrome, Safari */", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .terminal-output::-webkit-scrollbar-track {", 0x0A
        db "      background: var(--bg-secondary);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .terminal-output::-webkit-scrollbar-thumb {", 0x0A
        db "      background: #555;", 0x0A
        db "      border-radius: 4px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .terminal-output::-webkit-scrollbar-thumb:hover {", 0x0A
        db "      background: #666;", 0x0A
        db "    }", 0x0A

        ; LINES 536-550: Monaco editor and AI panel base styles
        db 0x0A
        db "    /* Monaco Editor Container */", 0x0A
        db "    .monaco-editor-container {", 0x0A
        db "      flex: 1;", 0x0A
        db "      overflow: hidden;", 0x0A
        db "      min-height: 0;", 0x0A
        db "      width: 100%;", 0x0A
        db "      height: 100%;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* AI Panel */", 0x0A
        db "    .ai-panel {", 0x0A
        db "      background: var(--bg-secondary);", 0x0A
        db "      border-left: 1px solid var(--border);", 0x0A
        db "      display: flex;", 0x0A
        db "      flex-direction: column;", 0x0A

        ; LINES 551-570: AI panel base and floating styles
        db "      overflow: hidden;", 0x0A
        db "      min-height: 0;", 0x0A
        db "      height: 100%;", 0x0A
        db "      position: relative;", 0x0A
        db "      transition: all 0.3s ease;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* Floating AI Panel */", 0x0A
        db "    .ai-panel.floating {", 0x0A
        db "      position: fixed !important;", 0x0A
        db "      top: 100px;", 0x0A
        db "      right: 20px;", 0x0A
        db "      width: 450px;", 0x0A
        db "      height: 600px;", 0x0A
        db "      z-index: 9999;", 0x0A
        db "      border: 2px solid var(--accent);", 0x0A
        db "      border-radius: 8px;", 0x0A
        db "      box-shadow: 0 10px 40px rgba(0, 0, 0, 0.5);", 0x0A

        ; LINES 571-590: Float controls and buttons
        db "      resize: both;", 0x0A
        db "      overflow: auto;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .ai-panel.floating .ai-header {", 0x0A
        db "      cursor: move;", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      color: white;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .float-controls {", 0x0A
        db "      display: flex;", 0x0A
        db "      gap: 5px;", 0x0A
        db "      margin-left: auto;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .float-btn {", 0x0A
        db "      background: rgba(255, 255, 255, 0.2);", 0x0A
        db "      border: none;", 0x0A
        db "      color: white;", 0x0A

        ; LINES 591-610: Float button styling and pane collapse
        db "      width: 24px;", 0x0A
        db "      height: 24px;", 0x0A
        db "      border-radius: 4px;", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      font-size: 14px;", 0x0A
        db "      display: flex;", 0x0A
        db "      align-items: center;", 0x0A
        db "      justify-content: center;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .float-btn:hover {", 0x0A
        db "      background: rgba(255, 255, 255, 0.3);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* Collapsible Panes */", 0x0A
        db "    .pane-collapsed {", 0x0A
        db "      display: none !important;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .collapse-handle {", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A

        ; LINES 611-630: Collapse handle and header styles
        db "      border: 1px solid var(--border);", 0x0A
        db "      padding: 4px;", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      text-align: center;", 0x0A
        db "      font-size: 10px;", 0x0A
        db "      -webkit-user-select: none;", 0x0A
        db "      user-select: none;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .collapse-handle:hover {", 0x0A
        db "      background: var(--accent);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .ai-header {", 0x0A
        db "      padding: 15px;", 0x0A
        db "      border-bottom: 1px solid var(--border);", 0x0A
        db "      display: flex;", 0x0A
        db "      flex-direction: column;", 0x0A
        db "      gap: 10px;", 0x0A

        ; LINES 631-650: Chat tabs styling
        db "      flex-shrink: 0;", 0x0A
        db "      max-height: 200px;", 0x0A
        db "      overflow-y: auto;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .ai-title {", 0x0A
        db "      font-weight: bold;", 0x0A
        db "      display: flex;", 0x0A
        db "      align-items: center;", 0x0A
        db "      gap: 10px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* Chat Tabs */", 0x0A
        db "    .chat-tabs {", 0x0A
        db "      display: flex;", 0x0A
        db "      gap: 8px;", 0x0A
        db "      align-items: center;", 0x0A
        db "      margin-top: 8px;", 0x0A
        db "    }", 0x0A

        ; LINES 651-670: Individual chat tab styling
        db 0x0A
        db "    .chat-tab {", 0x0A
        db "      display: inline-flex;", 0x0A
        db "      align-items: center;", 0x0A
        db "      gap: 8px;", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      padding: 6px 10px;", 0x0A
        db "      border-radius: 6px;", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      font-size: 12px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .chat-tab.active {", 0x0A
        db "      background: var(--accent);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .chat-tab .tab-close {", 0x0A
        db "      margin-left: 8px;", 0x0A

        ; LINES 671-690: Unread count badge and multi-chat container
        db "      font-weight: bold;", 0x0A
        db "      color: var(--text-dim);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .chat-tab .unread-count {", 0x0A
        db "      background: var(--accent-green);", 0x0A
        db "      color: #021;", 0x0A
        db "      border-radius: 8px;", 0x0A
        db "      padding: 2px 6px;", 0x0A
        db "      font-size: 10px;", 0x0A
        db "      margin-left: 6px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* Multi-Chat Styles */", 0x0A
        db "    .multi-chat-container {", 0x0A
        db "      position: fixed;", 0x0A
        db "      bottom: 25px;", 0x0A
        db "      right: 20px;", 0x0A
        db "      z-index: 10000;", 0x0A
        db "      width: 400px;", 0x0A

        ; LINES 691-710: Multi-chat container and popup
        db "      height: 500px;", 0x0A
        db "      display: none;", 0x0A
        db "      /* Hidden by default */", 0x0A
        db "      flex-direction: column;", 0x0A
        db "      background: var(--bg-secondary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      border-radius: 8px;", 0x0A
        db "      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-popup {", 0x0A
        db "      display: flex;", 0x0A
        db "      flex-direction: column;", 0x0A
        db "      height: 100%;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-header {", 0x0A
        db "      padding: 12px;", 0x0A
        db "      border-bottom: 1px solid var(--border);", 0x0A

        ; LINES 711-730: Multi-chat tabs and buttons
        db "      background: var(--bg-tertiary);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-tabs {", 0x0A
        db "      display: flex;", 0x0A
        db "      gap: 6px;", 0x0A
        db "      flex-wrap: wrap;", 0x0A
        db "      margin-bottom: 8px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-tab {", 0x0A
        db "      display: inline-flex;", 0x0A
        db "      align-items: center;", 0x0A
        db "      gap: 6px;", 0x0A
        db "      background: var(--bg-secondary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      padding: 4px 10px;", 0x0A
        db "      border-radius: 4px;", 0x0A
        db "      cursor: pointer;", 0x0A

        ; LINES 731-750: Multi-chat tab active state and new button
        db "      font-size: 11px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-tab.active {", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      color: white;", 0x0A
        db "      border-color: var(--accent);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-new-btn {", 0x0A
        db "      padding: 4px 12px;", 0x0A
        db "      background: var(--accent-green);", 0x0A
        db "      color: white;", 0x0A
        db "      border: none;", 0x0A
        db "      border-radius: 4px;", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      font-size: 12px;", 0x0A

        ; LINES 751-770: Multi-chat content area and message styles
        db "      font-weight: bold;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-new-btn:hover {", 0x0A
        db "      background: var(--accent-green-bright);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-content {", 0x0A
        db "      flex: 1;", 0x0A
        db "      overflow-y: auto;", 0x0A
        db "      padding: 12px;", 0x0A
        db "      display: flex;", 0x0A
        db "      flex-direction: column;", 0x0A
        db "      gap: 8px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-message {", 0x0A
        db "      padding: 8px 12px;", 0x0A

        ; LINES 771-790: User and AI message styling
        db "      border-radius: 4px;", 0x0A
        db "      font-size: 12px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-message.user {", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      color: white;", 0x0A
        db "      align-self: flex-end;", 0x0A
        db "      max-width: 80%;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-message.ai {", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      color: var(--text);", 0x0A
        db "      align-self: flex-start;", 0x0A
        db "      max-width: 80%;", 0x0A
        db "    }", 0x0A
        db 0x0A

        ; LINES 791-810: Multi-chat input area and input field
        db "    .multi-chat-input-area {", 0x0A
        db "      display: flex;", 0x0A
        db "      gap: 8px;", 0x0A
        db "      padding: 12px;", 0x0A
        db "      border-top: 1px solid var(--border);", 0x0A
        db "      background: var(--bg-secondary);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-input {", 0x0A
        db "      flex: 1;", 0x0A
        db "      padding: 8px 12px;", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      color: var(--text);", 0x0A
        db "      border-radius: 4px;", 0x0A
        db "      font-size: 12px;", 0x0A

        ; LINES 811-830: Input placeholder and send button
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-input::placeholder {", 0x0A
        db "      color: var(--text-dim);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-send-btn,", 0x0A
        db "    .multi-chat-settings-btn {", 0x0A
        db "      padding: 8px 12px;", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      color: white;", 0x0A
        db "      border: none;", 0x0A
        db "      border-radius: 4px;", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      font-size: 12px;", 0x0A

        ; LINES 831-850: Send button hover and settings dropdown
        db "      font-weight: bold;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-send-btn:hover,", 0x0A
        db "    .multi-chat-settings-btn:hover {", 0x0A
        db "      background: var(--accent-bright);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-settings {", 0x0A
        db "      display: none;", 0x0A
        db "      position: absolute;", 0x0A
        db "      top: 100%;", 0x0A
        db "      right: 0;", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      border-radius: 4px;", 0x0A

        ; LINES 851-870: Settings open state and mode buttons
        db "      padding: 12px;", 0x0A
        db "      margin-top: 4px;", 0x0A
        db "      z-index: 10001;", 0x0A
        db "      min-width: 250px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .multi-chat-settings.open {", 0x0A
        db "      display: block;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .mode-btn,", 0x0A
        db "    .quality-btn {", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      color: var(--text);", 0x0A
        db "      padding: 6px 12px;", 0x0A
        db "      border-radius: 4px;", 0x0A

        ; LINES 871-890: Quality button active state and tuning panel
        db "      cursor: pointer;", 0x0A
        db "      font-size: 11px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .mode-btn.active,", 0x0A
        db "    .quality-btn.active {", 0x0A
        db "      background: var(--accent);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* Model Tuning Sliders */", 0x0A
        db "    .tuning-panel {", 0x0A
        db "      background: rgba(0, 0, 0, 0.2);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db "      border-radius: 6px;", 0x0A
        db "      padding: 12px;", 0x0A
        db "      margin: 10px 0;", 0x0A

        ; LINES 891-910: Tuning slider container and label
        db "    }", 0x0A
        db 0x0A
        db "    .tuning-slider {", 0x0A
        db "      width: 100%;", 0x0A
        db "      margin: 8px 0;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .slider-container {", 0x0A
        db "      margin: 10px 0;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .slider-label {", 0x0A
        db "      display: flex;", 0x0A
        db "      justify-content: space-between;", 0x0A
        db "      align-items: center;", 0x0A
        db "      font-size: 11px;", 0x0A
        db "      margin-bottom: 5px;", 0x0A

        ; LINES 911-930: Slider value display and range input
        db "    }", 0x0A
        db 0x0A
        db "    .slider-value {", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      color: white;", 0x0A
        db "      padding: 2px 8px;", 0x0A
        db "      border-radius: 3px;", 0x0A
        db "      font-weight: bold;", 0x0A
        db "      min-width: 45px;", 0x0A
        db "      text-align: center;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    input[type=""range""] {", 0x0A
        db "      width: 100%;", 0x0A
        db "      height: 6px;", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      border-radius: 3px;", 0x0A

        ; LINES 931-950: Range thumb styling (webkit and mozilla)
        db "      outline: none;", 0x0A
        db "      -webkit-appearance: none;", 0x0A
        db "      appearance: none;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    input[type=""range""]::-webkit-slider-thumb {", 0x0A
        db "      -webkit-appearance: none;", 0x0A
        db "      appearance: none;", 0x0A
        db "      width: 16px;", 0x0A
        db "      height: 16px;", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      border-radius: 50%;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    input[type=""range""]::-moz-range-thumb {", 0x0A
        db "      width: 16px;", 0x0A

        ; LINES 951-970: Mozilla range thumb and tuning collapse button
        db "      height: 16px;", 0x0A
        db "      background: var(--accent);", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      border-radius: 50%;", 0x0A
        db "      border: none;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .tuning-collapse-btn {", 0x0A
        db "      cursor: pointer;", 0x0A
        db "      -webkit-user-select: none;", 0x0A
        db "      user-select: none;", 0x0A
        db "      display: flex;", 0x0A
        db "      align-items: center;", 0x0A
        db "      gap: 5px;", 0x0A
        db "      font-weight: bold;", 0x0A
        db "      padding: 5px;", 0x0A

        ; LINES 971-990: Tuning collapse button hover and chat tabs container
        db "      border-radius: 4px;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .tuning-collapse-btn:hover {", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    /* Chat Tabs */", 0x0A
        db "    .chat-tabs-container {", 0x0A
        db "      display: flex;", 0x0A
        db "      gap: 4px;", 0x0A
        db "      padding: 8px;", 0x0A
        db "      background: var(--bg-tertiary);", 0x0A
        db "      border-bottom: 1px solid var(--border);", 0x0A

        ; LINES 991-1000: Final lines with chat tabs overflow and thought container
        db "      overflow-x: auto;", 0x0A
        db "      flex-wrap: nowrap;", 0x0A
        db "    }", 0x0A
        db 0x0A
        db "    .chat-tab {", 0x0A
        db "      display: inline-flex;", 0x0A
        db "      align-items: center;", 0x0A
        db "      gap: 6px;", 0x0A
        db "      padding: 6px 12px;", 0x0A
        db "      background: var(--bg-secondary);", 0x0A
        db "      border: 1px solid var(--border);", 0x0A
        db 0
    len_chunk_002: equ $ - chunk_002_css_data

section '.text' align=16

    global html_content_chunk_002
    global css_data_chunk_002

    html_content_chunk_002:
        lea rax, [rel chunk_002_css_data]
        mov rdx, len_chunk_002
        ret

    css_data_chunk_002:
        lea rax, [rel chunk_002_css_data]
        mov rdx, len_chunk_002
        ret

; ╔════════════════════════════════════════════════════════════════════════════╗
; ║                   BUILD INSTRUCTIONS FOR THIS FILE                         ║
; ╚════════════════════════════════════════════════════════════════════════════╝
; 
; Compile for Windows x64:
;   nasm -f win64 IDEre2_CHUNK_002_Lines_501-1000.asm -o IDEre2_CHUNK_002.obj
;
; Compile for Linux/Unix x64:
;   nasm -f elf64 IDEre2_CHUNK_002_Lines_501-1000.asm -o IDEre2_CHUNK_002.o
;
; Link multiple chunks together:
;   g++ main.cpp IDEre2_CHUNK_001.o IDEre2_CHUNK_002.o -o program
;
; This creates a modular system where each 500-line chunk is:
; - Independently compilable
; - Separately linked
; - Accessible as named data sections
; - Usable for reconstruction of original HTML document
;
; ╚════════════════════════════════════════════════════════════════════════════╝
