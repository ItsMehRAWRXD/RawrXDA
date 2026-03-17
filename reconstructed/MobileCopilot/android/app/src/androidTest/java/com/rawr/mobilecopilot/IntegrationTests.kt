package com.rawr.mobilecopilot

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.rawr.mobilecopilot.core.HighPerformanceFileManager
import com.rawr.mobilecopilot.security.SecureSandbox
import com.rawr.mobilecopilot.streaming.NativeStreamingClient
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class IntegrationTests {
    @Test
    fun testFileIOAndSandbox() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val sandbox = SecureSandbox(appContext)
        val fileManager = HighPerformanceFileManager()
        val secureFile = sandbox.createSecureFile("test.bin")
        assertTrue(secureFile != null)
        val data = ByteArray(1024) { it.toByte() }
        val ok = fileManager.writeFileBytes(secureFile!!.absolutePath, data)
        assertTrue(ok)
        val read = fileManager.readFileBytes(secureFile.absolutePath)
        assertTrue(read != null && read!!.size == 1024)
    }

    @Test
    fun testStreamingFlow() = runBlocking {
        val client = NativeStreamingClient()
        val firstChunk = client.streamRequest("https://httpbin.org/stream/5").first()
        assertTrue(firstChunk is com.rawr.mobilecopilot.streaming.StreamChunk.Data || firstChunk is com.rawr.mobilecopilot.streaming.StreamChunk.Headers)
    }
}
