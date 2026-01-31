// RawrZ Memory Manager - Simple memory management utility
const { logger } = require('./logger');

class MemoryManager {
    constructor() {
        this.collections = new Map();
        this.maxCollections = 100000; // WIDE OPEN - Maximum collection limit
        this.maxCollectionSize = 1000000; // WIDE OPEN - Maximum collection size limit
    }

    createManagedCollection(name, type = 'Map', maxSize = 1000000) { // WIDE OPEN - Maximum default max size
        if (this.collections.has(name)) {
            return this.collections.get(name);
        }

        let collection;
        switch (type) {
            case 'Map':
                collection = new Map();
                break;
            case 'Set':
                collection = new Set();
                break;
            case 'Array':
                collection = [];
                break;
            default:
                collection = new Map();
        }

        // Add size management
        const originalSet = collection.set;
        const originalAdd = collection.add;
        const originalPush = collection.push;

        if (type === 'Map') {
            collection.set = (key, value) => {
                if (collection.size >= maxSize) {
                    const firstKey = collection.keys().next().value;
                    collection.delete(firstKey);
                }
                return originalSet.call(collection, key, value);
            };
        } else if (type === 'Set') {
            collection.add = (value) => {
                if (collection.size >= maxSize) {
                    const firstValue = collection.values().next().value;
                    collection.delete(firstValue);
                }
                return originalAdd.call(collection, value);
            };
        } else if (type === 'Array') {
            collection.push = (value) => {
                if (collection.length >= maxSize) {
                    collection.shift();
                }
                return originalPush.call(collection, value);
            };
        }

        this.collections.set(name, collection);
        return collection;
    }

    getCollection(name) {
        return this.collections.get(name);
    }

    deleteCollection(name) {
        return this.collections.delete(name);
    }

    cleanup() {
        for (const [name, collection] of this.collections) {
            if (collection instanceof Map) {
                collection.clear();
            } else if (collection instanceof Set) {
                collection.clear();
            } else if (Array.isArray(collection)) {
                collection.length = 0;
            }
        }
        this.collections.clear();
        logger.info('Memory manager cleaned up all collections');
    }

    getStatus() {
        const status = {
            totalCollections: this.collections.size,
            collections: {}
        };

        for (const [name, collection] of this.collections) {
            if (collection instanceof Map) {
                status.collections[name] = { type: 'Map', size: collection.size };
            } else if (collection instanceof Set) {
                status.collections[name] = { type: 'Set', size: collection.size };
            } else if (Array.isArray(collection)) {
                status.collections[name] = { type: 'Array', size: collection.length };
            }
        }

        return status;
    }
}

// Create singleton instance
const memoryManager = new MemoryManager();

function getMemoryManager() {
    return memoryManager;
}

module.exports = { getMemoryManager, MemoryManager };
