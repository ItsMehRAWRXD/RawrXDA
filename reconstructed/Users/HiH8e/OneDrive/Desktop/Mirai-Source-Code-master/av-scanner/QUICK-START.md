# 🚀 Quick Start Guide - AV-Sense

## Get Started in 5 Minutes

### Option 1: Automated Setup (Windows)

#### Using PowerShell:
```powershell
cd av-scanner
.\setup.ps1
```

#### Using Command Prompt:
```cmd
cd av-scanner
setup.bat
```

The setup script will:
1. Check Node.js installation
2. Install all dependencies
3. Create environment file
4. Initialize database
5. Start the server

### Option 2: Manual Setup

```bash
# 1. Navigate to project
cd av-scanner

# 2. Install dependencies
npm install

# 3. Copy environment file
cp .env.example .env

# 4. Edit .env and set JWT_SECRET
# Use a text editor to change JWT_SECRET to a random string

# 5. Initialize database
npm run init-db

# 6. Start server
npm start
```

## 🌐 Access the Application

Once started, open your browser to:
```
http://localhost:3000
```

## 👤 Create Your First Account

1. Click "Register" tab
2. Enter username, email, and password
3. Click "Register" button
4. You'll be automatically logged in

## 📤 Scan Your First File

1. Click "Choose File" or drag & drop a file
2. Wait for scan to complete (usually 5-10 seconds)
3. View detailed results from 27 AV engines
4. Download PDF report or share results

## 💰 Add Scans to Your Account

### For Testing (Modify Database Directly):

```bash
# Open SQLite database
sqlite3 database/av_scanner.db

# Add 100 free scans to user ID 1
UPDATE users SET scans_remaining = 100 WHERE id = 1;

# Exit SQLite
.quit
```

Refresh your browser to see updated scan count.

### For Production:

1. Navigate to "Pricing" page
2. Choose a plan
3. Complete payment integration (Stripe/PayPal)
4. Scans added automatically

## 📱 Setup Telegram Bot (Optional)

### 1. Create Bot
1. Open Telegram and search for @BotFather
2. Send `/newbot` command
3. Follow instructions to name your bot
4. Copy the bot token

### 2. Configure Bot
Edit `.env` file:
```env
TELEGRAM_BOT_TOKEN=your-bot-token-here
```

### 3. Restart Server
```bash
npm start
```

### 4. Use Bot
1. Open your bot in Telegram
2. Send `/start`
3. Send `/login username password`
4. Send any file to scan it!

## 🔧 Common Issues

### "Module not found" Error
```bash
npm install
```

### Port Already in Use
Edit `.env` and change:
```env
PORT=3001
```

### Database Locked
Close any other processes using the database, or delete and reinitialize:
```bash
rm database/av_scanner.db
npm run init-db
```

### Scan Not Working
Check server console for errors. Ensure Windows Defender is available on Windows systems.

## 📊 Test the API

### Register User
```bash
curl -X POST http://localhost:3000/api/auth/register \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"testuser\",\"email\":\"test@example.com\",\"password\":\"test123\"}"
```

### Login
```bash
curl -X POST http://localhost:3000/api/auth/login \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"testuser\",\"password\":\"test123\"}"
```

Copy the `token` from response.

### Upload File
```bash
curl -X POST http://localhost:3000/api/scan/upload \
  -H "Authorization: Bearer YOUR_TOKEN_HERE" \
  -F "file=@C:\path\to\your\file.exe"
```

## 🎨 Customize Theme

1. Login to dashboard
2. Click "Settings" in navigation
3. Choose Dark or Light theme
4. Select preferred font
5. Click "Save Settings"

## 📈 View Statistics

1. Navigate to "Statistics" page
2. View:
   - Total scans
   - Detection count
   - Clean files
   - Detection rate
   - 30-day trends

## 🔐 Production Deployment

### Before Going Live:

1. **Change JWT Secret**
   ```env
   JWT_SECRET=use-a-long-random-string-here-min-32-chars
   ```

2. **Set Production Mode**
   ```env
   NODE_ENV=production
   ```

3. **Configure HTTPS**
   - Use nginx reverse proxy
   - Install SSL certificate (Let's Encrypt)

4. **Setup Payment Gateway**
   - Get Stripe or PayPal API keys
   - Add to `.env`
   - Integrate webhook handlers

5. **Configure Domain**
   - Point domain to server
   - Update CORS settings
   - Update Telegram webhook URL

6. **Process Management**
   ```bash
   npm install -g pm2
   pm2 start backend/server.js --name av-sense
   pm2 startup
   pm2 save
   ```

## 🆘 Need Help?

Check these files:
- `README.md` - Full documentation
- `backend/server.js` - Server configuration
- `.env.example` - Environment variables reference
- Server console logs for errors

## 🎯 Next Steps

1. ✅ Set up account
2. ✅ Scan test file
3. ✅ View results and download PDF
4. ✅ Check statistics
5. ✅ Setup Telegram bot (optional)
6. ✅ Configure payment gateway (production)
7. ✅ Deploy to production server

---

**Enjoy private AV scanning with AV-Sense! 🛡️**
