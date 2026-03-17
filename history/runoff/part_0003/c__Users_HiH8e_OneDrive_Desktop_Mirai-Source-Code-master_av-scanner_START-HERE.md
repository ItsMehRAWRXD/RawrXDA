# 📚 AV-Sense Documentation Index

Welcome to the AV-Sense Private AV Scanning Service documentation!

## 🚀 Getting Started (Start Here!)

### New Users
1. **[QUICK-START.md](QUICK-START.md)** - Get up and running in 5 minutes
2. **[PROJECT-SUMMARY.md](PROJECT-SUMMARY.md)** - Overview of what's included
3. **Run setup**: `setup.ps1` or `setup.bat`
4. **Start server**: `npm start`
5. **Open**: http://localhost:3000

### Quick Commands
```bash
# Automated setup (Windows)
.\setup.ps1

# Manual setup
npm install
cp .env.example .env
npm run init-db
npm start
```

## 📖 Complete Documentation

### Core Documentation
- **[README.md](README.md)** - Complete documentation with API reference
- **[QUICK-START.md](QUICK-START.md)** - 5-minute setup guide
- **[PROJECT-SUMMARY.md](PROJECT-SUMMARY.md)** - What's been built
- **[FEATURES.md](FEATURES.md)** - Complete features list (150+)
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture diagrams
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues & solutions
- **[LICENSE](LICENSE)** - MIT License and disclaimers

### Setup Files
- **`setup.ps1`** - PowerShell automated setup script
- **`setup.bat`** - Windows batch automated setup script  
- **`start.bat`** - Quick start script
- **`.env.example`** - Environment configuration template

## 🎯 What is AV-Sense?

AV-Sense is a **complete, production-ready private antivirus scanning service** that allows you to scan files with 27+ popular AV engines **without** sharing your files with public services or AV vendors.

### Key Features
✅ Private scanning - files stay private  
✅ 27 AV engines including Defender, Kaspersky, McAfee  
✅ Beautiful dark/light theme dashboard  
✅ PDF report generation  
✅ Telegram bot integration  
✅ Full payment & billing system  
✅ Statistics & analytics  
✅ Complete API

## 📁 Project Structure

```
av-scanner/
├── 📄 Documentation
│   ├── README.md              # Main documentation
│   ├── QUICK-START.md         # Quick start guide
│   ├── PROJECT-SUMMARY.md     # Project overview
│   ├── FEATURES.md            # Features list
│   ├── ARCHITECTURE.md        # Architecture diagrams
│   ├── TROUBLESHOOTING.md     # Troubleshooting guide
│   └── THIS-FILE.md           # Documentation index
│
├── 🔧 Setup Scripts
│   ├── setup.ps1              # PowerShell setup
│   ├── setup.bat              # Batch setup
│   ├── start.bat              # Quick start
│   ├── .env.example           # Config template
│   └── package.json           # Dependencies
│
├── 💻 Backend
│   ├── server.js              # Express server
│   ├── scanner-engine.js      # AV scanning engine
│   ├── pdf-generator.js       # PDF reports
│   ├── telegram-bot.js        # Telegram integration
│   └── routes/                # API endpoints
│       ├── auth.js            # Authentication
│       ├── scan.js            # File scanning
│       ├── stats.js           # Statistics
│       ├── payment.js         # Billing
│       └── pdf.js             # PDF generation
│
├── 🗄️ Database
│   ├── init.js                # Schema setup
│   └── db.js                  # Database operations
│
├── 🎨 Frontend
│   ├── index.html             # Dashboard UI
│   ├── styles.css             # Styling (dark/light)
│   └── app.js                 # Frontend logic
│
└── 📦 Data Directories
    ├── uploads/               # File uploads
    ├── database/              # SQLite database
    └── backend/reports/       # PDF reports
```

## 🎓 Documentation by Topic

