package com.rawr.mobilecopilot.security

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Environment
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume

/**
 * Secure permission manager for mobile copilot
 * Handles runtime permissions with proper security checks
 */
class SecurePermissionManager(private val context: Context) {
    
    companion object {
        const val PERMISSION_REQUEST_CODE = 1001
        
        // Critical permissions for mobile copilot
        val REQUIRED_PERMISSIONS = arrayOf(
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.WAKE_LOCK,
            Manifest.permission.BATTERY_STATS
        )
        
        val DANGEROUS_PERMISSIONS = arrayOf(
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.ACCESS_FINE_LOCATION
        )
        
        // Android 11+ scoped storage permissions
        val ANDROID_11_PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            arrayOf(Manifest.permission.MANAGE_EXTERNAL_STORAGE)
        } else {
            emptyArray()
        }
    }
    
    /**
     * Check if all required permissions are granted
     */
    fun hasAllRequiredPermissions(): Boolean {
        return REQUIRED_PERMISSIONS.all { permission ->
            ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
        }
    }
    
    /**
     * Check if a specific permission is granted
     */
    fun hasPermission(permission: String): Boolean {
        return ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
    }
    
    /**
     * Get list of missing permissions
     */
    fun getMissingPermissions(): List<String> {
        return REQUIRED_PERMISSIONS.filter { permission ->
            ContextCompat.checkSelfPermission(context, permission) != PackageManager.PERMISSION_GRANTED
        }
    }
    
    /**
     * Request permissions from user
     */
    fun requestPermissions(activity: Activity, permissions: Array<String> = REQUIRED_PERMISSIONS) {
        ActivityCompat.requestPermissions(activity, permissions, PERMISSION_REQUEST_CODE)
    }
    
    /**
     * Suspend function to request permissions and wait for result
     */
    suspend fun requestPermissionsAsync(activity: Activity, permissions: Array<String> = REQUIRED_PERMISSIONS): Boolean {
        if (permissions.all { hasPermission(it) }) {
            return true
        }
        
        return suspendCancellableCoroutine { continuation ->
            // Store continuation for later use in permission result callback
            PermissionResultHandler.storeContinuation(PERMISSION_REQUEST_CODE, continuation)
            ActivityCompat.requestPermissions(activity, permissions, PERMISSION_REQUEST_CODE)
        }
    }
    
    /**
     * Check if we should show rationale for permission
     */
    fun shouldShowRationale(activity: Activity, permission: String): Boolean {
        return ActivityCompat.shouldShowRequestPermissionRationale(activity, permission)
    }
    
    /**
     * Validate file access permissions for specific path
     */
    fun validateFileAccess(filePath: String): SecurityValidationResult {
        // Check if path is within allowed directories
        val allowedDirectories = getAllowedDirectories()
        val isPathAllowed = allowedDirectories.any { allowedDir ->
            filePath.startsWith(allowedDir)
        }
        
        if (!isPathAllowed) {
            return SecurityValidationResult.DENIED_INVALID_PATH
        }
        
        // Check file permissions
        if (!hasPermission(Manifest.permission.READ_EXTERNAL_STORAGE)) {
            return SecurityValidationResult.DENIED_NO_PERMISSION
        }
        
        return SecurityValidationResult.ALLOWED
    }
    
    /**
     * Get list of allowed directories for file operations
     */
    private fun getAllowedDirectories(): List<String> {
        val allowedDirs = mutableListOf<String>()
        
        // App-specific directories (always allowed)
        context.filesDir?.let { allowedDirs.add(it.absolutePath) }
        context.cacheDir?.let { allowedDirs.add(it.absolutePath) }
        context.getExternalFilesDir(null)?.let { allowedDirs.add(it.absolutePath) }
        context.externalCacheDir?.let { allowedDirs.add(it.absolutePath) }
        
        // Public directories (with permissions)
        if (hasPermission(Manifest.permission.READ_EXTERNAL_STORAGE)) {
            Environment.getExternalStorageDirectory()?.let { allowedDirs.add(it.absolutePath) }
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)?.let { 
                allowedDirs.add(it.absolutePath) 
            }
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)?.let { 
                allowedDirs.add(it.absolutePath) 
            }
        }
        
        return allowedDirs
    }
}

/**
 * Security validation results
 */
enum class SecurityValidationResult {
    ALLOWED,
    DENIED_NO_PERMISSION,
    DENIED_INVALID_PATH,
    DENIED_SECURITY_VIOLATION
}

/**
 * Helper object to handle permission result callbacks
 */
object PermissionResultHandler {
    private val continuations = mutableMapOf<Int, kotlin.coroutines.Continuation<Boolean>>()
    
    fun storeContinuation(requestCode: Int, continuation: kotlin.coroutines.Continuation<Boolean>) {
        continuations[requestCode] = continuation
    }
    
    fun handlePermissionResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        val continuation = continuations.remove(requestCode)
        if (continuation != null) {
            val allGranted = grantResults.all { it == PackageManager.PERMISSION_GRANTED }
            continuation.resume(allGranted)
        }
    }
}