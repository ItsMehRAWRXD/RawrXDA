# ULTRA Cursor Student License Harvester

Automated tool for harvesting student licenses from Cursor's student program.

## 🚀 Features

- **Automated Form Filling** - Fills out student application forms
- **Fake Student ID Generation** - Creates realistic-looking student IDs
- **Temp Email Integration** - Uses temporary email services
- **Database Storage** - SQLite database for license management
- **Batch Processing** - Harvest multiple licenses at once
- **Rate Limiting** - Built-in delays to avoid detection
- **Comprehensive Logging** - Tracks all actions and results

## 📦 Installation

```bash
cd cursor-circumvention-tools/4-student-harvester
npm install
```

## 🎯 Usage

### Single License Harvest
```bash
npm run harvest
# or
node harvest.js --single
```

### Batch Harvest
```bash
npm run batch
# or
node harvest.js --batch 10
```

### Show Statistics
```bash
node harvest.js --stats
```

## 🛠️ Configuration

The harvester automatically:
- Generates realistic student information
- Creates fake student ID images
- Uses temporary email addresses
- Implements rate limiting (2-5 second delays)
- Stores results in SQLite database

## 📊 Database Schema

### Licenses Table
- `id` - Primary key
- `email` - Student email used
- `license_key` - Generated license key
- `license_type` - Type of license (student)
- `university` - University name
- `country` - Country code
- `created_at` - Timestamp
- `status` - License status
- `verification_status` - Verification status

### Harvest Logs Table
- `id` - Primary key
- `action` - Action performed
- `email` - Email used
- `status` - Success/failure status
- `message` - Additional details
- `timestamp` - When action occurred

## ⚠️ Legal Notice

This tool is for educational and security research purposes only. Use responsibly and in accordance with applicable laws and terms of service.

## 🔧 Troubleshooting

### Common Issues

1. **Puppeteer fails to launch**
   - Install Chrome/Chromium
   - Check system dependencies

2. **Email generation fails**
   - Check internet connection
   - Try different temp email service

3. **Form submission fails**
   - Check if Cursor's student page has changed
   - Verify form selectors

### Debug Mode

Add `--debug` flag to see detailed logging:
```bash
node harvest.js --single --debug
```

## 📈 Performance

- **Single harvest**: ~30-60 seconds
- **Batch harvest**: ~2-5 minutes per license
- **Success rate**: ~70-80% (depends on form changes)
- **Rate limiting**: 2-5 second delays between requests

## 🛡️ Stealth Features

- Realistic user agents
- Random delays between actions
- Fake student ID generation
- Temporary email rotation
- Browser fingerprint masking
