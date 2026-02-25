/**
 * GlyphCanvas - Real-time code visualization and background rendering system
 * Provides live code wallpaper, glyph streams, and agent activity visualization
 */

class GlyphCanvas {
    constructor(containerId = 'glyph-canvas') {
        this.container = document.getElementById(containerId);
        if (!this.container) {
            this.container = document.createElement('div');
            this.container.id = containerId;
            document.body.appendChild(this.container);
        }

        this.canvas = document.createElement('canvas');
        this.ctx = this.canvas.getContext('2d');
        this.container.appendChild(this.canvas);

        this.glyphs = [];
        this.codeStreams = [];
        this.agentTrails = [];
        this.particles = [];
        this.animationFrame = null;

        this.setupCanvas();
        this.setupShaders();
        this.startRenderLoop();

        console.log('🎨 GlyphCanvas initialized');
    }

    /**
     * Set up canvas dimensions and styling
     */
    setupCanvas() {
        const resizeCanvas = () => {
            this.canvas.width = window.innerWidth;
            this.canvas.height = window.innerHeight;
            this.canvas.style.position = 'fixed';
            this.canvas.style.top = '0';
            this.canvas.style.left = '0';
            this.canvas.style.zIndex = '-1';
            this.canvas.style.pointerEvents = 'none';
        };

        resizeCanvas();
        window.addEventListener('resize', resizeCanvas);

        // Set up canvas context
        this.ctx.fillStyle = '#0a0a0f';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }

    /**
     * Initialize WebGL shaders for advanced effects
     */
    setupShaders() {
        // For now, we'll use 2D canvas. WebGL can be added later for more advanced effects
        this.shaderEffects = {
            entropyGlow: (x, y, intensity) => {
                const gradient = this.ctx.createRadialGradient(x, y, 0, x, y, 50);
                gradient.addColorStop(0, `rgba(0, 255, 255, ${intensity})`);
                gradient.addColorStop(1, 'rgba(0, 255, 255, 0)');
                this.ctx.fillStyle = gradient;
                this.ctx.beginPath();
                this.ctx.arc(x, y, 50, 0, Math.PI * 2);
                this.ctx.fill();
            },

            syncTrail: (startX, startY, endX, endY, opacity) => {
                this.ctx.strokeStyle = `rgba(0, 255, 255, ${opacity})`;
                this.ctx.lineWidth = 2;
                this.ctx.beginPath();
                this.ctx.moveTo(startX, startY);
                this.ctx.lineTo(endX, endY);
                this.ctx.stroke();
            }
        };
    }

    /**
     * Start the main render loop
     */
    startRenderLoop() {
        const render = () => {
            this.update();
            this.draw();
            this.animationFrame = requestAnimationFrame(render);
        };
        render();
    }

    /**
     * Update glyph positions and states
     */
    update() {
        const now = Date.now();

        // Update glyphs
        this.glyphs = this.glyphs.filter(glyph => {
            glyph.age += 16; // Approximate 60fps
            glyph.y += glyph.speed;

            // Remove old glyphs
            return glyph.age < glyph.lifetime && glyph.y < this.canvas.height + 100;
        });

        // Update code streams
        this.codeStreams = this.codeStreams.filter(stream => {
            stream.age += 16;
            return stream.age < stream.lifetime;
        });

        // Update agent trails
        this.agentTrails = this.agentTrails.filter(trail => {
            trail.age += 16;
            trail.opacity = Math.max(0, trail.opacity - 0.01);
            return trail.opacity > 0;
        });

        // Update particles
        this.particles = this.particles.filter(particle => {
            particle.x += particle.vx;
            particle.y += particle.vy;
            particle.life -= 0.02;
            return particle.life > 0;
        });
    }

    /**
     * Draw all visual elements
     */
    draw() {
        // Clear canvas with fade effect
        this.ctx.fillStyle = 'rgba(10, 10, 15, 0.1)';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

        // Draw code streams (background layer)
        this.drawCodeStreams();

        // Draw agent trails
        this.drawAgentTrails();

        // Draw glyphs
        this.drawGlyphs();

        // Draw particles
        this.drawParticles();

        // Draw entropy effects
        this.drawEntropyEffects();
    }

    /**
     * Draw floating code streams in background
     */
    drawCodeStreams() {
        this.ctx.fillStyle = 'rgba(0, 255, 255, 0.03)';
        this.ctx.font = '12px monospace';

        this.codeStreams.forEach(stream => {
            const alpha = 1 - (stream.age / stream.lifetime);
            this.ctx.globalAlpha = alpha;

            this.ctx.fillText(
                stream.code,
                stream.x,
                stream.y + (stream.age * 0.5)
            );
        });

        this.ctx.globalAlpha = 1;
    }

    /**
     * Draw agent sync trails
     */
    drawAgentTrails() {
        this.agentTrails.forEach(trail => {
            this.shaderEffects.syncTrail(
                trail.startX, trail.startY,
                trail.endX, trail.endY,
                trail.opacity
            );
        });
    }

    /**
     * Draw floating glyphs
     */
    drawGlyphs() {
        this.glyphs.forEach(glyph => {
            const alpha = 1 - (glyph.age / glyph.lifetime);
            this.ctx.globalAlpha = alpha;

            // Glyph styling based on type
            if (glyph.type === 'agent') {
                this.ctx.fillStyle = glyph.color;
                this.ctx.font = '14px monospace';
                this.ctx.fillText(glyph.text, glyph.x, glyph.y);
            } else if (glyph.type === 'code') {
                this.ctx.fillStyle = 'rgba(0, 255, 0, 0.6)';
                this.ctx.font = '10px monospace';
                this.ctx.fillText(glyph.text, glyph.x, glyph.y);
            }

            // Add glow effect for high entropy
            if (glyph.entropy > 0.7) {
                this.shaderEffects.entropyGlow(glyph.x, glyph.y, glyph.entropy);
            }
        });

        this.ctx.globalAlpha = 1;
    }

