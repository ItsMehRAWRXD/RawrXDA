package com.rawr.mobilecopilot.security

import android.content.Context
import android.util.Log
import java.io.File
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/**
 * Secure sandbox for isolating and protecting mobile copilot operations
 * Implements defense-in-depth security architecture
 */
class SecureSandbox(private val context: Context) {
    
    companion object {
        private const val TAG = "SecureSandbox"
        private const val AES_KEY_SIZE = 256
        private const val GCM_IV_SIZE = 12
        private const val GCM_TAG_SIZE = 16
        private const val SANDBOX_DIR_NAME = "secure_sandbox"
    }
    
    private val secureRandom = SecureRandom()
    private val sandboxDirectory: File
    
    init {
        sandboxDirectory = File(context.filesDir, SANDBOX_DIR_NAME)
        if (!sandboxDirectory.exists()) {
            sandboxDirectory.mkdirs()
            Log.i(TAG, "Created secure sandbox directory: ${sandboxDirectory.absolutePath}")
        }
    }
    
    /**
     * Validate file operation within sandbox
     */
    fun validateFileOperation(filePath: String, operation: FileOperation): SandboxValidationResult {
        val file = File(filePath)
        
        // Check if file is within sandbox
        if (!isWithinSandbox(file)) {
            Log.w(TAG, "File operation denied - outside sandbox: $filePath")
            return SandboxValidationResult.DENIED_OUTSIDE_SANDBOX
        }
        
        // Check operation-specific permissions
        return when (operation) {
            FileOperation.READ -> {
                if (file.exists() && file.canRead()) {
                    SandboxValidationResult.ALLOWED
                } else {
                    SandboxValidationResult.DENIED_READ_PERMISSION
                }
            }
            FileOperation.WRITE -> {
                if (file.parentFile?.canWrite() == true) {
                    SandboxValidationResult.ALLOWED
                } else {
                    SandboxValidationResult.DENIED_WRITE_PERMISSION
                }
            }
            FileOperation.EXECUTE -> {
                // Execution not allowed in sandbox for security
                Log.w(TAG, "Execution denied in sandbox: $filePath")
                SandboxValidationResult.DENIED_EXECUTION_NOT_ALLOWED
            }
        }
    }
    
    /**
     * Check if file is within sandbox boundaries
     */
    private fun isWithinSandbox(file: File): Boolean {
        return try {
            val canonicalFile = file.canonicalFile
            val canonicalSandbox = sandboxDirectory.canonicalFile
            canonicalFile.absolutePath.startsWith(canonicalSandbox.absolutePath)
        } catch (e: Exception) {
            Log.e(TAG, "Error checking sandbox boundaries", e)
            false
        }
    }
    
    /**
     * Create secure file within sandbox
     */
    fun createSecureFile(fileName: String): File? {
        return try {
            val sanitizedName = sanitizeFileName(fileName)
            val secureFile = File(sandboxDirectory, sanitizedName)
            
            if (validateFileOperation(secureFile.absolutePath, FileOperation.WRITE) 
                == SandboxValidationResult.ALLOWED) {
                secureFile
            } else {
                null
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error creating secure file: $fileName", e)
            null
        }
    }
    
    /**
     * Sanitize file name to prevent path traversal attacks
     */
    private fun sanitizeFileName(fileName: String): String {
        return fileName
            .replace("..", "_")
            .replace("/", "_")
            .replace("\\", "_")
            .replace("\u0000", "_")
            .take(255) // Limit filename length
    }
    
    /**
     * Encrypt data before writing to sandbox
     */
    fun encryptData(data: ByteArray, key: SecretKey): EncryptedData? {
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            val iv = ByteArray(GCM_IV_SIZE)
            secureRandom.nextBytes(iv)
            val gcmSpec = GCMParameterSpec(GCM_TAG_SIZE * 8, iv)
            
            cipher.init(Cipher.ENCRYPT_MODE, key, gcmSpec)
            val encryptedBytes = cipher.doFinal(data)
            
            EncryptedData(encryptedBytes, iv)
        } catch (e: Exception) {
            Log.e(TAG, "Error encrypting data", e)
            null
        }
    }
    
    /**
     * Decrypt data read from sandbox
     */
    fun decryptData(encryptedData: EncryptedData, key: SecretKey): ByteArray? {
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            val gcmSpec = GCMParameterSpec(GCM_TAG_SIZE * 8, encryptedData.iv)
            
            cipher.init(Cipher.DECRYPT_MODE, key, gcmSpec)
            cipher.doFinal(encryptedData.data)
        } catch (e: Exception) {
            Log.e(TAG, "Error decrypting data", e)
            null
        }
    }
    
    /**
     * Generate secure encryption key
     */
    fun generateSecureKey(): SecretKey {
        val keyGenerator = KeyGenerator.getInstance("AES")
        keyGenerator.init(AES_KEY_SIZE, secureRandom)
        return keyGenerator.generateKey()
    }
    
    /**
     * Compute secure hash of data
     */
    fun computeHash(data: ByteArray): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val hashBytes = digest.digest(data)
        return hashBytes.joinToString("") { "%02x".format(it) }
    }
    
    /**
     * Verify data integrity using hash
     */
    fun verifyIntegrity(data: ByteArray, expectedHash: String): Boolean {
        val actualHash = computeHash(data)
        return actualHash.equals(expectedHash, ignoreCase = true)
    }
    
    /**
     * Clean up sandbox (remove all files)
     */
    fun cleanupSandbox() {
        try {
            sandboxDirectory.listFiles()?.forEach { file ->
                if (file.isFile) {
                    file.delete()
                    Log.d(TAG, "Deleted sandbox file: ${file.name}")
                }
            }
            Log.i(TAG, "Sandbox cleanup completed")
        } catch (e: Exception) {
            Log.e(TAG, "Error during sandbox cleanup", e)
        }
    }
    
    /**
     * Get sandbox directory path
     */
    fun getSandboxPath(): String = sandboxDirectory.absolutePath
}

/**
 * File operations enum
 */
enum class FileOperation {
    READ, WRITE, EXECUTE
}

/**
 * Sandbox validation results
 */
enum class SandboxValidationResult {
    ALLOWED,
    DENIED_OUTSIDE_SANDBOX,
    DENIED_READ_PERMISSION,
    DENIED_WRITE_PERMISSION,
    DENIED_EXECUTION_NOT_ALLOWED
}

/**
 * Data class for encrypted data
 */
data class EncryptedData(
    val data: ByteArray,
    val iv: ByteArray
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        
        other as EncryptedData
        
        if (!data.contentEquals(other.data)) return false
        if (!iv.contentEquals(other.iv)) return false
        
        return true
    }
    
    override fun hashCode(): Int {
        var result = data.contentHashCode()
        result = 31 * result + iv.contentHashCode()
        return result
    }
}