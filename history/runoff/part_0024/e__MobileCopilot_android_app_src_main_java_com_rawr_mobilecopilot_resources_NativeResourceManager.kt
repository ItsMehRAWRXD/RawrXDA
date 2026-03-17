package com.rawr.mobilecopilot.resources

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.*
import android.os.PowerManager
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import android.util.Log
import java.io.File
import java.io.FileReader
import java.util.concurrent.TimeUnit

/**
 * Native device resource manager for mobile copilot
 * Real-time monitoring of RAM, CPU, battery, and system performance
 */
class NativeResourceManager(private val context: Context) {
    
    companion object {
        private const val TAG = "NativeResourceManager"
        private const val MONITORING_INTERVAL = 1000L // 1 second
        private const val CPU_SAMPLE_DURATION = 100L // 100ms
    }
    
    private val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
    private val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
    private val batteryManager = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
        context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager
    } else null
    
    /**
     * Real-time RAM monitoring
     */
    fun monitorRAM(): Flow<RAMInfo> = flow {
        while (currentCoroutineContext().isActive) {
            val memoryInfo = ActivityManager.MemoryInfo()
            activityManager.getMemoryInfo(memoryInfo)
            
            val runtime = Runtime.getRuntime()
            val processInfo = getProcessMemoryInfo()
            
            val ramInfo = RAMInfo(
                totalRAM = memoryInfo.totalMem,
                availableRAM = memoryInfo.availMem,
                usedRAM = memoryInfo.totalMem - memoryInfo.availMem,
                lowMemoryThreshold = memoryInfo.threshold,
                isLowMemory = memoryInfo.lowMemory,
                processPrivateDirty = processInfo?.totalPrivateDirty?.times(1024L) ?: 0L,
                processSharedDirty = processInfo?.totalSharedDirty?.times(1024L) ?: 0L,
                processPss = processInfo?.totalPss?.times(1024L) ?: 0L,
                heapSize = runtime.totalMemory(),
                heapUsed = runtime.totalMemory() - runtime.freeMemory(),
                heapMax = runtime.maxMemory()
            )
            
            emit(ramInfo)
            delay(MONITORING_INTERVAL)
        }
    }.flowOn(Dispatchers.IO)
    
    /**
     * Real-time CPU monitoring using /proc/stat
     */
    fun monitorCPU(): Flow<CPUInfo> = flow {
        var lastCpuStats: CpuStats? = null
        
        while (currentCoroutineContext().isActive) {
            val currentCpuStats = readCpuStats()
            
            if (lastCpuStats != null && currentCpuStats != null) {
                val usage = calculateCpuUsage(lastCpuStats, currentCpuStats)
                val processUsage = getProcessCpuUsage()
                
                val cpuInfo = CPUInfo(
                    overallUsage = usage.overallUsage,
                    userUsage = usage.userUsage,
                    systemUsage = usage.systemUsage,
                    idleUsage = usage.idleUsage,
                    processUsage = processUsage,
                    coreCount = Runtime.getRuntime().availableProcessors(),
                    frequency = getCurrentCpuFrequency(),
                    temperature = getCpuTemperature()
                )
                
                emit(cpuInfo)
            }
            
            lastCpuStats = currentCpuStats
            delay(MONITORING_INTERVAL)
        }
    }.flowOn(Dispatchers.IO)
    
    /**
     * Real-time battery monitoring
     */
    fun monitorBattery(): Flow<BatteryInfo> = callbackFlow {
        val batteryReceiver = object : android.content.BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                if (intent?.action == Intent.ACTION_BATTERY_CHANGED) {
                    val batteryInfo = extractBatteryInfo(intent)
                    trySend(batteryInfo)
                }
            }
        }
        
        val filter = IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        context.registerReceiver(batteryReceiver, filter)
        
        // Send initial battery state
        val initialIntent = context.registerReceiver(null, filter)
        if (initialIntent != null) {
            val initialBatteryInfo = extractBatteryInfo(initialIntent)
            trySend(initialBatteryInfo)
        }
        
        awaitClose {
            context.unregisterReceiver(batteryReceiver)
        }
    }.flowOn(Dispatchers.Main)
    
    /**
     * System performance monitoring
     */
    fun monitorSystemPerformance(): Flow<SystemPerformanceInfo> = combine(
        monitorRAM(),
        monitorCPU(),
        monitorBattery()
    ) { ram, cpu, battery ->
        
        val thermalState = getThermalState()
        val powerProfile = getPowerProfile()
        val networkStats = getNetworkStats()
        val diskStats = getDiskStats()
        
        SystemPerformanceInfo(
            ramInfo = ram,
            cpuInfo = cpu,
            batteryInfo = battery,
            thermalState = thermalState,
            powerProfile = powerProfile,
            networkStats = networkStats,
            diskStats = diskStats,
            timestamp = System.currentTimeMillis()
        )
    }
    
    /**
     * Get process memory information
     */
    private fun getProcessMemoryInfo(): Debug.MemoryInfo? {
        return try {
            val pid = android.os.Process.myPid()
            val memoryInfos = activityManager.getProcessMemoryInfo(intArrayOf(pid))
            memoryInfos.firstOrNull()
        } catch (e: Exception) {
            Log.e(TAG, "Error getting process memory info", e)
            null
        }
    }
    
    /**
     * Read CPU statistics from /proc/stat
     */
    private fun readCpuStats(): CpuStats? {
        return try {
            val statFile = File("/proc/stat")
            if (!statFile.exists()) return null
            
            val line = statFile.readLines().first { it.startsWith("cpu ") }
            val values = line.split("\\s+".toRegex()).drop(1).map { it.toLong() }
            
            if (values.size >= 7) {
                CpuStats(
                    user = values[0],
                    nice = values[1],
                    system = values[2],
                    idle = values[3],
                    iowait = values[4],
                    irq = values[5],
                    softirq = values[6]
                )
            } else null
        } catch (e: Exception) {
            Log.e(TAG, "Error reading CPU stats", e)
            null
        }
    }
    
    /**
     * Calculate CPU usage between two measurements
     */
    private fun calculateCpuUsage(prev: CpuStats, curr: CpuStats): CpuUsage {
        val prevIdle = prev.idle + prev.iowait
        val currIdle = curr.idle + curr.iowait
        
        val prevNonIdle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq
        val currNonIdle = curr.user + curr.nice + curr.system + curr.irq + curr.softirq
        
        val prevTotal = prevIdle + prevNonIdle
        val currTotal = currIdle + currNonIdle
        
        val totalDelta = currTotal - prevTotal
        val idleDelta = currIdle - prevIdle
        
        val overallUsage = if (totalDelta > 0) {
            ((totalDelta - idleDelta).toDouble() / totalDelta.toDouble()) * 100.0
        } else 0.0
        
        val userUsage = if (totalDelta > 0) {
            ((curr.user - prev.user).toDouble() / totalDelta.toDouble()) * 100.0
        } else 0.0
        
        val systemUsage = if (totalDelta > 0) {
            ((curr.system - prev.system).toDouble() / totalDelta.toDouble()) * 100.0
        } else 0.0
        
        return CpuUsage(
            overallUsage = overallUsage,
            userUsage = userUsage,
            systemUsage = systemUsage,
            idleUsage = 100.0 - overallUsage
        )
    }
    
    /**
     * Get current process CPU usage
     */
    private fun getProcessCpuUsage(): Double {
        return try {
            val pid = android.os.Process.myPid()
            val statFile = File("/proc/$pid/stat")
            if (!statFile.exists()) return 0.0
            
            val stats = statFile.readText().split(" ")
            if (stats.size >= 15) {
                val utime = stats[13].toLong()
                val stime = stats[14].toLong()
                val totalTime = utime + stime
                
                // This is a simplified calculation
                // For accurate results, you'd need to track deltas over time
                totalTime.toDouble() / 100.0 // Convert to percentage
            } else 0.0
        } catch (e: Exception) {
            Log.e(TAG, "Error getting process CPU usage", e)
            0.0
        }
    }
    
    /**
     * Get current CPU frequency
     */
    private fun getCurrentCpuFrequency(): Long {
        return try {
            val freqFile = File("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")
            if (freqFile.exists()) {
                freqFile.readText().trim().toLong()
            } else 0L
        } catch (e: Exception) {
            Log.e(TAG, "Error reading CPU frequency", e)
            0L
        }
    }
    
    /**
     * Get CPU temperature (if available)
     */
    private fun getCpuTemperature(): Float {
        return try {
            val tempFiles = listOf(
                "/sys/class/thermal/thermal_zone0/temp",
                "/sys/devices/system/cpu/cpu0/cpufreq/cpu_temp",
                "/sys/class/hwmon/hwmon0/temp1_input"
            )
            
            for (tempFile in tempFiles) {
                val file = File(tempFile)
                if (file.exists()) {
                    val temp = file.readText().trim().toFloat()
                    return if (temp > 1000) temp / 1000 else temp // Convert millidegrees if needed
                }
            }
            0.0f
        } catch (e: Exception) {
            Log.e(TAG, "Error reading CPU temperature", e)
            0.0f
        }
    }
    
    /**
     * Extract battery information from intent
     */
    private fun extractBatteryInfo(intent: Intent): BatteryInfo {
        val level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
        val scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
        val status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
        val health = intent.getIntExtra(BatteryManager.EXTRA_HEALTH, -1)
        val voltage = intent.getIntExtra(BatteryManager.EXTRA_VOLTAGE, -1)
        val temperature = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, -1)
        val technology = intent.getStringExtra(BatteryManager.EXTRA_TECHNOLOGY) ?: "Unknown"
        
        val percentage = if (level >= 0 && scale > 0) {
            (level.toFloat() / scale.toFloat()) * 100f
        } else 0f
        
        val currentNow = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && batteryManager != null) {
            batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_NOW)
        } else 0
        
        val capacityRemaining = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && batteryManager != null) {
            batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER)
        } else 0
        
        return BatteryInfo(
            level = level,
            percentage = percentage,
            status = getBatteryStatus(status),
            health = getBatteryHealth(health),
            voltage = voltage,
            temperature = temperature / 10.0f, // Convert to Celsius
            technology = technology,
            currentNow = currentNow,
            capacityRemaining = capacityRemaining,
            isCharging = status == BatteryManager.BATTERY_STATUS_CHARGING,
            isPowerSaveMode = powerManager.isPowerSaveMode
        )
    }
    
    /**
     * Get thermal state
     */
    @Suppress("DEPRECATION")
    private fun getThermalState(): ThermalState {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val thermalStatus = powerManager.currentThermalStatus
            when (thermalStatus) {
                PowerManager.THERMAL_STATUS_NONE -> ThermalState.NORMAL
                PowerManager.THERMAL_STATUS_LIGHT -> ThermalState.LIGHT
                PowerManager.THERMAL_STATUS_MODERATE -> ThermalState.MODERATE
                PowerManager.THERMAL_STATUS_SEVERE -> ThermalState.SEVERE
                PowerManager.THERMAL_STATUS_CRITICAL -> ThermalState.CRITICAL
                PowerManager.THERMAL_STATUS_EMERGENCY -> ThermalState.EMERGENCY
                PowerManager.THERMAL_STATUS_SHUTDOWN -> ThermalState.SHUTDOWN
                else -> ThermalState.UNKNOWN
            }
        } else {
            ThermalState.UNKNOWN
        }
    }
    
    /**
     * Get power profile information
     */
    private fun getPowerProfile(): PowerProfile {
        val isInteractive = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            powerManager.isInteractive
        } else {
            @Suppress("DEPRECATION")
            powerManager.isScreenOn
        }
        
        return PowerProfile(
            isPowerSaveMode = powerManager.isPowerSaveMode,
            isInteractive = isInteractive,
            isDeviceIdleMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                powerManager.isDeviceIdleMode
            } else false,
            isIgnoringBatteryOptimizations = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                powerManager.isIgnoringBatteryOptimizations(context.packageName)
            } else false
        )
    }
    
    /**
     * Get network statistics
     */
    private fun getNetworkStats(): NetworkStats {
        val mobileRx = android.net.TrafficStats.getMobileRxBytes()
        val mobileTx = android.net.TrafficStats.getMobileTxBytes()
        val totalRx = android.net.TrafficStats.getTotalRxBytes()
        val totalTx = android.net.TrafficStats.getTotalTxBytes()
        
        return NetworkStats(
            mobileRxBytes = mobileRx,
            mobileTxBytes = mobileTx,
            totalRxBytes = totalRx,
            totalTxBytes = totalTx,
            wifiRxBytes = maxOf(0, totalRx - mobileRx),
            wifiTxBytes = maxOf(0, totalTx - mobileTx)
        )
    }
    
    /**
     * Get disk statistics
     */
    private fun getDiskStats(): DiskStats {
        val internalStorage = context.filesDir
        val externalStorage = context.getExternalFilesDir(null)
        
        return DiskStats(
            internalTotalSpace = internalStorage.totalSpace,
            internalFreeSpace = internalStorage.freeSpace,
            internalUsedSpace = internalStorage.totalSpace - internalStorage.freeSpace,
            externalTotalSpace = externalStorage?.totalSpace ?: 0L,
            externalFreeSpace = externalStorage?.freeSpace ?: 0L,
            externalUsedSpace = (externalStorage?.totalSpace ?: 0L) - (externalStorage?.freeSpace ?: 0L)
        )
    }
    
    private fun getBatteryStatus(status: Int): String = when (status) {
        BatteryManager.BATTERY_STATUS_CHARGING -> "Charging"
        BatteryManager.BATTERY_STATUS_DISCHARGING -> "Discharging"
        BatteryManager.BATTERY_STATUS_FULL -> "Full"
        BatteryManager.BATTERY_STATUS_NOT_CHARGING -> "Not Charging"
        BatteryManager.BATTERY_STATUS_UNKNOWN -> "Unknown"
        else -> "Unknown"
    }
    
    private fun getBatteryHealth(health: Int): String = when (health) {
        BatteryManager.BATTERY_HEALTH_GOOD -> "Good"
        BatteryManager.BATTERY_HEALTH_OVERHEAT -> "Overheat"
        BatteryManager.BATTERY_HEALTH_DEAD -> "Dead"
        BatteryManager.BATTERY_HEALTH_OVER_VOLTAGE -> "Over Voltage"
        BatteryManager.BATTERY_HEALTH_UNSPECIFIED_FAILURE -> "Unspecified Failure"
        BatteryManager.BATTERY_HEALTH_COLD -> "Cold"
        BatteryManager.BATTERY_HEALTH_UNKNOWN -> "Unknown"
        else -> "Unknown"
    }
}

