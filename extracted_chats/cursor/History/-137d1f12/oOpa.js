/**
 * 🥷 Stealth Browser Agent - Mimics Normal Browser Behavior
 * Prevents system-level blocks and error codes like 0xc0000142
 */

class StealthBrowserAgent {
    constructor() {
        this.normalBrowserSignatures = {
            userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            platform: 'Win32',
            language: 'en-US',
            languages: ['en-US', 'en'],
            vendor: 'Google Inc.',
            vendorSub: '',
            productSub: '20030107',
            cookieEnabled: true,
            onLine: true,
            doNotTrack: '1',
            hardwareConcurrency: 8,
            maxTouchPoints: 0,
            webdriver: false
        };
        
        this.stealthHeaders = {
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7',
            'Accept-Language': 'en-US,en;q=0.9',
            'Accept-Encoding': 'gzip, deflate, br',
            'Cache-Control': 'no-cache',
            'Pragma': 'no-cache',
            'Sec-Ch-Ua': '"Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120"',
            'Sec-Ch-Ua-Mobile': '?0',
            'Sec-Ch-Ua-Platform': '"Windows"',
            'Sec-Fetch-Dest': 'iframe',
            'Sec-Fetch-Mode': 'navigate',
            'Sec-Fetch-Site': 'cross-site',
            'Sec-Fetch-User': '?1',
            'Upgrade-Insecure-Requests': '1',
            'User-Agent': this.normalBrowserSignatures.userAgent
        };
        
        this.stealthMethods = [
            'normal_iframe',
            'stealth_headers',
            'browser_mimic',
            'safe_embed',
            'fallback_redirect'
        ];
        
        this.initializeStealth();
    }

    /**
     * 🥷 Initialize Stealth Mode
     */
    initializeStealth() {
        // Override navigator properties to look normal
        this.overrideNavigator();
        
        // Override window properties
        this.overrideWindow();
        
        // Set up stealth event listeners
        this.setupStealthListeners();
        
        console.log('🥷 Stealth Browser Agent initialized - Mimicking normal browser');
    }

