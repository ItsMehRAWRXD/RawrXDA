package com.rawr.mobilecopilot.core

/**
 * Native file manager for high-performance file I/O operations
 * Uses native POSIX APIs for maximum performance
 */
class NativeFileManager {
    
    companion object {
        init {
            System.loadLibrary("native-fileio")
        }
        
        const val MODE_READ = 0
        const val MODE_WRITE = 1
        const val MODE_READ_WRITE = 2
    }
    
    /**
     * Open a file with specified mode
     * @param path Absolute path to the file
     * @param mode File access mode (MODE_READ, MODE_WRITE, MODE_READ_WRITE)
     * @return File descriptor or -1 on error
     */
    external fun nativeOpenFile(path: String, mode: Int): Long
    
    /**
     * Read data from file
     * @param fd File descriptor
     * @param buffer Buffer to read into
     * @param offset Offset in buffer
     * @param length Number of bytes to read
     * @return Number of bytes read or -1 on error
     */
    external fun nativeReadFile(fd: Long, buffer: ByteArray, offset: Int, length: Int): Int
    
    /**
     * Write data to file
     * @param fd File descriptor
     * @param buffer Buffer to write from
     * @param offset Offset in buffer
     * @param length Number of bytes to write
     * @return Number of bytes written or -1 on error
     */
    external fun nativeWriteFile(fd: Long, buffer: ByteArray, offset: Int, length: Int): Int
    
    /**
     * Close file descriptor
     * @param fd File descriptor to close
     */
    external fun nativeCloseFile(fd: Long)
    
    /**
     * Get file size
     * @param path Absolute path to the file
     * @return File size in bytes or -1 on error
     */
    external fun nativeGetFileSize(path: String): Long
}

/**
 * High-level file operations wrapper
 */
class HighPerformanceFileManager {
    private val nativeManager = NativeFileManager()
    
    /**
     * Read entire file into byte array using native I/O
     */
    fun readFileBytes(path: String): ByteArray? {
        val fileSize = nativeManager.nativeGetFileSize(path)
        if (fileSize <= 0) return null
        
        val fd = nativeManager.nativeOpenFile(path, NativeFileManager.MODE_READ)
        if (fd == -1L) return null
        
        return try {
            val buffer = ByteArray(fileSize.toInt())
            val bytesRead = nativeManager.nativeReadFile(fd, buffer, 0, fileSize.toInt())
            if (bytesRead == fileSize.toInt()) buffer else null
        } finally {
            nativeManager.nativeCloseFile(fd)
        }
    }
    
    /**
     * Write byte array to file using native I/O
     */
    fun writeFileBytes(path: String, data: ByteArray): Boolean {
        val fd = nativeManager.nativeOpenFile(path, NativeFileManager.MODE_WRITE)
        if (fd == -1L) return false
        
        return try {
            val bytesWritten = nativeManager.nativeWriteFile(fd, data, 0, data.size)
            bytesWritten == data.size
        } finally {
            nativeManager.nativeCloseFile(fd)
        }
    }
    
    /**
     * Stream file reading with callback
     */
    fun streamReadFile(path: String, chunkSize: Int = 8192, onChunk: (ByteArray, Int) -> Unit): Boolean {
        val fd = nativeManager.nativeOpenFile(path, NativeFileManager.MODE_READ)
        if (fd == -1L) return false
        
        return try {
            val buffer = ByteArray(chunkSize)
            var totalRead = 0
            
            while (true) {
                val bytesRead = nativeManager.nativeReadFile(fd, buffer, 0, chunkSize)
                if (bytesRead <= 0) break
                
                onChunk(buffer, bytesRead)
                totalRead += bytesRead
                
                if (bytesRead < chunkSize) break // EOF
            }
            
            true
        } finally {
            nativeManager.nativeCloseFile(fd)
        }
    }
}