// Data classes for resource information
data class RAMInfo(
    val totalRAM: Long,
    val availableRAM: Long,
    val usedRAM: Long,
    val lowMemoryThreshold: Long,
    val isLowMemory: Boolean,
    val processPrivateDirty: Long,
    val processSharedDirty: Long,
    val processPss: Long,
    val heapSize: Long,
    val heapUsed: Long,
    val heapMax: Long
)

data class CPUInfo(
    val overallUsage: Double,
    val userUsage: Double,
    val systemUsage: Double,
    val idleUsage: Double,
    val processUsage: Double,
    val coreCount: Int,
    val frequency: Long,
    val temperature: Float
)

data class BatteryInfo(
    val level: Int,
    val percentage: Float,
    val status: String,
    val health: String,
    val voltage: Int,
    val temperature: Float,
    val technology: String,
    val currentNow: Int,
    val capacityRemaining: Int,
    val isCharging: Boolean,
    val isPowerSaveMode: Boolean
)

data class SystemPerformanceInfo(
    val ramInfo: RAMInfo,
    val cpuInfo: CPUInfo,
    val batteryInfo: BatteryInfo,
    val thermalState: ThermalState,
    val powerProfile: PowerProfile,
    val networkStats: NetworkStats,
    val diskStats: DiskStats,
    val timestamp: Long
)

private data class CpuStats(
    val user: Long,
    val nice: Long,
    val system: Long,
    val idle: Long,
    val iowait: Long,
    val irq: Long,
    val softirq: Long
)

private data class CpuUsage(
    val overallUsage: Double,
    val userUsage: Double,
    val systemUsage: Double,
    val idleUsage: Double
)

enum class ThermalState {
    NORMAL, LIGHT, MODERATE, SEVERE, CRITICAL, EMERGENCY, SHUTDOWN, UNKNOWN
}

data class PowerProfile(
    val isPowerSaveMode: Boolean,
    val isInteractive: Boolean,
    val isDeviceIdleMode: Boolean,
    val isIgnoringBatteryOptimizations: Boolean
)

data class NetworkStats(
    val mobileRxBytes: Long,
    val mobileTxBytes: Long,
    val totalRxBytes: Long,
    val totalTxBytes: Long,
    val wifiRxBytes: Long,
    val wifiTxBytes: Long
)

data class DiskStats(
    val internalTotalSpace: Long,
    val internalFreeSpace: Long,
    val internalUsedSpace: Long,
    val externalTotalSpace: Long,
    val externalFreeSpace: Long,
    val externalUsedSpace: Long
)