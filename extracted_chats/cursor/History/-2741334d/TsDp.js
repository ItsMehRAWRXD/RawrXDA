/**
 * GenesisOS Network Driver
 * Network driver for the kernel
 */

export class NetworkDriver {
    constructor() {
        this.initialized = false;
    }

    async initialize() {
        if (this.initialized) return;

        // Check for network capabilities
        if (typeof fetch === 'function') {
            console.log('✅ Network Driver initialized with Fetch API');
        } else {
            console.warn('⚠️ Fetch API not available');
        }

        this.initialized = true;
    }

    async fetch(url, options = {}) {
        await this.initialize();

        try {
            const response = await fetch(url, {
                method: options.method || 'GET',
                headers: options.headers || {},
                body: options.body || null,
                ...options
            });

            return {
                ok: response.ok,
                status: response.status,
                statusText: response.statusText,
                headers: Object.fromEntries(response.headers.entries()),
                data: await response.text()
            };
        } catch (error) {
            throw new Error(`Network request failed: ${error.message}`);
        }
    }

    async get(url, options = {}) {
        return this.fetch(url, { ...options, method: 'GET' });
    }

    async post(url, data, options = {}) {
        return this.fetch(url, {
            ...options,
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            body: JSON.stringify(data)
        });
    }

    async put(url, data, options = {}) {
        return this.fetch(url, {
            ...options,
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            body: JSON.stringify(data)
        });
    }

    async delete(url, options = {}) {
        return this.fetch(url, { ...options, method: 'DELETE' });
    }
}