### Installation & Setup
- [QUICK-START.md](QUICK-START.md) - Step-by-step installation
- [README.md#installation](README.md#-installation) - Detailed setup
- [TROUBLESHOOTING.md#installation-issues](TROUBLESHOOTING.md#installation-issues) - Setup problems

### Usage
- [QUICK-START.md#create-your-first-account](QUICK-START.md#-create-your-first-account) - Getting started
- [README.md#api-documentation](README.md#-api-documentation) - API usage
- [FEATURES.md](FEATURES.md) - What you can do

### Configuration
- [README.md#step-2-configure-environment](README.md#step-2-configure-environment) - Environment setup
- `.env.example` - All config options
- [TROUBLESHOOTING.md#environment-variables-checklist](TROUBLESHOOTING.md#environment-variables-checklist) - Config checklist

### Features
- [FEATURES.md](FEATURES.md) - Complete features (150+)
- [PROJECT-SUMMARY.md#-all-requested-features-implemented](PROJECT-SUMMARY.md#-all-requested-features-implemented) - Features overview
- [README.md#-features](README.md#-features) - Core features

### Architecture
- [ARCHITECTURE.md](ARCHITECTURE.md) - System design
- [README.md#-architecture](README.md#-architecture) - Tech stack
- [PROJECT-SUMMARY.md#-technology-stack](PROJECT-SUMMARY.md#-technology-stack) - Technologies used

### API Reference
- [README.md#-api-documentation](README.md#-api-documentation) - Complete API docs
- [README.md#endpoints](README.md#endpoints) - All endpoints
- [PROJECT-SUMMARY.md#-api-endpoints](PROJECT-SUMMARY.md#-api-endpoints) - Endpoint list

### Telegram Bot
- [README.md#-telegram-bot-setup](README.md#-telegram-bot-setup) - Setup guide
- [QUICK-START.md#-setup-telegram-bot-optional](QUICK-START.md#-setup-telegram-bot-optional) - Quick setup
- [PROJECT-SUMMARY.md#-telegram-bot-commands](PROJECT-SUMMARY.md#-telegram-bot-commands) - Commands

### Troubleshooting
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - All common issues
- [QUICK-START.md#-common-issues](QUICK-START.md#-common-issues) - Quick fixes
- [README.md#-support](README.md#-support) - Getting help

### Deployment
- [README.md#-production-deployment](README.md#-production-deployment) - Production setup
- [QUICK-START.md#-production-deployment](QUICK-START.md#-production-deployment) - Deploy checklist
- [TROUBLESHOOTING.md#production-deployment-issues](TROUBLESHOOTING.md#production-deployment-issues) - Deploy issues

## 🎯 Common Tasks

### I want to...

**Get started quickly**
→ [QUICK-START.md](QUICK-START.md)

**Understand what I'm installing**  
→ [PROJECT-SUMMARY.md](PROJECT-SUMMARY.md)

**See all features**  
→ [FEATURES.md](FEATURES.md)

**Learn the API**  
→ [README.md#-api-documentation](README.md#-api-documentation)

**Fix an error**  
→ [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

**Deploy to production**  
→ [README.md#-production-deployment](README.md#-production-deployment)

**Setup Telegram bot**  
→ [README.md#-telegram-bot-setup](README.md#-telegram-bot-setup)

**Customize the app**  
→ [README.md#-customization](README.md#-customization)

**Understand the architecture**  
→ [ARCHITECTURE.md](ARCHITECTURE.md)

## 💡 Quick Reference

### Essential Commands
```bash
# Setup
npm install              # Install dependencies
npm run init-db         # Initialize database
npm start               # Start server
npm run dev             # Start with auto-reload

# Testing
curl http://localhost:3000/api/health  # Check server

# Database
sqlite3 database/av_scanner.db         # Open database
```

### Important Files
```
.env                    # Your configuration (create from .env.example)
database/av_scanner.db  # SQLite database
package.json           # Dependencies
backend/server.js      # Main server file
```

### Default Settings
- **Port**: 3000
- **Max File Size**: 50MB
- **Database**: SQLite (./database/av_scanner.db)
- **Upload Directory**: ./uploads
- **JWT Expiration**: 7 days

## 🔐 Security

**Before Production:**
- [ ] Change JWT_SECRET in .env
- [ ] Enable HTTPS
- [ ] Configure rate limiting
- [ ] Set up proper backups
- [ ] Review [README.md#-security--privacy](README.md#-security--privacy)

## 📊 Pricing Plans

| Plan | Price | Discounted | Scans |
|------|-------|------------|-------|
| Basic | $0.30/scan | $0.195/scan | Pay-per-scan |
| Personal | $60/month | $39/month | 200/month |
| Professional | $180/month | $117/month | 600/month |
| Enterprise | $360/month | $234/month | 1200/month |

*35% discount included!*

## 🆘 Need Help?

1. **Check documentation** (this index)
2. **Read troubleshooting** ([TROUBLESHOOTING.md](TROUBLESHOOTING.md))
3. **Review server logs** (console output)
4. **Try fresh install** (delete and reinstall)

## 📚 Documentation Statistics

- **7 documentation files**
- **150+ features documented**
- **Comprehensive API reference**
- **Complete troubleshooting guide**
- **Production deployment guide**
- **Security best practices**

## 🎉 Ready to Start?

### Quickest Path:
1. Run `setup.ps1` or `setup.bat`
2. Edit `.env` (set JWT_SECRET)
3. Open http://localhost:3000
4. Create account
5. Upload file
6. View results!

### Learning Path:
1. [PROJECT-SUMMARY.md](PROJECT-SUMMARY.md) - What you're getting
2. [QUICK-START.md](QUICK-START.md) - Get it running
3. [FEATURES.md](FEATURES.md) - Explore capabilities
4. [README.md](README.md) - Deep dive
5. [ARCHITECTURE.md](ARCHITECTURE.md) - Understand design

---

**Everything you need is documented. Let's get started! 🚀**

For the fastest start: Run `.\setup.ps1` now!
