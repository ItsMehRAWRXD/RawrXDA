package com.rawr.mobilecopilot.core

/**
 * Native memory mapping for high-performance file access
 * Uses mmap for zero-copy file operations
 */
class MemoryMapper {
    
    companion object {
        init {
            System.loadLibrary("native-fileio")
        }
    }
    
    /**
     * Map file into memory
     * @param path Absolute path to the file
     * @param readOnly Whether to map as read-only
     * @return Mapping pointer or 0 on error
     */
    external fun nativeMapFile(path: String, readOnly: Boolean): Long
    
    /**
     * Get size of mapped memory
     * @param mappingPtr Mapping pointer
     * @return Size in bytes
     */
    external fun nativeGetMappedSize(mappingPtr: Long): Long
    
    /**
     * Read bytes from mapped memory
     * @param mappingPtr Mapping pointer
     * @param offset Offset in bytes
     * @param length Number of bytes to read
     * @return Byte array with data
     */
    external fun nativeReadMappedBytes(mappingPtr: Long, offset: Long, length: Int): ByteArray?
    
    /**
     * Write bytes to mapped memory
     * @param mappingPtr Mapping pointer
     * @param offset Offset in bytes
     * @param data Data to write
     */
    external fun nativeWriteMappedBytes(mappingPtr: Long, offset: Long, data: ByteArray)
    
    /**
     * Unmap file from memory
     * @param mappingPtr Mapping pointer
     */
    external fun nativeUnmapFile(mappingPtr: Long)
}

/**
 * High-level memory-mapped file wrapper
 */
class MappedFile(private val path: String, private val readOnly: Boolean = true) : AutoCloseable {
    private val mapper = MemoryMapper()
    private var mappingPtr: Long = 0
    private var size: Long = 0
    
    init {
        mappingPtr = mapper.nativeMapFile(path, readOnly)
        if (mappingPtr != 0L) {
            size = mapper.nativeGetMappedSize(mappingPtr)
        } else {
            throw RuntimeException("Failed to map file: $path")
        }
    }
    
    /**
     * Get the size of the mapped file
     */
    fun getSize(): Long = size
    
    /**
     * Read bytes from the mapped file
     * @param offset Offset in the file
     * @param length Number of bytes to read
     * @return Byte array with data or null on error
     */
    fun readBytes(offset: Long, length: Int): ByteArray? {
        if (offset + length > size) return null
        return mapper.nativeReadMappedBytes(mappingPtr, offset, length)
    }
    
    /**
     * Write bytes to the mapped file (only if not read-only)
     * @param offset Offset in the file
     * @param data Data to write
     */
    fun writeBytes(offset: Long, data: ByteArray) {
        if (readOnly) throw IllegalStateException("Cannot write to read-only mapped file")
        if (offset + data.size > size) throw IllegalArgumentException("Write would exceed file bounds")
        
        mapper.nativeWriteMappedBytes(mappingPtr, offset, data)
    }
    
    /**
     * Read the entire file as bytes
     */
    fun readAll(): ByteArray? {
        return if (size > Int.MAX_VALUE) {
            null // File too large for single array
        } else {
            readBytes(0, size.toInt())
        }
    }
    
    /**
     * Stream read the file in chunks
     * @param chunkSize Size of each chunk
     * @param onChunk Callback for each chunk
     */
    fun streamRead(chunkSize: Int = 1024 * 1024, onChunk: (ByteArray, Long) -> Unit) {
        var offset = 0L
        
        while (offset < size) {
            val remainingBytes = (size - offset).toInt()
            val currentChunkSize = minOf(chunkSize, remainingBytes)
            
            val chunk = readBytes(offset, currentChunkSize)
            if (chunk != null) {
                onChunk(chunk, offset)
            }
            
            offset += currentChunkSize
        }
    }
    
    override fun close() {
        if (mappingPtr != 0L) {
            mapper.nativeUnmapFile(mappingPtr)
            mappingPtr = 0
            size = 0
        }
    }
}