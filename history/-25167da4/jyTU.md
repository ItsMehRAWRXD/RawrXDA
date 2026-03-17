# 🎙️ VOICE-ENABLED ASSISTANT - Complete Guide

## 🚀 System Overview

**Your IDE now has a voice! Complete with:**
- 🗣️ **Speech-to-Text**: Speak commands naturally  
- 🔊 **Text-to-Speech**: Hear all responses
- 📝 **Live Subtitles**: Accessibility for everyone
- 🧠 **1MB Memory**: Learns and remembers your preferences
- 🤖 **Smart Translation**: Natural language → Machine commands
- 🌐 **Web Integration**: Search and browse
- 📚 **Codebase Knowledge**: Full source intelligence

---

## ⚡ Quick Start (3 Modes)

### 1. Interactive with Voice (Recommended)

```powershell
cd "D:\lazy init ide\scripts"
.\voice_assistant.ps1 -Mode interactive -EnableVoice -EnableSubtitles
```

- Type questions OR say "voice" to speak
- Responses are spoken and subtitled
- All interactions remembered

### 2. Voice-Only (Hands-Free)

```powershell
.\voice_assistant.ps1 -Mode voice -EnableSubtitles
```

- 100% hands-free operation
- Just speak, no typing
- Perfect for accessibility

### 3. Text with Memory

```powershell
.\voice_assistant.ps1 -Mode interactive
```

- Traditional typing
- Still learns preferences
- 1MB context memory active

---

## 🗣️ Voice Command Examples

**Just speak naturally:**
- "send five agents to D test folder"
- "create a seven B model for coding"
- "add a todo to fix the parser"
- "search the web for async patterns"
- "what files are in my project"

**System understands and translates!**

---

## 🧠 Context Memory (1MB Capacity)

### What It Learns Automatically

**Model Preferences:**
```
You: create a 7B model [3 times]
Memory: "User prefers 7B models"
```

**Swarm Patterns:**
```
You: send 5 agents [4 times]
Memory: "Typical swarm size is 5"
```

**Directories:**
```
You: deploy to D:\projects [5 times]
Memory: "Frequent directory logged"
```

### Check Memory Stats

```powershell
.\context_memory.ps1 -Operation stats
```

Shows usage, patterns learned, and your preferences!

### Get Smart Suggestions

```powershell
.\context_memory.ps1 -Operation tune
```

**Example output:**
```
🎯 Based on your patterns:
  🧠 Favorite Models: 7B, 13B
  🤖 Avg Swarm Size: 5 agents  
  📁 Top Directory: D:\projects\website
```

### Search History

```powershell
.\context_memory.ps1 -Operation search -Context "model"
```

Finds all past model-related conversations!

---

## 🔊 Text-to-Speech

### Speak Any Text

```powershell
.\speech_interface.ps1 -Operation speak -Text "Your message here"
```

### With Subtitles

```powershell
.\speech_interface.ps1 -Operation speak -Text "Hello world" -ShowSubtitles
```

### Different Voices

```powershell
# List voices
.\speech_interface.ps1 -Operation voices

# Use specific voice
.\speech_interface.ps1 -Operation speak -Text "Hello" -Voice "Microsoft Zira Desktop"
```

### Control Rate and Volume

```powershell
# Faster (Rate: -10 to 10)
.\speech_interface.ps1 -Operation speak -Text "Quick" -Rate 5

# Louder (Volume: 0 to 100)
.\speech_interface.ps1 -Operation speak -Text "Loud" -Volume 100
```

---

## 🎤 Speech-to-Text

### Listen for Commands

```powershell
.\speech_interface.ps1 -Operation listen
```

**Recognizes:**
- "send agents to..."
- "create a model"
- "add a todo"
- "search the web"
- "monitor swarm"
- "help"

### View Subtitle History

```powershell
.\speech_interface.ps1 -Operation subtitle
```

Shows last 20 spoken/heard messages!

---

## 📝 Accessibility Features

### Live Subtitles

**Auto-generated for all speech:**
- Real-time display
- Word-by-word highlighting
- Saved with timestamps
- Review anytime

### Subtitle File

Location: `D:\lazy init ide\data\current_subtitles.txt`

**View recent:**
```powershell
Get-Content "D:\lazy init ide\data\current_subtitles.txt" -Tail 20
```

---

## 🎬 Real Workflows

### Hands-Free Development

```
1. Start: .\voice_assistant.ps1 -Mode voice -EnableSubtitles

2. You: (speak) "send five agents to my project folder"
   AI: (speaks) "Command translated. Swarm control script..."
   [Subtitles show command]

3. You: (speak) "what is the status"
   AI: (speaks) "Checking swarm status..."
   [Shows and reads status]

4. You: (speak) "exit"
   AI: (speaks) "Goodbye!"
```

