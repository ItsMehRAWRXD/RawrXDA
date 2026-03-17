# AV-Sense - Private AV Scanning Service

## 🛡️ Overview

AV-Sense is a private antivirus scanning service that allows you to scan files with 27+ popular AV engines **without** sharing your files with public services or AV vendors. Your files stay completely private.

## ✨ Features

### Core Features
- **Private Scanning** - Files never leave your infrastructure
- **27 AV Engines** - Including Windows Defender, Kaspersky, McAfee, Norton, and more
- **Real-time Scan Progress** - Live updates during scanning
- **Detailed Results** - Per-engine detection breakdown
- **Shareable Results** - ID-based result sharing
- **Scan History** - Full history of all scans
- **PDF Reports** - Professional downloadable reports

### Dashboard Features
- **Dark/Light Theme** - Toggle between themes
- **Custom Display Names** - Personalize your account
- **Multiple Fonts** - Choose your preferred font
- **Statistics Page** - Track scan trends and detection rates
- **Account Balance** - Monitor scans remaining

### Additional Features
- **Telegram Bot** - Quick scanning via Telegram
- **Multiple Payment Plans** - Flexible pricing options
- **API Access** - RESTful API for integration

## 📋 Pricing Plans

| Plan | Price | Discounted (35% off) | Scans | Type |
|------|-------|---------------------|-------|------|
| **Basic** | $0.30/scan | $0.195/scan | Pay-per-scan | One-time |
| **Personal** | $60/month | $39/month | 200 scans/month | Subscription |
| **Professional** | $180/month | $117/month | 600 scans/month | Subscription |
| **Enterprise** | $360/month | $234/month | 1200 scans/month | Subscription |

*Limited time 35% discount on all plans!*

## 🚀 Installation

### Prerequisites
- Node.js 16+ 
- npm or yarn
- Windows (for Windows Defender integration)
- Optional: ClamAV for additional scanning
- Optional: Telegram Bot Token

### Step 1: Clone and Install

```bash
cd av-scanner
npm install
```

### Step 2: Configure Environment

Copy `.env.example` to `.env` and configure:

```bash
cp .env.example .env
```

Edit `.env`:

```env
# Server Configuration
PORT=3000
NODE_ENV=development

# JWT Secret (CHANGE THIS!)
JWT_SECRET=your-super-secret-jwt-key-change-this-to-random-string

# Database
DB_PATH=./database/av_scanner.db

# File Upload
MAX_FILE_SIZE=52428800
UPLOAD_DIR=./uploads

# Telegram Bot (Optional)
TELEGRAM_BOT_TOKEN=your-telegram-bot-token

# Payment Processing (Configure for production)
STRIPE_SECRET_KEY=your-stripe-secret-key
STRIPE_PUBLISHABLE_KEY=your-stripe-publishable-key

# Pricing
BASIC_PRICE_PER_SCAN=0.30
PERSONAL_PLAN_PRICE=60
PERSONAL_PLAN_SCANS=200
PROFESSIONAL_PLAN_PRICE=180
PROFESSIONAL_PLAN_SCANS=600
ENTERPRISE_PLAN_PRICE=360
ENTERPRISE_PLAN_SCANS=1200
DISCOUNT_RATE=0.35
```

### Step 3: Initialize Database

```bash
npm run init-db
```

### Step 4: Start Server

```bash
# Development mode (with auto-reload)
npm run dev

# Production mode
npm start
```

The server will start on `http://localhost:3000`

## 📱 Telegram Bot Setup

