/**
 * Safe File System Operations - Windows Protected Directory Handler
 * Prevents EPERM errors when accessing system directories like Recycle Bin
 */

import * as fs from 'fs';
import * as path from 'path';

// Protected directories that should be skipped on Windows
const PROTECTED_DIRS = [
    '$RECYCLE.BIN',
    'System Volume Information',
    'Recovery',
    '$Recycle.Bin',
    'WindowsApps',
    'WpSystem',
    'MSOCache'
];

/**
 * Check if a path is a protected Windows directory
 */
export function isProtectedDirectory(dirPath: string): boolean {
    const normalized = path.normalize(dirPath).toLowerCase();
    return PROTECTED_DIRS.some(protected => 
        normalized.includes(protected.toLowerCase())
    );
}

/**
 * Safe readdir that skips protected directories
 */
export function safeReaddirSync(dirPath: string): string[] {
    // Skip protected directories
    if (isProtectedDirectory(dirPath)) {
        console.warn(`[SKIP] Protected directory: ${dirPath}`);
        return [];
    }

    try {
        return fs.readdirSync(dirPath);
    } catch (err: any) {
        if (err.code === 'EPERM' || err.code === 'EACCES') {
            console.warn(`[SKIP] Permission denied: ${dirPath}`);
            return [];
        }
        // Re-throw other errors
        throw err;
    }
}

/**
 * Safe readdir (async) that skips protected directories
 */
export async function safeReaddir(dirPath: string): Promise<string[]> {
    // Skip protected directories
    if (isProtectedDirectory(dirPath)) {
        console.warn(`[SKIP] Protected directory: ${dirPath}`);
        return [];
    }

    try {
        return await fs.promises.readdir(dirPath);
    } catch (err: any) {
        if (err.code === 'EPERM' || err.code === 'EACCES') {
            console.warn(`[SKIP] Permission denied: ${dirPath}`);
            return [];
        }
        // Re-throw other errors
        throw err;
    }
}

/**
 * Safe file stat that handles protected files
 */
export function safeStatSync(filePath: string): fs.Stats | null {
    if (isProtectedDirectory(filePath)) {
        return null;
    }

    try {
        return fs.statSync(filePath);
    } catch (err: any) {
        if (err.code === 'EPERM' || err.code === 'EACCES' || err.code === 'ENOENT') {
            return null;
        }
        throw err;
    }
}

/**
 * Recursively walk directory tree, skipping protected directories
 */
export function* walkDirectorySync(dirPath: string, maxDepth: number = 10): Generator<string> {
    if (maxDepth <= 0 || isProtectedDirectory(dirPath)) {
        return;
    }

    const entries = safeReaddirSync(dirPath);
    
    for (const entry of entries) {
        const fullPath = path.join(dirPath, entry);
        const stat = safeStatSync(fullPath);
        
        if (!stat) continue;
        
        if (stat.isDirectory()) {
            yield* walkDirectorySync(fullPath, maxDepth - 1);
        } else {
            yield fullPath;
        }
    }
}

/**
 * Example usage
 */
export function scanDriveExample() {
    console.log('Scanning D:\\ drive safely...');
    
    const files = safeReaddirSync('D:\\');
    console.log('Root directories:', files);
    
    // Walk through all files (with depth limit)
    let count = 0;
    for (const file of walkDirectorySync('D:\\', 3)) {
        count++;
        if (count <= 10) {
            console.log('Found:', file);
        }
    }
    console.log(`Total files found: ${count}`);
}