    /**
     * Draw particle effects
     */
    drawParticles() {
        this.particles.forEach(particle => {
            this.ctx.fillStyle = `rgba(0, 255, 255, ${particle.life})`;
            this.ctx.beginPath();
            this.ctx.arc(particle.x, particle.y, particle.size, 0, Math.PI * 2);
            this.ctx.fill();
        });
    }

    /**
     * Draw entropy-based visual effects
     */
    drawEntropyEffects() {
        // Draw background pulse based on overall entropy
        const avgEntropy = this.calculateAverageEntropy();
        if (avgEntropy > 0.5) {
            this.ctx.fillStyle = `rgba(0, 255, 255, ${avgEntropy * 0.02})`;
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        }
    }

    /**
     * Calculate average entropy across all glyphs
     */
    calculateAverageEntropy() {
        if (this.glyphs.length === 0) return 0;

        const totalEntropy = this.glyphs.reduce((sum, glyph) => sum + glyph.entropy, 0);
        return totalEntropy / this.glyphs.length;
    }

    /**
     * Add a new glyph to the visualization
     */
    addGlyph(text, type = 'agent', entropy = 0.5, x = null, y = null) {
        const glyph = {
            text,
            type,
            entropy,
            x: x || Math.random() * (this.canvas.width - 200),
            y: y || -50,
            speed: 0.5 + (entropy * 2),
            age: 0,
            lifetime: 10000 + (entropy * 5000),
            color: this.getEntropyColor(entropy, type)
        };

        this.glyphs.push(glyph);

        // Add particles for high entropy glyphs
        if (entropy > 0.7) {
            this.createParticles(glyph.x, glyph.y, 5);
        }

        // Keep only recent glyphs
        if (this.glyphs.length > 100) {
            this.glyphs = this.glyphs.slice(-100);
        }
    }

    /**
     * Add a code stream to the background
     */
    addCodeStream(code, x = null, y = null) {
        const stream = {
            code,
            x: x || Math.random() * (this.canvas.width - 300),
            y: y || Math.random() * this.canvas.height,
            age: 0,
            lifetime: 30000
        };

        this.codeStreams.push(stream);

        // Keep only recent streams
        if (this.codeStreams.length > 20) {
            this.codeStreams = this.codeStreams.slice(-20);
        }
    }

    /**
     * Add an agent sync trail
     */
    addSyncTrail(startX, startY, endX, endY, opacity = 0.5) {
        const trail = {
            startX,
            startY,
            endX,
            endY,
            opacity,
            age: 0,
            lifetime: 5000
        };

        this.agentTrails.push(trail);
    }

    /**
     * Create particle burst effect
     */
    createParticles(x, y, count = 10) {
        for (let i = 0; i < count; i++) {
            const particle = {
                x: x + (Math.random() - 0.5) * 20,
                y: y + (Math.random() - 0.5) * 20,
                vx: (Math.random() - 0.5) * 2,
                vy: (Math.random() - 0.5) * 2,
                life: 1.0,
                size: Math.random() * 3 + 1
            };

            this.particles.push(particle);
        }
    }

    /**
     * Get color based on entropy and glyph type
     */
    getEntropyColor(entropy, type) {
        if (type === 'agent') {
            if (entropy > 0.8) return '#ff4444'; // High entropy - red
            if (entropy > 0.6) return '#ffaa00'; // Medium entropy - orange
            return '#00ffff'; // Low entropy - cyan
        } else if (type === 'code') {
            if (entropy > 0.7) return '#00ff00'; // High entropy code - green
            return '#0088ff'; // Normal code - blue
        }

        return '#ffffff'; // Default white
    }

    /**
     * Set background image (for the provided base64 image)
     */
    setBackgroundImage(base64Image) {
        const img = new Image();
        img.onload = () => {
            this.backgroundImage = img;
            console.log('🎨 Code background image loaded');
        };
        img.src = base64Image;
    }

    /**
     * Render agent activity from SwarmAnalytics
     */
    render(data) {
        // Add glyph for agent activity
        this.addGlyph(
            `${data.agent}: ${data.intent} → ${data.result}`,
            'agent',
            data.entropy || 0.5
        );

        // Add code stream if it's code-related
        if (data.result && data.result.includes('function') || data.result.includes('class')) {
            this.addCodeStream(data.result.substring(0, 100));
        }

        // Add sync trail if twinsie sync is mentioned
        if (data.twinsieSync && data.twinsieSync.length > 0) {
            // Find agent positions for trail (simplified)
            const startX = Math.random() * this.canvas.width;
            const startY = Math.random() * this.canvas.height;

            data.twinsieSync.forEach((_, index) => {
                const endX = Math.random() * this.canvas.width;
                const endY = Math.random() * this.canvas.height;
                this.addSyncTrail(startX, startY, endX, endY, 0.3);
            });
        }

        // Create particle burst for high entropy events
        if (data.entropy > 0.8) {
            this.createParticles(
                Math.random() * this.canvas.width,
                Math.random() * this.canvas.height,
                15
            );
        }
    }

    /**
     * Clean up resources
     */
    destroy() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
        }
        if (this.container && this.canvas) {
            this.container.removeChild(this.canvas);
        }
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = GlyphCanvas;
}

// Global access for browser
if (typeof window !== 'undefined') {
    window.GlyphCanvas = GlyphCanvas;
}