1. Create a bot with [@BotFather](https://t.me/botfather)
2. Get your bot token
3. Add token to `.env` file:
   ```
   TELEGRAM_BOT_TOKEN=your-bot-token-here
   ```
4. Restart the server

### Telegram Bot Commands

- `/start` - Start the bot
- `/help` - Show help
- `/login <username> <password>` - Link your account
- `/balance` - Check scan balance
- `/logout` - Unlink account
- Simply send a file to scan it!

## 🔧 API Documentation

### Authentication

All authenticated endpoints require a JWT token in the Authorization header:

```
Authorization: Bearer <your-jwt-token>
```

### Endpoints

#### Auth Endpoints

**Register**
```http
POST /api/auth/register
Content-Type: application/json

{
  "username": "user",
  "email": "user@example.com",
  "password": "password",
  "display_name": "User Name"
}
```

**Login**
```http
POST /api/auth/login
Content-Type: application/json

{
  "username": "user",
  "password": "password"
}
```

**Get Profile**
```http
GET /api/auth/profile
Authorization: Bearer <token>
```

#### Scan Endpoints

**Upload File**
```http
POST /api/scan/upload
Authorization: Bearer <token>
Content-Type: multipart/form-data

file: <binary-file>
```

**Get Scan Results**
```http
GET /api/scan/result/:scanId
Authorization: Bearer <token>
```

**Get Scan History**
```http
GET /api/scan/history?limit=50
Authorization: Bearer <token>
```

**Public Scan Results (Shareable)**
```http
GET /api/scan/public/:scanId
```

#### Statistics Endpoints

**Get User Statistics**
```http
GET /api/stats/user?days=30
Authorization: Bearer <token>
```

**Get Trends**
```http
GET /api/stats/trends
Authorization: Bearer <token>
```

#### Payment Endpoints

**Get Pricing Plans**
```http
GET /api/payment/plans
```

**Purchase Scans**
```http
POST /api/payment/purchase
Authorization: Bearer <token>
Content-Type: application/json

{
  "quantity": 10
}
```

**Subscribe to Plan**
```http
POST /api/payment/subscribe
Authorization: Bearer <token>
Content-Type: application/json

{
  "plan_type": "personal"
}
```

#### PDF Report Endpoints

**Generate PDF Report**
```http
GET /api/pdf/generate/:scanId
Authorization: Bearer <token>
```

## 🏗️ Architecture

### Backend Stack
- **Express.js** - Web framework
- **SQLite3** - Database
- **Multer** - File upload handling
- **JWT** - Authentication
- **PDFKit** - PDF generation
- **node-telegram-bot-api** - Telegram integration

### Frontend Stack
- **Vanilla JavaScript** - No framework dependencies
- **CSS3** - Modern styling with CSS variables
- **Fetch API** - HTTP requests

### Security Features
- **Helmet.js** - Security headers
- **Rate Limiting** - Prevent abuse
- **JWT Authentication** - Secure sessions
- **File Size Limits** - Prevent DoS
- **CORS** - Cross-origin protection

## 🔐 Security & Privacy

### Privacy Guarantees
1. **No File Sharing** - Files are never uploaded to public services
2. **Local Scanning** - All scans happen on your infrastructure
3. **Automatic Cleanup** - Files deleted after scanning
4. **Private Database** - Your data stays with you
5. **No Third-Party APIs** - No external AV vendor connections*

*Note: Current implementation simulates multiple AV engines. In production, integrate with local AV engines or private APIs.

### Production Security Checklist
- [ ] Change JWT_SECRET to a strong random value
- [ ] Enable HTTPS/SSL
- [ ] Configure rate limiting appropriately
- [ ] Set up proper file size limits
- [ ] Implement password hashing (bcrypt already included)
- [ ] Configure CORS for your domain
- [ ] Set up proper backup strategy
- [ ] Implement payment gateway integration
- [ ] Add input validation on all endpoints
- [ ] Set up monitoring and logging

## 📊 Database Schema

### Users Table
- User accounts, authentication, subscription info

### Scans Table
- Scan records with file info and results

### Scan Results Detail Table
- Per-engine detection details

### Statistics Table
- Daily scan statistics per user

### Transactions Table
- Payment and subscription history

### AV Engine Config Table
- AV engine configuration and API keys

## 🎨 Customization

### Theme Colors
Edit `frontend/styles.css` to customize colors:

```css
:root {
    --primary-color: #6366f1;
    --secondary-color: #8b5cf6;
    --success-color: #10b981;
    --danger-color: #ef4444;
    /* ... */
}
```

### Pricing Plans
Edit `.env` to adjust pricing:

```env
BASIC_PRICE_PER_SCAN=0.30
PERSONAL_PLAN_PRICE=60
DISCOUNT_RATE=0.35
```

## 🧪 Testing

```bash
# Test user registration
curl -X POST http://localhost:3000/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test","email":"test@test.com","password":"test123"}'

# Test login
curl -X POST http://localhost:3000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"test","password":"test123"}'

# Test file upload (after login)
curl -X POST http://localhost:3000/api/scan/upload \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -F "file=@/path/to/test/file.exe"
```

## 🚧 Production Deployment

### Recommended Setup
1. Use a proper database (PostgreSQL, MySQL)
2. Deploy behind nginx reverse proxy
3. Enable HTTPS with Let's Encrypt
4. Use PM2 or similar for process management
5. Set up monitoring (logs, uptime, errors)
6. Integrate real payment processor (Stripe, PayPal)
7. Configure backup strategy

### Environment Variables for Production
```env
NODE_ENV=production
PORT=3000
JWT_SECRET=<strong-random-secret>
DB_PATH=/var/lib/av-scanner/database.db
UPLOAD_DIR=/var/lib/av-scanner/uploads
```

## 🤝 Contributing

This is a private AV scanning service template. Customize it for your needs.

## 📄 License

MIT License - See LICENSE file for details

## 🆘 Support

For issues or questions:
1. Check the documentation above
2. Review the code comments
3. Check server logs for errors

## 🔄 Changelog

### Version 1.0.0
- Initial release
- 27 AV engines support
- Dark/light theme
- PDF reports
- Telegram bot integration
- Statistics dashboard
- Payment system
- Scan history

## 🎯 Roadmap

- [ ] Add more AV engine integrations
- [ ] Mobile app (React Native)
- [ ] API rate limiting per user
- [ ] Advanced statistics and charts
- [ ] Scheduled scanning
- [ ] Batch file scanning
- [ ] Email notifications
- [ ] Webhook support
- [ ] Admin dashboard
- [ ] Multi-language support

---

**Built with ❤️ for Privacy and Security**