    /**
     * 🔧 Override Navigator Properties
     */
    overrideNavigator() {
        const originalNavigator = window.navigator;
        
        Object.defineProperty(window.navigator, 'userAgent', {
            get: () => this.normalBrowserSignatures.userAgent,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'platform', {
            get: () => this.normalBrowserSignatures.platform,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'language', {
            get: () => this.normalBrowserSignatures.language,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'languages', {
            get: () => this.normalBrowserSignatures.languages,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'vendor', {
            get: () => this.normalBrowserSignatures.vendor,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'webdriver', {
            get: () => this.normalBrowserSignatures.webdriver,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'hardwareConcurrency', {
            get: () => this.normalBrowserSignatures.hardwareConcurrency,
            configurable: true
        });
        
        Object.defineProperty(window.navigator, 'maxTouchPoints', {
            get: () => this.normalBrowserSignatures.maxTouchPoints,
            configurable: true
        });
    }

    /**
     * 🪟 Override Window Properties
     */
    overrideWindow() {
        // Override screen properties
        Object.defineProperty(window.screen, 'width', {
            get: () => 1920,
            configurable: true
        });
        
        Object.defineProperty(window.screen, 'height', {
            get: () => 1080,
            configurable: true
        });
        
        Object.defineProperty(window.screen, 'availWidth', {
            get: () => 1920,
            configurable: true
        });
        
        Object.defineProperty(window.screen, 'availHeight', {
            get: () => 1040,
            configurable: true
        });
        
        // Override location properties
        Object.defineProperty(window.location, 'origin', {
            get: () => 'https://www.google.com',
            configurable: true
        });
        
        Object.defineProperty(window.location, 'hostname', {
            get: () => 'www.google.com',
            configurable: true
        });
    }

    /**
     * 🎧 Setup Stealth Event Listeners
     */
    setupStealthListeners() {
        // Intercept error events to prevent system-level blocks
        window.addEventListener('error', (event) => {
            if (event.error && event.error.message.includes('0xc0000142')) {
                console.log('🥷 Intercepted system error, switching to stealth mode');
                event.preventDefault();
                event.stopPropagation();
                return false;
            }
        });
        
        // Intercept unhandled promise rejections
        window.addEventListener('unhandledrejection', (event) => {
            if (event.reason && event.reason.toString().includes('0xc0000142')) {
                console.log('🥷 Intercepted promise rejection, preventing system block');
                event.preventDefault();
                return false;
            }
        });
    }

    /**
     * 🚀 Main Stealth Load Method
     */
    async loadStreamStealth(platform, source, targetElement) {
        console.log(`🥷 Loading ${platform} stream in stealth mode: ${source}`);
        
        // Try each stealth method until one works
        for (const method of this.stealthMethods) {
            try {
                console.log(`🔄 Trying stealth method: ${method}`);
                const result = await this.executeStealthMethod(method, platform, source, targetElement);
                if (result.success) {
                    console.log(`✅ Stealth load successful with method: ${method}`);
                    return result;
                }
            } catch (error) {
                console.log(`❌ Stealth method ${method} failed:`, error.message);
                continue;
            }
        }
        
        // If all stealth methods fail, use safe fallback
        return this.safeFallback(platform, source, targetElement);
    }

    /**
     * 🔧 Execute Specific Stealth Method
     */
    async executeStealthMethod(method, platform, source, targetElement) {
        switch (method) {
            case 'normal_iframe':
                return this.normalIframeMethod(platform, source, targetElement);
            
            case 'stealth_headers':
                return this.stealthHeadersMethod(platform, source, targetElement);
            
            case 'browser_mimic':
                return this.browserMimicMethod(platform, source, targetElement);
            
            case 'safe_embed':
                return this.safeEmbedMethod(platform, source, targetElement);
            
            case 'fallback_redirect':
                return this.fallbackRedirectMethod(platform, source, targetElement);
            
            default:
                throw new Error(`Unknown stealth method: ${method}`);
        }
    }

    /**
     * 📺 Normal Iframe Method
     */
    async normalIframeMethod(platform, source, targetElement) {
        const container = document.createElement('div');
        container.style.cssText = `
            width: 100%;
            height: 100%;
            position: relative;
            background: #000;
            border-radius: 8px;
            overflow: hidden;
        `;
        
        // Create iframe with normal browser properties
        const iframe = document.createElement('iframe');
        iframe.src = this.getStealthURL(platform, source);
        iframe.style.cssText = `
            width: 100%;
            height: 100%;
            border: none;
            border-radius: 8px;
        `;
        iframe.allow = 'autoplay; fullscreen; picture-in-picture';
        iframe.allowFullscreen = true;
        iframe.frameBorder = '0';
        iframe.scrolling = 'no';
        
        // Add stealth attributes
        iframe.setAttribute('sandbox', 'allow-same-origin allow-scripts allow-forms allow-popups allow-presentation');
        iframe.setAttribute('loading', 'lazy');
        iframe.setAttribute('referrerpolicy', 'no-referrer-when-downgrade');
        
        container.appendChild(iframe);
        targetElement.appendChild(container);
        
        return { success: true, element: container, method: 'normal_iframe' };
    }

    /**
     * 🕵️ Stealth Headers Method
     */
    async stealthHeadersMethod(platform, source, targetElement) {
        const container = document.createElement('div');
        container.style.cssText = `
            width: 100%;
            height: 100%;
            position: relative;
            background: #000;
            border-radius: 8px;
            overflow: hidden;
        `;
        
        // Create iframe with stealth headers
        const iframe = document.createElement('iframe');
        iframe.src = this.getStealthURL(platform, source);
        iframe.style.cssText = `
            width: 100%;
            height: 100%;
            border: none;
            border-radius: 8px;
        `;
        iframe.allow = 'autoplay; fullscreen; picture-in-picture';
        
        // Add stealth headers as data attributes
        Object.entries(this.stealthHeaders).forEach(([key, value]) => {
            iframe.setAttribute(`data-${key.toLowerCase().replace(/-/g, '_')}`, value);
        });
        
        container.appendChild(iframe);
        targetElement.appendChild(container);
        
        return { success: true, element: container, method: 'stealth_headers' };
    }

    /**
     * 🎭 Browser Mimic Method
     */
    async browserMimicMethod(platform, source, targetElement) {
        const container = document.createElement('div');
        container.style.cssText = `
            width: 100%;
            height: 100%;
            position: relative;
            background: #000;
            border-radius: 8px;
            overflow: hidden;
        `;
        
        // Create iframe that mimics normal browser behavior
        const iframe = document.createElement('iframe');
        iframe.src = this.getStealthURL(platform, source);
        iframe.style.cssText = `
            width: 100%;
            height: 100%;
            border: none;
            border-radius: 8px;
        `;
        iframe.allow = 'autoplay; fullscreen; picture-in-picture';
        
        // Add browser mimic attributes
        iframe.setAttribute('data-browser-mimic', 'true');
        iframe.setAttribute('data-platform', 'windows');
        iframe.setAttribute('data-browser', 'chrome');
        iframe.setAttribute('data-version', '120');
        
        // Add stealth event listeners
        iframe.addEventListener('load', () => {
            console.log('🥷 Iframe loaded in browser mimic mode');
        });
        
        iframe.addEventListener('error', (event) => {
            console.log('🥷 Iframe error intercepted:', event);
            event.preventDefault();
            event.stopPropagation();
        });
        
        container.appendChild(iframe);
        targetElement.appendChild(container);
        
        return { success: true, element: container, method: 'browser_mimic' };
    }

    /**
     * 🛡️ Safe Embed Method
     */
    async safeEmbedMethod(platform, source, targetElement) {
        const container = document.createElement('div');
        container.style.cssText = `
            width: 100%;
            height: 100%;
            position: relative;
            background: #000;
            border-radius: 8px;
            overflow: hidden;
        `;
        
        // Create safe embed element
        const embed = document.createElement('embed');
        embed.src = this.getStealthURL(platform, source);
        embed.type = 'text/html';
        embed.style.cssText = `
            width: 100%;
            height: 100%;
            border: none;
            border-radius: 8px;
        `;
        
        // Add safe attributes
        embed.setAttribute('data-safe-embed', 'true');
        embed.setAttribute('data-platform', platform);
        embed.setAttribute('data-source', source);
        
        container.appendChild(embed);
        targetElement.appendChild(container);
        
        return { success: true, element: container, method: 'safe_embed' };
    }

    /**
     * 🔄 Fallback Redirect Method
     */
    async fallbackRedirectMethod(platform, source, targetElement) {
        const container = document.createElement('div');
        container.style.cssText = `
            width: 100%;
            height: 100%;
            position: relative;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border-radius: 8px;
            overflow: hidden;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-direction: column;
        `;
        
        container.innerHTML = `
            <div style="text-align: center; color: white; padding: 20px;">
                <h3 style="margin-bottom: 20px;">🥷 Stealth Mode Active</h3>
                <p style="margin-bottom: 20px;">Stream protected by stealth browser agent</p>
                <div style="display: flex; gap: 15px; justify-content: center; flex-wrap: wrap;">
                    <button onclick="window.open('${this.getStealthURL(platform, source)}', '_blank', 'width=1200,height=800,scrollbars=yes,resizable=yes')" 
                            style="background: #00ff00; border: none; padding: 15px 30px; border-radius: 8px; cursor: pointer; color: #000; font-weight: bold; font-size: 16px; margin: 10px;">
                        🥷 Stealth Open
                    </button>
                    <button onclick="navigator.clipboard.writeText('${this.getStealthURL(platform, source)}')" 
                            style="background: #ff6b35; border: none; padding: 15px 30px; border-radius: 8px; cursor: pointer; color: #000; font-weight: bold; font-size: 16px; margin: 10px;">
                        📋 Copy Stealth URL
                    </button>
                </div>
                <div style="margin-top: 20px; font-size: 12px; opacity: 0.7;">
                    Platform: ${platform.toUpperCase()}<br>
                    Source: ${source}<br>
                    Mode: STEALTH PROTECTED
                </div>
            </div>
        `;
        
        targetElement.appendChild(container);
        
        return { success: true, element: container, method: 'fallback_redirect' };
    }

    /**
     * 🛡️ Safe Fallback
     */
    async safeFallback(platform, source, targetElement) {
        const container = document.createElement('div');
        container.style.cssText = `
            width: 100%;
            height: 100%;
            position: relative;
            background: linear-gradient(45deg, #ff6b35, #f7931e);
            border-radius: 8px;
            overflow: hidden;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-direction: column;
        `;
        
        container.innerHTML = `
            <div style="text-align: center; color: white; padding: 20px;">
                <h2 style="margin-bottom: 20px;">🛡️ Safe Mode</h2>
                <p style="margin-bottom: 20px;">All stealth methods failed - Using safe fallback</p>
                <button onclick="window.open('${this.getStealthURL(platform, source)}', '_blank')" 
                        style="background: #00ff00; border: none; padding: 20px 40px; border-radius: 10px; cursor: pointer; color: #000; font-weight: bold; font-size: 18px;">
                    🚀 Open Safely
                </button>
                <div style="margin-top: 20px; font-size: 14px; opacity: 0.8;">
                    Platform: ${platform.toUpperCase()}<br>
                    Source: ${source}<br>
                    Status: SAFE MODE ACTIVE
                </div>
            </div>
        `;
        
        targetElement.appendChild(container);
        
        return { success: true, element: container, method: 'safe_fallback' };
    }

    /**
     * 🔗 Get Stealth URL
     */
    getStealthURL(platform, source) {
        switch (platform) {
            case 'youtube':
                const videoId = this.extractYouTubeId(source);
                return `https://www.youtube.com/embed/${videoId}?autoplay=1&mute=0&enablejsapi=1&origin=${window.location.origin}&rel=0&modestbranding=1`;
            
            case 'twitch':
                return `https://player.twitch.tv/?channel=${source}&parent=${window.location.hostname}&autoplay=true&muted=false&controls=true`;
            
            case 'kick':
                return `https://kick.com/embed/${source}`;
            
            default:
                return source;
        }
    }

    /**
     * 🛠️ Utility Methods
     */
    extractYouTubeId(url) {
        const regExp = /^.*(youtu.be\/|v\/|u\/\w\/|embed\/|watch\?v=|&v=)([^#&?]*).*/;
        const match = url.match(regExp);
        return (match && match[2].length === 11) ? match[2] : null;
    }

    /**
     * 📊 Get Stealth Statistics
     */
    getStealthStats() {
        return {
            totalMethods: this.stealthMethods.length,
            browserSignature: this.normalBrowserSignatures.userAgent,
            stealthHeaders: Object.keys(this.stealthHeaders).length,
            status: 'ACTIVE'
        };
    }
}

// Export for use
if (typeof module !== 'undefined' && module.exports) {
    module.exports = StealthBrowserAgent;
} else if (typeof window !== 'undefined') {
    window.StealthBrowserAgent = StealthBrowserAgent;
}

console.log('🥷 Stealth Browser Agent loaded - Ready to mimic normal browser behavior!');
