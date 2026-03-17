package com.rawr.mobilecopilot.core

import android.content.Context
import android.media.MediaRecorder
import android.os.Build
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.util.Log
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import java.io.File

class VoiceProcessor(private val context: Context) {
    private var recorder: MediaRecorder? = null
    private var speechRecognizer: SpeechRecognizer? = null
    private var outputFile: File? = null

    fun startRecording(): File? {
        try {
            stopRecording()
            outputFile = File.createTempFile("voice_input", ".m4a", context.cacheDir)
            val r = MediaRecorder()
            r.setAudioSource(MediaRecorder.AudioSource.MIC)
            r.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4)
            r.setAudioEncoder(MediaRecorder.AudioEncoder.AAC)
            r.setAudioEncodingBitRate(128000)
            r.setAudioSamplingRate(44100)
            r.setOutputFile(outputFile!!.absolutePath)
            r.prepare()
            r.start()
            recorder = r
            return outputFile
        } catch (e: Exception) {
            Log.e("VoiceProcessor", "startRecording failed: ${e.message}")
            return null
        }
    }

    fun stopRecording(): File? {
        try {
            recorder?.apply { stop(); release() }
        } catch (_: Exception) { }
        recorder = null
        return outputFile
    }

    fun streamTranscription(language: String = "en-US"): Flow<String> = callbackFlow {
        if (!SpeechRecognizer.isRecognitionAvailable(context)) {
            close(IllegalStateException("Speech recognition not available"))
            return@callbackFlow
        }
        speechRecognizer?.destroy()
        val sr = SpeechRecognizer.createSpeechRecognizer(context)
        speechRecognizer = sr
        val intent = RecognizerIntent().apply {
            action = RecognizerIntent.ACTION_RECOGNIZE_SPEECH
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
            putExtra(RecognizerIntent.EXTRA_LANGUAGE, language)
        }
        val listener = object : RecognitionListener {
            override fun onReadyForSpeech(params: Bundle?) {}
            override fun onBeginningOfSpeech() {}
            override fun onRmsChanged(rmsdB: Float) {}
            override fun onBufferReceived(buffer: ByteArray?) {}
            override fun onEndOfSpeech() {}
            override fun onError(error: Int) { trySend("[error:$error]") }
            override fun onResults(results: Bundle?) {
                val texts = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                texts?.firstOrNull()?.let { trySend(it) }
            }
            override fun onPartialResults(partialResults: Bundle?) {
                val texts = partialResults?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                texts?.firstOrNull()?.let { trySend(it) }
            }
            override fun onEvent(eventType: Int, params: Bundle?) {}
        }
        sr.setRecognitionListener(listener)
        sr.startListening(intent)

        awaitClose {
            try { sr.stopListening(); sr.destroy() } catch (_: Exception) {}
            speechRecognizer = null
        }
    }
}package com.rawr.mobilecopilot.core

import android.content.Context
import android.media.MediaRecorder
import android.os.Environment
import android.speech.RecognitionListener
import android.speech.SpeechRecognizer
import android.speech.RecognizerIntent
import android.content.Intent
import android.util.Log
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.receiveAsFlow
import java.io.File

class VoiceProcessor(private val context: Context) {
    private var mediaRecorder: MediaRecorder? = null
    private var speechRecognizer: SpeechRecognizer? = null
    private var isRecording: Boolean = false
    private val transcriptChannel = Channel<String>(Channel.UNLIMITED)

    fun transcripts(): Flow<String> = transcriptChannel.receiveAsFlow()

    fun startRecording() {
        if (isRecording) return
        try {
            val outputDir = context.getExternalFilesDir(Environment.DIRECTORY_MUSIC)
            val outputFile = File(outputDir, "voice_input.raw")
            mediaRecorder = MediaRecorder().apply {
                setAudioSource(MediaRecorder.AudioSource.MIC)
                setOutputFormat(MediaRecorder.OutputFormat.THREE_GPP)
                setAudioEncoder(MediaRecorder.AudioEncoder.AMR_NB)
                setOutputFile(outputFile.absolutePath)
                prepare()
                start()
            }
            isRecording = true
            initSpeechRecognizer()
            startSpeechRecognition()
        } catch (e: Exception) {
            Log.e("VoiceProcessor", "Failed to start recording: ${e.message}")
        }
    }

    private fun initSpeechRecognizer() {
        if (SpeechRecognizer.isRecognitionAvailable(context)) {
            speechRecognizer = SpeechRecognizer.createSpeechRecognizer(context).apply {
                setRecognitionListener(object : RecognitionListener {
                    override fun onReadyForSpeech(params: Bundle?) {}
                    override fun onBeginningOfSpeech() {}
                    override fun onRmsChanged(rmsdB: Float) {}
                    override fun onBufferReceived(buffer: ByteArray?) {}
                    override fun onEndOfSpeech() {}
                    override fun onError(error: Int) {
                        transcriptChannel.trySend("[speech_error:$error]")
                    }
                    override fun onResults(results: Bundle?) {
                        val data = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                        data?.firstOrNull()?.let { transcriptChannel.trySend(it) }
                    }
                    override fun onPartialResults(partialResults: Bundle?) {
                        val data = partialResults?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                        data?.firstOrNull()?.let { transcriptChannel.trySend(it) }
                    }
                    override fun onEvent(eventType: Int, params: Bundle?) {}
                })
            }
        }
    }

    private fun startSpeechRecognition() {
        val intent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
        }
        speechRecognizer?.startListening(intent)
    }

    fun stop() {
        try {
            mediaRecorder?.apply {
                stop(); release()
            }
            speechRecognizer?.apply { stopListening(); cancel(); destroy() }
        } catch (_: Exception) {}
        mediaRecorder = null
        speechRecognizer = null
        isRecording = false
    }
}