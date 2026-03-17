package com.rawr.mobilecopilot.services

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel

/**
 * Background service for AI inference tasks
 */
class AIInferenceService : Service() {
    private val job = Job()
    private val scope = CoroutineScope(Dispatchers.Default + job)

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        Log.i("AIInferenceService", "Service created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.i("AIInferenceService", "Service started")
        // TODO: Wire to ModelInferenceEngine for background processing
        return START_STICKY
    }

    override fun onDestroy() {
        scope.cancel()
        Log.i("AIInferenceService", "Service destroyed")
        super.onDestroy()
    }
}