/**
 * Fetch Timeout Wrapper with AbortController
 * Prevents hanging fetch requests by enforcing timeouts
 * Provides centralized fetch management with retry logic
 */

// CRITICAL: Save original fetch BEFORE anything else!
const originalFetch = typeof window !== 'undefined' ? window.fetch : (typeof global !== 'undefined' ? global.fetch : undefined);

class FetchTimeoutWrapper {
    constructor() {
        this.activeRequests = new Map();
        this.requestId = 0;
        this.defaultTimeout = 30000; // 30 seconds
        this.originalFetch = originalFetch; // Store reference to original
        this.stats = {
            total: 0,
            succeeded: 0,
            failed: 0,
            timedOut: 0,
            aborted: 0
        };
    }

    /**
     * Fetch with automatic timeout and abort capability
     * @param {string} url - URL to fetch
     * @param {object} options - Fetch options
     * @param {number} timeout - Timeout in milliseconds (default: 30000)
     * @returns {Promise<Response>}
     */
    async fetch(url, options = {}, timeout = this.defaultTimeout) {
        const requestId = ++this.requestId;
        const controller = new AbortController();
        const { signal } = controller;

        // Merge abort signal with existing options
        const fetchOptions = {
            ...options,
            signal
        };

        // Store request info for tracking
        const requestInfo = {
            id: requestId,
            url,
            controller,
            startTime: Date.now(),
            timeout
        };
        this.activeRequests.set(requestId, requestInfo);
        this.stats.total++;

        // Create timeout promise
        const timeoutPromise = new Promise((_, reject) => {
            requestInfo.timeoutId = setTimeout(() => {
                this.stats.timedOut++;
                controller.abort();
                reject(new Error(`Fetch timeout after ${timeout}ms: ${url}`));
            }, timeout);
        });

        try {
            // Race between fetch and timeout
            // CRITICAL: Use originalFetch to avoid infinite recursion!
            const fetchFn = this.originalFetch || originalFetch;
            if (!fetchFn) {
                throw new Error('Original fetch not available');
            }
            const response = await Promise.race([
                fetchFn(url, fetchOptions),
                timeoutPromise
            ]);

            // Clear timeout on success
            clearTimeout(requestInfo.timeoutId);
            this.stats.succeeded++;
            
            // Clean up
            this.activeRequests.delete(requestId);
            
            return response;
        } catch (error) {
            // Clear timeout on error
            clearTimeout(requestInfo.timeoutId);
            
            // Track abort vs other failures
            if (error.name === 'AbortError' || error.message.includes('aborted')) {
                this.stats.aborted++;
            } else if (!error.message.includes('timeout')) {
                this.stats.failed++;
            }
            
            // Clean up
            this.activeRequests.delete(requestId);
            
            throw error;
        }
    }

    /**
     * Fetch with retry logic
     * @param {string} url - URL to fetch
     * @param {object} options - Fetch options
     * @param {number} retries - Number of retry attempts (default: 3)
     * @param {number} timeout - Timeout per attempt in ms (default: 30000)
     * @returns {Promise<Response>}
     */
    async fetchWithRetry(url, options = {}, retries = 3, timeout = this.defaultTimeout) {
        let lastError;
        
        for (let attempt = 0; attempt <= retries; attempt++) {
            try {
                return await this.fetch(url, options, timeout);
            } catch (error) {
                lastError = error;
                
                // Don't retry on 4xx errors (client errors)
                if (error.status >= 400 && error.status < 500) {
                    throw error;
                }
                
                // Wait before retry (exponential backoff)
                if (attempt < retries) {
                    const delay = Math.min(1000 * Math.pow(2, attempt), 10000);
                    console.warn(`Fetch attempt ${attempt + 1} failed, retrying in ${delay}ms...`, error.message);
                    await new Promise(resolve => setTimeout(resolve, delay));
                }
            }
        }
        
        throw lastError;
    }

    /**
     * Abort all active requests
     */
    abortAll() {
        console.log(`Aborting ${this.activeRequests.size} active requests...`);
        this.activeRequests.forEach(request => {
            try {
                request.controller.abort();
                clearTimeout(request.timeoutId);
            } catch (err) {
                console.error('Error aborting request:', err);
            }
        });
        this.activeRequests.clear();
    }

    /**
     * Abort specific request by URL pattern
     * @param {string|RegExp} pattern - URL pattern to match
     */
    abortByUrl(pattern) {
        const regex = pattern instanceof RegExp ? pattern : new RegExp(pattern);
        let aborted = 0;
        
        this.activeRequests.forEach((request, id) => {
            if (regex.test(request.url)) {
                try {
                    request.controller.abort();
                    clearTimeout(request.timeoutId);
                    this.activeRequests.delete(id);
                    aborted++;
                } catch (err) {
                    console.error('Error aborting request:', err);
                }
            }
        });
        
        console.log(`Aborted ${aborted} requests matching pattern:`, pattern);
        return aborted;
    }

    /**
     * Get statistics about fetch requests
     */
    getStats() {
        const active = this.activeRequests.size;
        const successRate = this.stats.total > 0 
            ? ((this.stats.succeeded / this.stats.total) * 100).toFixed(2) 
            : '0.00';
        
        return {
            ...this.stats,
            active,
            successRate: `${successRate}%`,
            activeRequests: Array.from(this.activeRequests.values()).map(req => ({
                id: req.id,
                url: req.url,
                duration: Date.now() - req.startTime,
                timeout: req.timeout
            }))
        };
    }

    /**
     * Reset statistics
     */
    resetStats() {
        this.stats = {
            total: 0,
            succeeded: 0,
            failed: 0,
            timedOut: 0,
            aborted: 0
        };
    }

    /**
     * Set default timeout for all requests
     * @param {number} timeout - Timeout in milliseconds
     */
    setDefaultTimeout(timeout) {
        this.defaultTimeout = timeout;
        console.log(`Default fetch timeout set to ${timeout}ms`);
    }
}

// Create global instance
const fetchWrapper = new FetchTimeoutWrapper();

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = fetchWrapper;
}

// Make available globally in renderer
if (typeof window !== 'undefined') {
    window.fetchWrapper = fetchWrapper;
    window.originalFetch = originalFetch; // Expose original
    
    // Override native fetch with wrapped version
    window.fetch = function(url, options) {
        // Use wrapper by default, but allow bypass with special flag
        if (options && options.__bypassWrapper) {
            delete options.__bypassWrapper;
            return originalFetch(url, options);
        }
        return fetchWrapper.fetch(url, options);
    };
    
    // Add global helper functions
    window.fetchWithRetry = (url, options, retries, timeout) => 
        fetchWrapper.fetchWithRetry(url, options, retries, timeout);
    
    window.abortAllFetches = () => fetchWrapper.abortAll();
    window.getFetchStats = () => fetchWrapper.getStats();
    
    console.log('✅ Fetch timeout wrapper installed - All fetch calls now have 30s timeout by default');
}

// Cleanup on page unload
if (typeof window !== 'undefined') {
    window.addEventListener('beforeunload', () => {
        console.log('Cleaning up fetch requests...');
        fetchWrapper.abortAll();
    });
}

