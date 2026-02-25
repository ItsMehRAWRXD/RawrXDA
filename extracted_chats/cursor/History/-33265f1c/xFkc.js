/**
 * BigDaddyG IDE - Image Generation Engine
 * Generate images directly in the IDE with !pic command
 * "Conducting a cognitive symphony in pixels..."
 */

(function() {
'use strict';

class ImageGenerator {
    constructor() {
        this.enabled = true;
        this.apiEndpoint = 'http://localhost:11441/api/generate-image';
        this.generatedImages = [];
        this.savePath = null;
        
        console.log('[ImageGen] 🎨 Initializing image generator...');
    }
    
    async init() {
        // Register !pic command handler
        this.registerCommandHandler();
        
        // Create image gallery panel
        this.createGalleryUI();
        
        console.log('[ImageGen] ✅ Image generator ready! Use "!pic your description"');
    }
    
    registerCommandHandler() {
        // Hook into AI chat to intercept !pic commands
        if (window.floatingChat) {
            const originalSend = window.floatingChat.send.bind(window.floatingChat);
            
            window.floatingChat.send = async function() {
                const input = document.getElementById('floating-chat-input');
                const message = input?.value.trim();
                
                if (message && message.startsWith('!pic ')) {
                    // Handle image generation
                    const prompt = message.substring(5).trim();
                    
                    if (prompt) {
                        input.value = '';
                        await window.imageGenerator.generateImage(prompt);
                        return;
                    }
                }
                
                // Call original send for non-!pic messages
                return originalSend();
            };
            
            console.log('[ImageGen] ✅ !pic command registered in floating chat');
        }
        
        // Also register in right sidebar chat if exists
        const aiInput = document.getElementById('ai-input');
        if (aiInput) {
            const originalHandler = window.sendToAI;
            
            window.sendToAI = async function() {
                const message = aiInput?.value.trim();
                
                if (message && message.startsWith('!pic ')) {
                    const prompt = message.substring(5).trim();
                    
                    if (prompt) {
                        aiInput.value = '';
                        await window.imageGenerator.generateImage(prompt);
                        return;
                    }
                }
                
                // Call original handler
                if (originalHandler) {
                    return originalHandler();
                }
            };
            
            console.log('[ImageGen] ✅ !pic command registered in sidebar chat');
        }
    }
    
    createGalleryUI() {
        // Add gallery button to right sidebar
        const chatTab = document.querySelector('[data-tab="chat"]');
        
        if (chatTab) {
            const galleryBtn = document.createElement('button');
            galleryBtn.style.cssText = `
                margin: 8px 12px;
                padding: 8px 12px;
                background: linear-gradient(135deg, #f093fb, #f5576c);
                border: none;
                color: white;
                border-radius: 8px;
                cursor: pointer;
                font-size: 12px;
                font-weight: 600;
                transition: all 0.2s;
            `;
            galleryBtn.innerHTML = '🎨 Image Gallery';
            galleryBtn.onclick = () => this.openGallery();
            
            const chatContent = document.getElementById('chat-tab-content');
            if (chatContent) {
                chatContent.insertBefore(galleryBtn, chatContent.firstChild);
            }
        }
    }
    
    async generateImage(prompt) {
        console.log(`[ImageGen] 🎨 Generating image: "${prompt}"`);
        
        // Show generating message
        this.showGeneratingMessage(prompt);
        
        try {
            // Build cinematic prompt
            const cinematicPrompt = this.buildCinematicPrompt(prompt);
            
            // Try local generation first (Orchestra)
            let imageData = await this.generateLocal(cinematicPrompt);
            
            // If local fails, try cloud APIs
            if (!imageData) {
                imageData = await this.generateCloud(cinematicPrompt);
            }
            
            if (imageData) {
                // Display image
                await this.displayImage(prompt, imageData);
                
                // Save to gallery
                this.saveToGallery(prompt, imageData);
                
                console.log('[ImageGen] ✅ Image generated successfully!');
            } else {
                throw new Error('No image generation service available');
            }
            
        } catch (error) {
            console.error('[ImageGen] ❌ Generation failed:', error);
            this.showErrorMessage(prompt, error.message);
        }
    }
    
    buildCinematicPrompt(userPrompt) {
        // Enhance prompt with cinematic elements
        const enhancements = [
            'cinematic lighting',
            'neon jade and beige color palette',
            'dark IDE environment',
            'glowing overlays',
            'volumetric lighting',
            'firefly-like particle effects'
        ];
        
        const enhanced = `${userPrompt}, ${enhancements.join(', ')}, high quality, detailed, 4K`;
        
        console.log('[ImageGen] 🎬 Enhanced prompt:', enhanced);
        
        return enhanced;
    }
    
    async generateLocal(prompt) {
        console.log('[ImageGen] 🏠 Trying local generation (Orchestra)...');
        
        try {
            const response = await fetch(this.apiEndpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    prompt,
                    width: 1024,
                    height: 1024,
                    model: 'stable-diffusion', // If Orchestra supports it
                    steps: 30,
                    cfg_scale: 7.5
                }),
                timeout: 60000
            });
            
            if (response.ok) {
                const data = await response.json();
                console.log('[ImageGen] ✅ Local generation successful');
                return data.image || data.data; // Base64 image
            }
        } catch (error) {
            console.log('[ImageGen] ℹ️ Local generation not available:', error.message);
        }
        
        return null;
    }
    
    async generateCloud(prompt) {
        console.log('[ImageGen] ☁️ Trying cloud generation...');
        
        // Try multiple cloud providers
        const providers = [
            { name: 'Pollinations', url: `https://image.pollinations.ai/prompt/${encodeURIComponent(prompt)}` },
            { name: 'Replicate', url: 'https://api.replicate.com/v1/predictions' },
            // Add more providers as needed
        ];
        
        // Try Pollinations (free, no API key needed!)
        try {
            const pollinationsUrl = `https://image.pollinations.ai/prompt/${encodeURIComponent(prompt)}?width=1024&height=1024&nologo=true`;
            
            console.log('[ImageGen] 🌸 Using Pollinations AI...');
            
            // Fetch image as blob
            const response = await fetch(pollinationsUrl);
            
            if (response.ok) {
                const blob = await response.blob();
                const reader = new FileReader();
                
                return new Promise((resolve) => {
                    reader.onloadend = () => {
                        console.log('[ImageGen] ✅ Cloud generation successful (Pollinations)');
                        resolve(reader.result); // Base64
                    };
                    reader.readAsDataURL(blob);
                });
            }
        } catch (error) {
            console.error('[ImageGen] ❌ Cloud generation failed:', error);
        }
        
        return null;
    }
    
    showGeneratingMessage(prompt) {
        // Add message to chat
        const container = document.getElementById('floating-chat-messages') || 
                          document.getElementById('ai-messages') ||
                          document.getElementById('ai-chat-messages');
        
        if (!container) return;
        
        const msgDiv = document.createElement('div');
        msgDiv.id = 'image-gen-progress';
        msgDiv.style.cssText = `
            margin-bottom: 16px;
            padding: 16px;
            background: linear-gradient(135deg, rgba(240, 147, 251, 0.1), rgba(245, 87, 108, 0.1));
            border-left: 4px solid #f093fb;
            border-radius: 12px;
            animation: pulse 2s infinite;
        `;
        
        msgDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 12px; margin-bottom: 12px;">
                <span style="font-size: 20px;">🎨</span>
                <span style="font-weight: 600; color: #f093fb;">Image Generator</span>
                <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
            </div>
            <div style="color: var(--cursor-text); font-size: 13px; margin-bottom: 12px;">
                <strong>Prompt:</strong> "${prompt}"
            </div>
            <div style="display: flex; align-items: center; gap: 8px; color: var(--cursor-accent);">
                <div style="width: 16px; height: 16px; border: 3px solid #f093fb; border-top-color: transparent; border-radius: 50%; animation: spin 1s linear infinite;"></div>
                <span style="font-size: 12px; font-style: italic;">"Conducting a cognitive symphony in pixels..."</span>
            </div>
        `;
        
        container.appendChild(msgDiv);
        container.scrollTop = container.scrollHeight;
        
        // Add animations
        if (!document.getElementById('imagegen-animations')) {
            const style = document.createElement('style');
            style.id = 'imagegen-animations';
            style.textContent = `
                @keyframes pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: 0.7; }
                }
                @keyframes spin {
                    from { transform: rotate(0deg); }
                    to { transform: rotate(360deg); }
                }
            `;
            document.head.appendChild(style);
        }
    }
    
    async displayImage(prompt, imageData) {
        // Remove generating message
        const progressMsg = document.getElementById('image-gen-progress');
        if (progressMsg) progressMsg.remove();
        
        // Add image to chat
        const container = document.getElementById('floating-chat-messages') || 
                          document.getElementById('ai-messages') ||
                          document.getElementById('ai-chat-messages');
        
        if (!container) return;
        
        const imgDiv = document.createElement('div');
        imgDiv.style.cssText = `
            margin-bottom: 16px;
            padding: 16px;
            background: linear-gradient(135deg, rgba(240, 147, 251, 0.1), rgba(245, 87, 108, 0.1));
            border-left: 4px solid #f093fb;
            border-radius: 12px;
        `;
        
        imgDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 12px; margin-bottom: 12px;">
                <span style="font-size: 20px;">🎨</span>
                <span style="font-weight: 600; color: #f093fb;">Generated Image</span>
                <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
                <span style="margin-left: auto; font-size: 10px; color: var(--cursor-jade-dark); background: rgba(119, 221, 190, 0.2); padding: 3px 8px; border-radius: 10px;">✓ Success</span>
            </div>
            <div style="color: var(--cursor-text-secondary); font-size: 11px; margin-bottom: 12px;">
                <strong>Prompt:</strong> "${prompt}"
            </div>
            <div style="border-radius: 8px; overflow: hidden; box-shadow: 0 8px 24px rgba(0,0,0,0.3); margin-bottom: 12px;">
                <img src="${imageData}" style="width: 100%; height: auto; display: block; user-select: none;" alt="${prompt}">
            </div>
            <div style="display: flex; gap: 8px;">
                <button onclick="imageGenerator.saveImage('${imageData}', '${prompt.replace(/'/g, "\\'")}')" style="flex: 1; background: rgba(119, 221, 190, 0.15); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    💾 Save to Project
                </button>
                <button onclick="imageGenerator.copyToClipboard('${imageData}')" style="flex: 1; background: rgba(119, 221, 190, 0.15); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    📋 Copy Image
                </button>
                <button onclick="imageGenerator.regenerate('${prompt.replace(/'/g, "\\'")}')" style="flex: 1; background: rgba(240, 147, 251, 0.15); border: 1px solid #f093fb; color: #f093fb; padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    🔄 Regenerate
                </button>
            </div>
        `;
        
        container.appendChild(imgDiv);
        container.scrollTop = container.scrollHeight;
        
        // Show success notification
        this.showNotification('✅ Image generated!', 'success');
    }
    
    showErrorMessage(prompt, error) {
        // Remove generating message
        const progressMsg = document.getElementById('image-gen-progress');
        if (progressMsg) progressMsg.remove();
        
        const container = document.getElementById('floating-chat-messages') || 
                          document.getElementById('ai-messages') ||
                          document.getElementById('ai-chat-messages');
        
        if (!container) return;
        
        const errorDiv = document.createElement('div');
        errorDiv.style.cssText = `
            margin-bottom: 16px;
            padding: 16px;
            background: rgba(255, 71, 87, 0.1);
            border-left: 4px solid #ff4757;
            border-radius: 12px;
        `;
        
        errorDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 12px; margin-bottom: 8px;">
                <span style="font-size: 20px;">❌</span>
                <span style="font-weight: 600; color: #ff4757;">Image Generation Failed</span>
            </div>
            <div style="color: var(--cursor-text-secondary); font-size: 12px; margin-bottom: 8px;">
                <strong>Prompt:</strong> "${prompt}"
            </div>
            <div style="color: #ff4757; font-size: 12px; line-height: 1.6;">
                <strong>Error:</strong> ${error}
                <br><br>
                <strong>Available options:</strong>
                <ul style="margin: 8px 0; padding-left: 20px;">
                    <li>Ensure Orchestra server supports image generation</li>
                    <li>Or use cloud generation (requires internet)</li>
                    <li>Try a different prompt</li>
                </ul>
            </div>
            <button onclick="imageGenerator.regenerate('${prompt.replace(/'/g, "\\'")}')" style="margin-top: 8px; background: rgba(255, 71, 87, 0.15); border: 1px solid #ff4757; color: #ff4757; padding: 8px 12px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                🔄 Try Again
            </button>
        `;
        
        container.appendChild(errorDiv);
        container.scrollTop = container.scrollHeight;
    }
    
    async saveImage(imageData, prompt) {
        console.log('[ImageGen] 💾 Saving image...');
        
        try {
            if (window.electron && window.electron.saveFileDialog) {
                const defaultName = `${prompt.replace(/[^a-z0-9]/gi, '_').substring(0, 50)}_${Date.now()}.png`;
                
                const filePath = await window.electron.saveFileDialog({
                    title: 'Save Generated Image',
                    defaultPath: defaultName,
                    filters: [
                        { name: 'PNG Images', extensions: ['png'] },
                        { name: 'All Files', extensions: ['*'] }
                    ]
                });
                
                if (filePath) {
                    // Convert base64 to binary
                    const base64Data = imageData.split(',')[1];
                    
                    if (window.electron.writeFile) {
                        await window.electron.writeFile(filePath, base64Data, 'base64');
                        this.showNotification(`✅ Saved: ${filePath}`, 'success');
                    }
                }
            } else {
                // Fallback: download in browser
                const link = document.createElement('a');
                link.href = imageData;
                link.download = `generated_${Date.now()}.png`;
                link.click();
                this.showNotification('✅ Image downloaded!', 'success');
            }
        } catch (error) {
            console.error('[ImageGen] ❌ Save failed:', error);
            this.showNotification('❌ Failed to save image', 'error');
        }
    }
    
    copyToClipboard(imageData) {
        console.log('[ImageGen] 📋 Copying image to clipboard...');
        
        try {
            // Convert base64 to blob
            fetch(imageData)
                .then(res => res.blob())
                .then(blob => {
                    const item = new ClipboardItem({ 'image/png': blob });
                    navigator.clipboard.write([item])
                        .then(() => {
                            this.showNotification('✅ Image copied to clipboard!', 'success');
                        })
                        .catch(err => {
                            console.error('[ImageGen] Clipboard error:', err);
                            this.showNotification('❌ Failed to copy image', 'error');
                        });
                });
        } catch (error) {
            console.error('[ImageGen] ❌ Copy failed:', error);
            this.showNotification('❌ Failed to copy image', 'error');
        }
    }
    
    async regenerate(prompt) {
        console.log('[ImageGen] 🔄 Regenerating image...');
        await this.generateImage(prompt);
    }
    
    saveToGallery(prompt, imageData) {
        this.generatedImages.push({
            timestamp: Date.now(),
            prompt,
            imageData,
            id: `img-${Date.now()}`
        });
        
        // Save to localStorage
        try {
            localStorage.setItem('bigdaddyg-image-gallery', JSON.stringify(this.generatedImages));
        } catch (error) {
            console.warn('[ImageGen] Failed to save to gallery:', error);
        }
    }
    
    openGallery() {
        console.log('[ImageGen] 🖼️ Opening image gallery...');
        
        // Load from localStorage
        try {
            const saved = localStorage.getItem('bigdaddyg-image-gallery');
            if (saved) {
                this.generatedImages = JSON.parse(saved);
            }
        } catch (error) {
            console.warn('[ImageGen] Failed to load gallery:', error);
        }
        
        // Create gallery modal
        const modal = document.createElement('div');
        modal.style.cssText = `
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 90vw;
            max-width: 1200px;
            height: 80vh;
            background: var(--cursor-bg);
            border: 2px solid var(--cursor-jade-dark);
            border-radius: 16px;
            box-shadow: 0 24px 48px rgba(0, 0, 0, 0.5);
            z-index: 10002;
            overflow: hidden;
            animation: slideInUp 0.3s ease-out;
        `;
        
        modal.innerHTML = `
            <div style="padding: 16px 20px; background: linear-gradient(135deg, var(--cursor-bg-secondary), var(--cursor-bg-tertiary)); border-bottom: 1px solid var(--cursor-border); display: flex; justify-content: space-between; align-items: center;">
                <div style="display: flex; align-items: center; gap: 12px;">
                    <span style="font-size: 20px;">🖼️</span>
                    <span style="font-weight: 600; font-size: 15px; color: var(--cursor-jade-dark);">Image Gallery</span>
                    <span style="font-size: 11px; color: var(--cursor-text-secondary); background: rgba(119, 221, 190, 0.1); padding: 3px 8px; border-radius: 12px;">${this.generatedImages.length} images</span>
                </div>
                <button onclick="this.parentElement.parentElement.remove()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px;">
                    ✕ Close
                </button>
            </div>
            <div style="height: calc(100% - 60px); overflow-y: auto; padding: 20px; display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 16px;">
                ${this.generatedImages.length === 0 ? 
                    `<div style="grid-column: 1 / -1; text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                        <div style="font-size: 48px; margin-bottom: 16px;">🎨</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">No images yet</div>
                        <div style="font-size: 13px;">Use "!pic your description" to generate images</div>
                    </div>` :
                    this.generatedImages.map(img => `
                        <div style="background: var(--cursor-bg-secondary); border: 1px solid var(--cursor-border); border-radius: 12px; overflow: hidden; transition: all 0.2s;" onmouseover="this.style.borderColor='var(--cursor-jade-light)'" onmouseout="this.style.borderColor='var(--cursor-border)'">
                            <img src="${img.imageData}" style="width: 100%; height: 200px; object-fit: cover; cursor: pointer;" onclick="window.open('${img.imageData}', '_blank')" alt="${img.prompt}">
                            <div style="padding: 12px;">
                                <div style="font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 6px;">
                                    ${new Date(img.timestamp).toLocaleString()}
                                </div>
                                <div style="font-size: 12px; color: var(--cursor-text); margin-bottom: 8px; line-height: 1.4;">
                                    "${img.prompt}"
                                </div>
                                <div style="display: flex; gap: 6px;">
                                    <button onclick="imageGenerator.saveImage('${img.imageData}', '${img.prompt.replace(/'/g, "\\'")}')" style="flex: 1; background: rgba(119, 221, 190, 0.15); border: none; color: var(--cursor-jade-dark); padding: 6px; border-radius: 4px; cursor: pointer; font-size: 10px;">
                                        💾 Save
                                    </button>
                                    <button onclick="imageGenerator.copyToClipboard('${img.imageData}')" style="flex: 1; background: rgba(119, 221, 190, 0.15); border: none; color: var(--cursor-jade-dark); padding: 6px; border-radius: 4px; cursor: pointer; font-size: 10px;">
                                        📋 Copy
                                    </button>
                                </div>
                            </div>
                        </div>
                    `).join('')
                }
            </div>
        `;
        
        document.body.appendChild(modal);
    }
    
    showNotification(message, type = 'info') {
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: ${type === 'error' ? '#ff4757' : type === 'success' ? '#77ddbe' : '#4a90e2'};
            color: white;
            padding: 12px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 20000;
            font-size: 13px;
            font-weight: 600;
            animation: slideInRight 0.3s ease-out;
        `;
        notification.textContent = message;
        document.body.appendChild(notification);
        
        setTimeout(() => notification.remove(), 3000);
    }
}

// Initialize
window.imageGenerator = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.imageGenerator = new ImageGenerator();
        window.imageGenerator.init();
    });
} else {
    window.imageGenerator = new ImageGenerator();
    window.imageGenerator.init();
}

// Export
window.ImageGenerator = ImageGenerator;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = ImageGenerator;
}

console.log('[ImageGen] 📦 Image generator module loaded');

})(); // End IIFE

