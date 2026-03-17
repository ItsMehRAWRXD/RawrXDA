package com.rawr.mobilecopilot.core

import kotlinx.coroutines.*
import android.util.Log

/**
 * Native TensorFlow Lite model inference engine
 * Direct interface to TensorFlow Lite C++ API for maximum performance
 */
class ModelInferenceEngine {
    
    companion object {
        init {
            System.loadLibrary("native-fileio")
        }
        
        private const val TAG = "ModelInferenceEngine"
    }
    
    /**
     * Load model from file path
     * @param modelPath Absolute path to .tflite model file
     * @return Model pointer or 0 on error
     */
    external fun nativeLoadModel(modelPath: String): Long
    
    /**
     * Get input tensor shape
     * @param modelPtr Model pointer
     * @param inputIndex Input tensor index
     * @return Array of dimensions
     */
    external fun nativeGetInputShape(modelPtr: Long, inputIndex: Int): IntArray?
    
    /**
     * Get output tensor shape
     * @param modelPtr Model pointer
     * @param outputIndex Output tensor index
     * @return Array of dimensions
     */
    external fun nativeGetOutputShape(modelPtr: Long, outputIndex: Int): IntArray?
    
    /**
     * Set input data for inference
     * @param modelPtr Model pointer
     * @param inputIndex Input tensor index
     * @param inputData Input data as float array
     * @return Success status
     */
    external fun nativeSetInputData(modelPtr: Long, inputIndex: Int, inputData: FloatArray): Boolean
    
    /**
     * Run model inference
     * @param modelPtr Model pointer
     * @return Success status
     */
    external fun nativeRunInference(modelPtr: Long): Boolean
    
    /**
     * Get output data after inference
     * @param modelPtr Model pointer
     * @param outputIndex Output tensor index
     * @return Output data as float array
     */
    external fun nativeGetOutputData(modelPtr: Long, outputIndex: Int): FloatArray?
    
    /**
     * Release model and free memory
     * @param modelPtr Model pointer
     */
    external fun nativeReleaseModel(modelPtr: Long)
}

/**
 * High-level AI model wrapper for inference operations
 */
class AIModel(private val modelPath: String) : AutoCloseable {
    private val engine = ModelInferenceEngine()
    private var modelPtr: Long = 0
    private var isLoaded = false
    
    /**
     * Load the model from file
     */
    suspend fun load() = withContext(Dispatchers.IO) {
        if (isLoaded) return@withContext
        
        modelPtr = engine.nativeLoadModel(modelPath)
        if (modelPtr == 0L) {
            throw RuntimeException("Failed to load model: $modelPath")
        }
        
        isLoaded = true
        Log.i("AIModel", "Successfully loaded model: $modelPath")
    }
    
    /**
     * Get input tensor shape
     * @param inputIndex Input tensor index (default 0)
     * @return Array of dimensions
     */
    fun getInputShape(inputIndex: Int = 0): IntArray? {
        if (!isLoaded) throw IllegalStateException("Model not loaded")
        return engine.nativeGetInputShape(modelPtr, inputIndex)
    }
    
    /**
     * Get output tensor shape
     * @param outputIndex Output tensor index (default 0)
     * @return Array of dimensions
     */
    fun getOutputShape(outputIndex: Int = 0): IntArray? {
        if (!isLoaded) throw IllegalStateException("Model not loaded")
        return engine.nativeGetOutputShape(modelPtr, outputIndex)
    }
    
    /**
     * Run inference with input data
     * @param inputData Input data as float array
     * @param inputIndex Input tensor index (default 0)
     * @param outputIndex Output tensor index (default 0)
     * @return Output data as float array
     */
    suspend fun predict(
        inputData: FloatArray, 
        inputIndex: Int = 0, 
        outputIndex: Int = 0
    ): FloatArray? = withContext(Dispatchers.Default) {
        
        if (!isLoaded) throw IllegalStateException("Model not loaded")
        
        // Set input data
        if (!engine.nativeSetInputData(modelPtr, inputIndex, inputData)) {
            Log.e("AIModel", "Failed to set input data")
            return@withContext null
        }
        
        // Run inference
        if (!engine.nativeRunInference(modelPtr)) {
            Log.e("AIModel", "Inference failed")
            return@withContext null
        }
        
        // Get output data
        return@withContext engine.nativeGetOutputData(modelPtr, outputIndex)
    }
    
    /**
     * Run batch inference
     * @param batchInputs List of input data arrays
     * @param inputIndex Input tensor index (default 0)
     * @param outputIndex Output tensor index (default 0)
     * @return List of output data arrays
     */
    suspend fun predictBatch(
        batchInputs: List<FloatArray>,
        inputIndex: Int = 0,
        outputIndex: Int = 0
    ): List<FloatArray?> = withContext(Dispatchers.Default) {
        
        batchInputs.map { inputData ->
            predict(inputData, inputIndex, outputIndex)
        }
    }
    
    /**
     * Stream inference for large datasets
     * @param inputStream Sequence of input data
     * @param batchSize Number of inputs to process at once
     * @param onResult Callback for each result
     */
    suspend fun streamPredict(
        inputStream: Sequence<FloatArray>,
        batchSize: Int = 1,
        onResult: (FloatArray?) -> Unit
    ) = withContext(Dispatchers.Default) {
        
        inputStream.chunked(batchSize).forEach { batch ->
            val results = predictBatch(batch)
            results.forEach { result ->
                onResult(result)
            }
        }
    }
    
    override fun close() {
        if (isLoaded && modelPtr != 0L) {
            engine.nativeReleaseModel(modelPtr)
            modelPtr = 0
            isLoaded = false
            Log.i("AIModel", "Model released")
        }
    }
}