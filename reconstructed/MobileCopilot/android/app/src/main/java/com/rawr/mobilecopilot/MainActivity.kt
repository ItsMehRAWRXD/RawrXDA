package com.rawr.mobilecopilot

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.lifecycle.lifecycleScope
import androidx.compose.runtime.rememberCoroutineScope
import android.content.Context
import com.rawr.mobilecopilot.core.*
import com.rawr.mobilecopilot.security.*
import com.rawr.mobilecopilot.streaming.*
import com.rawr.mobilecopilot.resources.*
import com.rawr.mobilecopilot.ui.*
import com.rawr.mobilecopilot.ui.theme.MobileCopilotTheme
import kotlinx.coroutines.launch
import android.util.Log

class MainActivity : ComponentActivity() {
    
    private lateinit var permissionManager: SecurePermissionManager
    private lateinit var secureSandbox: SecureSandbox
    private lateinit var resourceManager: NativeResourceManager
    private lateinit var streamingClient: NativeStreamingClient
    private lateinit var fileManager: HighPerformanceFileManager
    private var aiModel: AIModel? = null
    private lateinit var voiceProcessor: VoiceProcessor
    
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.values.all { it }
        if (allGranted) {
            initializeServices()
        } else {
            Log.w("MainActivity", "Some permissions were denied")
            // Show permission rationale or handle gracefully
        }
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Initialize security and resource managers
        permissionManager = SecurePermissionManager(this)
        secureSandbox = SecureSandbox(this)
        resourceManager = NativeResourceManager(this)
        streamingClient = NativeStreamingClient()
        fileManager = HighPerformanceFileManager()
        voiceProcessor = VoiceProcessor(this)
        
        // Request permissions
        requestPermissions()
        
        setContent {
            MobileCopilotTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MobileCopilotApp(
                        onInitModel = { path ->
                            lifecycleScope.launch {
                                aiModel = AIModel(path)
                                aiModel?.load()
                            }
                        },
                        onPredict = { input ->
                            lifecycleScope.launch {
                                val result = aiModel?.predict(input)
                                // handle result (no-op UI binding here)
                                Log.d("MobileCopilotApp", "Inference result size: ${result?.size ?: -1}")
                            }
                        },
                        streamingClient = streamingClient,
                        voiceProcessor = voiceProcessor
                    )
                }
            }
        }
    }
    
    private fun requestPermissions() {
        lifecycleScope.launch {
            try {
                val hasPermissions = permissionManager.requestPermissionsAsync(
                    this@MainActivity,
                    SecurePermissionManager.REQUIRED_PERMISSIONS
                )
                
                if (hasPermissions) {
                    initializeServices()
                } else {
                    Log.e("MainActivity", "Required permissions not granted")
                }
            } catch (e: Exception) {
                Log.e("MainActivity", "Error requesting permissions", e)
            }
        }
    }
    
    private fun initializeServices() {
        lifecycleScope.launch {
            try {
                // Start monitoring system resources
                resourceManager.monitorSystemPerformance().collect { performanceInfo ->
                    Log.d("MainActivity", "System Performance: $performanceInfo")
                    // Handle resource information
                }
            } catch (e: Exception) {
                Log.e("MainActivity", "Error initializing services", e)
            }
        }
    }
    
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        PermissionResultHandler.handlePermissionResult(requestCode, permissions, grantResults)
    }
}

@Composable
fun MobileCopilotApp(
    onInitModel: (String) -> Unit,
    onPredict: (FloatArray) -> Unit,
    streamingClient: NativeStreamingClient,
    voiceProcessor: VoiceProcessor
) {
    var currentScreen by remember { mutableStateOf(Screen.CHAT) }
    var chatMessages by remember { mutableStateOf(listOf<String>()) }
    // Initialize model from bundled assets (Camellia pipeline integration)
    LaunchedEffect(Unit) {
        try {
            // Load model from assets directory populated by Gradle Camellia task
            val modelPath = "android_asset/models/camellia-model.tflite"
            onInitModel(modelPath)
            Log.d("MobileCopilotApp", "Model initialization started for: $modelPath")
        } catch (e: Exception) {
            Log.e("MobileCopilotApp", "Failed to initialize model from assets: ${e.message}")
        }
    }
    
    val uiScope = rememberCoroutineScope()

    var latencyMs by remember { mutableStateOf(0L) }
    var tokensPerSec by remember { mutableStateOf(0.0) }
    var streamStart by remember { mutableStateOf(0L) }
    var tokenCounter by remember { mutableStateOf(0) }

    var voiceActive by remember { mutableStateOf(false) }
    // Collect transcripts when voice active
    LaunchedEffect(voiceActive) {
        if (voiceActive) {
            voiceProcessor.transcripts().collect { partial ->
                // treat partial transcript as user message for live refinement
                val tokenized = tokenizeText(partial)
                onPredict(tokenized)
                chatMessages = chatMessages + "[voice] $partial"
            }
        }
    }

    when (currentScreen) {
        Screen.CHAT -> {
            MobileCopilotUI(
                onMessageSent = { message ->
                    uiScope.launch {
                        try {
                            // Real tokenization: convert text to model input
                            val tokenized = tokenizeText(message)
                            onPredict(tokenized)
                            chatMessages = chatMessages + "User: $message"
                            
                            // Stream inference results
                            streamStart = System.nanoTime(); tokenCounter = 0
                            streamingClient.streamInference(tokenized).collect { chunk ->
                                tokenCounter += chunk.size
                                val responseText = decodeTokens(chunk)
                                chatMessages = chatMessages + "AI: $responseText"
                                val elapsedMs = (System.nanoTime() - streamStart) / 1_000_000.0
                                latencyMs = elapsedMs.toLong()
                                tokensPerSec = if (elapsedMs > 0) tokenCounter / (elapsedMs / 1000.0) else 0.0
                            }
                        } catch (e: Exception) {
                            Log.e("MobileCopilotApp", "Inference failed: ${e.message}")
                            chatMessages = chatMessages + "Error: ${e.message}"
                        }
                    }
                },
                onVoiceCommand = {
                    voiceActive = !voiceActive
                    if (voiceActive) {
                        voiceProcessor.startRecording()
                        chatMessages = chatMessages + "[voice] listening..."
                    } else {
                        voiceProcessor.stop()
                        chatMessages = chatMessages + "[voice] stopped"
                    }
                },
                onFileAttach = {
                    uiScope.launch {
                        try {
                            val file = secureSandbox.openFileSecurely()
                            val content = fileManager.readFile(file)
                            Log.d("MobileCopilotApp", "File loaded: ${content.size} bytes")
                            // Process file content through inference pipeline
                        } catch (e: Exception) {
                            Log.e("MobileCopilotApp", "File attach failed: ${e.message}")
                        }
                    }
                },
                onSettingsClick = { currentScreen = Screen.SETTINGS },
                latencyMs = latencyMs,
                tokensPerSec = tokensPerSec
            )
        }
        Screen.SETTINGS -> {
            SettingsScreen(
                onBackClick = {
                    currentScreen = Screen.CHAT
                }
            )
        }
    }
}

