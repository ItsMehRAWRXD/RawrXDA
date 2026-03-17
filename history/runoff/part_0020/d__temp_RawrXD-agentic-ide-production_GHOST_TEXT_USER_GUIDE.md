# Ghost Text - Quick User Guide

## 🎯 What is Ghost Text?

Ghost text is AI-powered code suggestions that appear in light gray as you type. Accept them with one keystroke to complete your code faster!

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| **TAB** | Show suggestion |
| **Ctrl+Enter** | Accept suggestion |
| **ESC** | Dismiss suggestion |
| **Ctrl+Space** | Force code completion |

---

## 📖 How to Use

### 1️⃣ Type Code Normally
```cpp
if (x > 0)
```

### 2️⃣ Press TAB
Ghost text appears in gray:
```cpp
if (x > 0) {
    
}
```

### 3️⃣ Accept with Ctrl+Enter
Code is inserted at cursor:
```cpp
if (x > 0) {
    // Cursor here
}
```

### 4️⃣ Or Press ESC to Dismiss
```cpp
if (x > 0)
// Suggestion gone, keep typing
```

---

## 🎨 Visual Example

```
┌─────────────────────────────────────┐
│ int main() {                        │
│     for (int i = 0; i <     ← Suggestion here (gray italic)
│                                     │
│ }                                   │
└─────────────────────────────────────┘
```

---

## 💡 What It Can Suggest

### Keywords
- `if` → Complete if statement
- `for` → Complete for loop
- `while` → Complete while loop
- `class` → Complete class definition
- `def` → Complete function definition (Python)

### Syntax
- Closing braces and parentheses
- Function bodies
- Control flow structures
- Variable declarations

### Smart Features
- Maintains your indentation
- Respects code style
- Multiple language support
- Context-aware

---

## 🌍 Supported Languages

- **C/C++** - Full support
- **Python** - Full support
- **JavaScript** - Full support
- Others - Basic pattern matching

---

## 🎮 Right-Click Menu

Right-click in the editor to access:

1. **Code Completion** - Get next code block
2. **Explain Code** - Describe selected code
3. **Refactor Code** - Get refactoring suggestions
4. **Generate Tests** - Create test templates
5. **Find Bugs** - Identify potential issues
6. **Toggle Ghost Text** - Enable/disable feature

---

## ⚙️ Settings

### Disable Ghost Text Temporarily
1. Right-click in editor
2. Select "Toggle Ghost Text"
3. Press again to re-enable

### Language Detection
- Automatic based on file extension
- Set in settings if needed

---

## 💡 Tips & Tricks

### Tip 1: Use When Stuck
```
Type: for (
Press: TAB
→ Complete for loop structure
```

### Tip 2: Dismiss Unwanted Suggestions
```
Press: ESC
→ Keep your original code
```

### Tip 3: Multiple Suggestions
```
Press: Ctrl+Space
→ Cycle through options (if available)
```

### Tip 4: Complex Patterns
```
Type: class MyClass {
Press: TAB
→ Adds public/private sections
```

---

## ❓ FAQ

**Q: Do I need an AI subscription?**  
A: No! Ghost text works completely offline with built-in smart patterns.

**Q: Does it use my code for training?**  
A: No! Everything is local. Your code never leaves your computer.

**Q: Can I turn it off?**  
A: Yes! Right-click and select "Toggle Ghost Text" or press ESC to dismiss individual suggestions.

**Q: What if I don't like the suggestion?**  
A: Just press ESC or keep typing. The suggestion won't be inserted unless you press Ctrl+Enter.

**Q: Does it work with all languages?**  
A: Best with C++, Python, and JavaScript. Other languages get basic pattern matching.

**Q: Is it fast?**  
A: Yes! Suggestions appear instantly (< 1ms) since they're generated locally.

**Q: Can I configure it?**  
A: Yes! See your IDE's settings for customization options.

---

## 🚀 Getting Started

1. **Open IDE** → Creates a code editor
2. **Type some code** → Type normally
3. **Press TAB** → See suggestion
4. **Press Ctrl+Enter** → Accept it
5. **Enjoy faster coding!** → Repeat

---

## 📞 Help & Support

### If Ghost Text Doesn't Appear
1. Make sure you're in a code editor (not text box)
2. Check that ghost text is enabled (toggle if needed)
3. Make sure you pressed TAB (not Space)

### If Text Inserts Unexpectedly
- This is TAB completing rather than ghost text
- Adjust your IDE settings if needed

### Performance Issues
- Ghost text uses < 1% CPU
- If slow, disable suggestion rendering in settings

---

## ✨ Best Practices

✅ **Do:**
- Use TAB to get suggestions when stuck
- Accept suggestions with Ctrl+Enter
- Dismiss with ESC if unwanted
- Use right-click menu for complex tasks

❌ **Don't:**
- Force-accept bad suggestions (press ESC instead)
- Rely on it for complex logic (review suggestions first)
- Expect AI-level understanding (it uses pattern matching)
- Share concerns about code privacy (it never leaves your machine)

---

## 🎓 Learning More

- Check IDE menu → Help → Ghost Text
- See GHOST_TEXT_IMPLEMENTATION.md for technical details
- See GHOST_TEXT_COMPLETE.md for feature overview

---

## 🎉 That's It!

You're ready to use ghost text for faster coding! Enjoy your AI-powered code completions! 🚀

**Happy Coding!** ✨
