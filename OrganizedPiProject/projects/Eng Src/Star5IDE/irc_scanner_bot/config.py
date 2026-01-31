grep -RIl --binary-files=without-match -E 'shell_exec|passthru|system|assert\\s*\\(|base64_decode\\(|gzinflate\\(|preg_replace\\s*\\(.+?/e' /var/www 2>/dev/null
grep -RIl --binary-files=without-match -E 'eval\\s*\\(|create_function\\s*\\(|assert\\s*\\(' /var/www 2>/dev/null
find /var/www -type f -name '*.php' -mtime -14 -ls
"""
IRC Scanner Bot Configuration
"""

import os
from typing import Dict, List

class Config:
    """Configuration class for IRC Scanner Bot"""
    
    # IRC Configuration
    IRC_SERVER = os.getenv('IRC_SERVER', 'irc.libera.chat')
    IRC_PORT = int(os.getenv('IRC_PORT', 6667))
    IRC_CHANNEL = os.getenv('IRC_CHANNEL', '#scanner-bot')
    IRC_NICKNAME = os.getenv('IRC_NICKNAME', 'ScanBot')
    IRC_IDENT = os.getenv('IRC_IDENT', 'scanner')
    IRC_REALNAME = os.getenv('IRC_REALNAME', 'Security Scanner Bot')
    
    # SSL/TLS Configuration
    USE_SSL = os.getenv('USE_SSL', 'False').lower() == 'true'
    
    # Bot Configuration
    COMMAND_PREFIX = os.getenv('COMMAND_PREFIX', '!')
    MAX_RESULTS_PER_ENGINE = int(os.getenv('MAX_RESULTS_PER_ENGINE', 100))
    REQUEST_TIMEOUT = int(os.getenv('REQUEST_TIMEOUT', 10))
    RATE_LIMIT_DELAY = float(os.getenv('RATE_LIMIT_DELAY', 1.0))
    
    # Scanner Configuration
    MAX_CONCURRENT_SCANS = int(os.getenv('MAX_CONCURRENT_SCANS', 10))
    SCAN_TIMEOUT = int(os.getenv('SCAN_TIMEOUT', 30))
    
    # Security Configuration
    ALLOWED_USERS: List[str] = os.getenv('ALLOWED_USERS', '').split(',') if os.getenv('ALLOWED_USERS') else []
    REQUIRE_AUTH = os.getenv('REQUIRE_AUTH', 'True').lower() == 'true'
    
    # Logging Configuration
    LOG_LEVEL = os.getenv('LOG_LEVEL', 'INFO')
    LOG_FILE = os.getenv('LOG_FILE', 'scanner_bot.log')
    
    # User Agents for web requests
    USER_AGENTS = [
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
        'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
        'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0',
        'Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:89.0) Gecko/20100101 Firefox/89.0',
    ]
    
    # Search Engine Domains
    GOOGLE_DOMAINS = [
        'com', 'co.uk', 'ca', 'com.au', 'de', 'fr', 'it', 'es', 'nl', 'be',
        'ch', 'se', 'no', 'dk', 'fi', 'co.nz', 'co.jp', 'com.br', 'com.mx',
        'com.ar', 'co.in', 'ru', 'pl', 'com.tr', 'com.ua', 'com.sg', 'com.hk'
    ]
    
    # File paths for vulnerability testing (example patterns)
    PATH_PATTERNS = [
        'index.php?', 'home.php?', 'page.php?', 'view.php?', 'file.php?',
        'content.php?', 'include.php?', 'main.php?', 'load.php?', 'show.php?'
    ]
    
    @classmethod
    def validate(cls):
        """Validate configuration"""
        if not cls.IRC_SERVER:
            raise ValueError("IRC_SERVER must be configured")
        if not cls.IRC_CHANNEL:
            raise ValueError("IRC_CHANNEL must be configured")
        if cls.IRC_PORT < 1 or cls.IRC_PORT > 65535:
            raise ValueError("IRC_PORT must be between 1 and 65535")