### Smart Learning Session

```
Day 1: "create a 7B model"
Day 2: "create a 7B model"  
Day 3: "create a 7B model"

Day 4: .\context_memory.ps1 -Operation tune
Output: "Favorite model: 7B (used 3 times)"

Next time: System suggests 7B by default!
```

### Voice Research

```
You: (speak) "search the web for PowerShell arrays"
AI: (speaks) "Found 5 results. First, PowerShell documentation..."
[Shows links with subtitles]

You: (speak) "open the first link"
AI: (speaks) "Opening browser"
[Browser launches]
```

---

## 💡 Pro Tips

### 1. Use Clear Speech

✅ "send five agents to D drive test folder"
❌ "uhh maybe send some agents somewhere"

### 2. Enable Subtitles for Accuracy

Always use `-EnableSubtitles` to verify recognition!

### 3. Check Memory Regularly

```powershell
.\context_memory.ps1 -Operation stats
```

See what the AI has learned about you.

### 4. Combine Features

```powershell
# Voice + Subtitles + Memory
.\voice_assistant.ps1 -Mode interactive -EnableVoice -EnableSubtitles
```

Best of all worlds!

### 5. Export Memory for Backup

```powershell
.\context_memory.ps1 -Operation export
```

Creates readable backup of all learned patterns.

---

## 🔧 Customization

### Change Default Voice

```powershell
.\voice_assistant.ps1 -Mode interactive -EnableVoice -VoiceName "Microsoft Zira Desktop"
```

### Adjust Memory Limit

Edit `context_memory.ps1`:
```powershell
$this.MaxSizeBytes = 2MB  # Increase to 2MB
```

### Add Custom Voice Commands

Edit `speech_interface.ps1` in `BuildCommandGrammar()`:
```powershell
$commands = @(
    "your custom command",
    # ... existing commands
)
```

---

## 📊 Memory Management

### When Memory Fills Up

At 1MB capacity, auto-compression happens:
- Keeps newest 70% of conversations
- Compresses oldest 30% into summary
- Preserves all learned patterns
- No data loss!

### Manual Compression

```powershell
.\context_memory.ps1 -Operation compress
```

### Clear Everything

```powershell
.\context_memory.ps1 -Operation clear
```

⚠️ Warning: Erases all learned preferences!

---

## 🐛 Troubleshooting

### "Speech recognition not available"

**Solution:** Install Windows Speech Recognition
1. Settings → Time & Language → Speech
2. Enable "Online speech recognition"
3. Restart PowerShell

### "Voice not found"

**Solution:** List available voices
```powershell
.\speech_interface.ps1 -Operation voices
```

Use exact name shown.

### "No speech detected"

**Solutions:**
1. Check microphone settings
2. Speak louder and clearer
3. Reduce background noise
4. Test: `.\speech_interface.ps1 -Operation test`

### "Memory full"

**Solution:** Compress or clear
```powershell
.\context_memory.ps1 -Operation compress
```

---

## ✅ Feature Checklist

- [ ] Voice input working
- [ ] Voice output working
- [ ] Subtitles displaying
- [ ] Context memory saving
- [ ] Learning preferences
- [ ] Web search integrated
- [ ] Command translation active
- [ ] Knowledge base loaded

**Test all:**
```powershell
.\voice_assistant.ps1 -Mode interactive -EnableVoice -EnableSubtitles
```

---

## 📞 Quick Reference

```powershell
# Voice assistant (full features)
.\voice_assistant.ps1 -Mode interactive -EnableVoice -EnableSubtitles

# Voice-only mode
.\voice_assistant.ps1 -Mode voice -EnableSubtitles

# Memory stats
.\context_memory.ps1 -Operation stats

# Memory suggestions
.\context_memory.ps1 -Operation tune

# Speak text
.\speech_interface.ps1 -Operation speak -Text "Hello"

# Listen for voice
.\speech_interface.ps1 -Operation listen

# View subtitles
.\speech_interface.ps1 -Operation subtitle

# List voices
.\speech_interface.ps1 -Operation voices

# Test everything
.\speech_interface.ps1 -Operation test
```

---

## 🎉 You're Ready!

**Your IDE can now:**
- 🗣️ Understand your voice
- 🔊 Speak back to you
- 📝 Show subtitles for accessibility
- 🧠 Remember your preferences (1MB memory!)
- 🤖 Translate natural language to commands
- 🌐 Search the web on your behalf
- 📚 Know your entire codebase

**Just say what you need, and your AI companion handles the rest! 🚀**

---

**Pro Tip:** Start with:
```powershell
.\voice_assistant.ps1 -Mode voice -EnableSubtitles
```

Lean back, speak naturally, and watch the magic happen! ✨