@Composable
fun SettingsScreen(
    onBackClick: () -> Unit
) {
    var selectedModel by remember { mutableStateOf("camellia-model.tflite") }
    var inferenceThreads by remember { mutableStateOf(4) }
    var useGPU by remember { mutableStateOf(false) }
    var maxMemoryMB by remember { mutableStateOf(512) }
    
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp)
        ) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(onClick = onBackClick) {
                    Icon(Icons.Default.ArrowBack, "Back")
                }
                Text(
                    "Settings",
                    style = MaterialTheme.typography.headlineMedium,
                    modifier = Modifier.padding(start = 8.dp)
                )
            }
            
            Spacer(modifier = Modifier.height(24.dp))
            
            // Model Selection
            Text("Model Configuration", style = MaterialTheme.typography.titleMedium)
            Spacer(modifier = Modifier.height(8.dp))
            
            // Available models dropdown
            Text("Selected Model: $selectedModel")
            
            Spacer(modifier = Modifier.height(16.dp))
            
            // Performance Settings
            Text("Performance Tuning", style = MaterialTheme.typography.titleMedium)
            Spacer(modifier = Modifier.height(8.dp))
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Inference Threads: $inferenceThreads")
                Slider(
                    value = inferenceThreads.toFloat(),
                    onValueChange = { inferenceThreads = it.toInt() },
                    valueRange = 1f..8f,
                    steps = 6
                )
            }
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Use GPU Acceleration")
                Switch(
                    checked = useGPU,
                    onCheckedChange = { useGPU = it }
                )
            }
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Max Memory (MB): $maxMemoryMB")
                Slider(
                    value = maxMemoryMB.toFloat(),
                    onValueChange = { maxMemoryMB = it.toInt() },
                    valueRange = 256f..2048f,
                    steps = 7
                )
            }
            
            Spacer(modifier = Modifier.height(24.dp))
            
            // Resource Monitoring
            Text("Resource Monitor", style = MaterialTheme.typography.titleMedium)
            Spacer(modifier = Modifier.height(8.dp))
            
            // Real-time resource display would go here
            Text("CPU Usage: Monitoring...", style = MaterialTheme.typography.bodyMedium)
            Text("Memory Usage: Monitoring...", style = MaterialTheme.typography.bodyMedium)
            Text("Battery Impact: Low", style = MaterialTheme.typography.bodyMedium)
        }
    }
}

enum class Screen {
    CHAT, SETTINGS
}

// --- Tokenization / Decoding / Voice helpers ---
private const val MAX_TOKEN_LENGTH = 512

private var globalTokenizer: BPETokenizer? = null

private fun ensureTokenizer(context: Context) {
    if (globalTokenizer == null) {
        try { globalTokenizer = TokenizerLoader.load(context) } catch (_: Exception) { }
    }
}

private fun tokenizeText(text: String): FloatArray {
    // Ensure BPE tokenizer loaded
    // NOTE: In production, call ensureTokenizer once during initialization
    // For now, fallback to byte tokens if unavailable.
    val tokenizer = globalTokenizer
    if (tokenizer == null) {
        val raw = text.encodeToByteArray().map { it.toInt() and 0xFF }
        return FloatArray(MAX_TOKEN_LENGTH) { i -> if (i < raw.size) raw[i].toFloat() else 0f }
    }
    val encoded = tokenizer.encode(text).map { it.toFloat() }
    return FloatArray(MAX_TOKEN_LENGTH) { i -> if (i < encoded.size) encoded[i] else 0f }
}

private fun decodeTokens(tokens: FloatArray): String {
    val sb = StringBuilder()
    for (v in tokens) {
        if (v <= 0f) continue
        val c = v.toInt().toChar()
        // filter non-printable
        if (c.code in 32..126) sb.append(c)
    }
    return sb.toString()
}

private suspend fun recordAudio(): ByteArray {
    // TODO: Implement MediaRecorder based capture; returning empty for now.
    return ByteArray(0)
}

private suspend fun speechToText(audioData: ByteArray): String {
    // TODO: Integrate SpeechRecognizer or on-device model.
    return "(voice transcription not yet implemented)"
}