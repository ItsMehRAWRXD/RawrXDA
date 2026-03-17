# AV-Sense Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         AV-SENSE SYSTEM                              │
│                  Private AV Scanning Service                         │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                         CLIENT INTERFACES                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │   Web App    │  │ Telegram Bot │  │   REST API   │              │
│  │ (Dashboard)  │  │   Interface  │  │   Clients    │              │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘              │
│         │                  │                  │                      │
└─────────┼──────────────────┼──────────────────┼──────────────────────┘
          │                  │                  │
          └──────────────────┴──────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────────┐
│                      EXPRESS.JS SERVER                               │
│                         Port: 3000                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌────────────────────────────────────────────────────────┐         │
│  │              MIDDLEWARE LAYER                          │         │
│  ├────────────────────────────────────────────────────────┤         │
│  │  • Helmet.js (Security Headers)                        │         │
│  │  • CORS (Cross-Origin Protection)                      │         │
│  │  • Rate Limiter (100 req/15min)                        │         │
│  │  • JWT Authentication                                  │         │
│  │  • Multer (File Upload - 50MB limit)                   │         │
│  └────────────────────────────────────────────────────────┘         │
│                                                                      │
│  ┌────────────────────────────────────────────────────────┐         │
│  │                 API ROUTES                             │         │
│  ├────────────────────────────────────────────────────────┤         │
│  │  /api/auth/*      - Authentication & Authorization     │         │
│  │  /api/scan/*      - File Upload & Scanning             │         │
│  │  /api/stats/*     - Statistics & Analytics             │         │
│  │  /api/payment/*   - Billing & Subscriptions            │         │
│  │  /api/pdf/*       - PDF Report Generation              │         │
│  └────────────────────────────────────────────────────────┘         │
│                                                                      │
└──────────────────────────────┬───────────────────────────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
┌───────▼──────┐    ┌──────────▼─────────┐    ┌──────▼──────┐
│   Scanner    │    │      Database      │    │  PDF Gen    │
│   Engine     │    │     Operations     │    │  Service    │
├──────────────┤    ├────────────────────┤    ├─────────────┤
│              │    │                    │    │             │
│ • Heuristic  │    │ • User Management  │    │ • Report    │
│   Analysis   │    │ • Scan Records     │    │   Layout    │
│ • Entropy    │    │ • Statistics       │    │ • PDF       │
│   Calc       │    │ • Transactions     │    │   Export    │
│ • Pattern    │    │ • History          │    │             │
│   Match      │    │                    │    │             │
│              │    │                    │    │             │
│ • Win        │    └──────────┬─────────┘    └─────────────┘
│   Defender   │               │
│ • ClamAV     │               │
│ • 25 Other   │               │
│   Engines    │               │
│   (Sim)      │               │
└──────────────┘               │
                               │
                    ┌──────────▼─────────┐
                    │  SQLite Database   │
                    ├────────────────────┤
                    │                    │
                    │ Tables:            │
                    │ • users            │
                    │ • scans            │
                    │ • scan_results     │
                    │ • statistics       │
                    │ • transactions     │
                    │ • av_engine_config │
                    │                    │
                    └────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                         FILE FLOW                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. User uploads file → Multer middleware                           │
│  2. File saved to ./uploads/ with unique name                       │
│  3. Scan record created in database (pending)                       │
│  4. Scanner engine processes file:                                  │
│     • Calculate SHA-256 hash                                        │
│     • Perform heuristic analysis                                    │
│     • Scan with Windows Defender                                    │
│     • Scan with ClamAV                                              │
│     • Simulate 25 other AV engines                                  │
│  5. Results saved to database (completed)                           │
│  6. File automatically deleted                                      │
│  7. Statistics updated                                              │
│  8. Results returned to user                                        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    SECURITY LAYERS                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Layer 1: Rate Limiting                                             │
│  ├─ 100 requests per 15 minutes per IP                              │
│  └─ Prevents abuse and DoS attacks                                  │
│                                                                      │
│  Layer 2: Authentication                                            │
│  ├─ JWT tokens (7-day expiration)                                   │
│  ├─ bcrypt password hashing (10 rounds)                             │
│  └─ Bearer token authorization                                      │
│                                                                      │
│  Layer 3: Input Validation                                          │
│  ├─ File size limits (50MB max)                                     │
│  ├─ Content-Type validation                                         │
│  └─ SQL injection prevention                                        │
│                                                                      │
│  Layer 4: Privacy Protection                                        │
│  ├─ No external API calls                                           │
│  ├─ Automatic file deletion                                         │
│  ├─ Local-only processing                                           │
│  └─ Isolated scan environment                                       │
│                                                                      │
│  Layer 5: Application Security                                      │
│  ├─ Helmet.js security headers                                      │
│  ├─ CORS configuration                                              │
│  ├─ XSS protection                                                  │
│  └─ HTTPS ready                                                     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                   PAYMENT & BILLING FLOW                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  User → Select Plan → Payment Gateway → Transaction Record          │
│                           (Stripe/PayPal)          ↓                │
│                                                    ↓                │
│                                          Update User Balance        │
│                                                    ↓                │
│                                          Add Scans to Account        │
│                                                    ↓                │
│                                          Email Receipt (Optional)   │
│                                                                      │
│  Plans:                                                             │
│  • Basic: $0.195/scan (pay-per-use)                                │
│  • Personal: $39/month (200 scans)                                 │
│  • Professional: $117/month (600 scans)                            │
│  • Enterprise: $234/month (1200 scans)                             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                   TELEGRAM BOT FLOW                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Telegram User → /login command → Session Created                   │
│       ↓                                    ↓                        │
│  Send File                           User Linked                    │
│       ↓                                    ↓                        │
│  Bot Downloads → Temp Storage → Scanner → Results in Chat          │
│       ↓                                                             │
│  File Deleted                                                       │
│                                                                      │
│  Commands:                                                          │
│  • /start - Introduction                                            │
│  • /login - Link account                                            │
│  • /balance - Check scans                                           │
│  • /history - View scans                                            │
│  • [File] - Auto-scan                                               │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    DEPLOYMENT OPTIONS                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Development:                                                       │
│  └─ npm run dev (with nodemon auto-reload)                         │
│                                                                      │
│  Production:                                                        │
│  ├─ PM2 Process Manager                                             │
│  ├─ Nginx Reverse Proxy                                             │
│  ├─ SSL/TLS (Let's Encrypt)                                         │
│  └─ Environment: production                                         │
│                                                                      │
│  Container:                                                         │
│  └─ Docker-ready architecture                                       │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

                    Built with Node.js & Express.js
                    Designed for Privacy & Security
```
