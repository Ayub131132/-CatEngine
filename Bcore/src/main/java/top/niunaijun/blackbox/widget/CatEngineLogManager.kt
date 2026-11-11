package top.niunaijun.blackbox.widget

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Process
import android.util.Log
import androidx.core.content.FileProvider
import java.io.BufferedReader
import java.io.File
import java.io.FileOutputStream
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.io.PrintWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors

/**
 * High-performance, robust logging system for CatEngine.
 * - Stores every application, native CatEngine, and crash log to `catengine_logs.txt`.
 * - Strictly enforces 50 MB maximum size limit: deletes and starts a new file if limit exceeded.
 * - Provides seamless "Share Logs" intent using standard Android Share menu.
 */
object CatEngineLogManager {
    private const val TAG = "CatEngineLog"
    private const val LOG_FILE_NAME = "catengine_logs.txt"
    private const val MAX_FILE_SIZE_BYTES = 50L * 1024L * 1024L // 50 MB

    private val executor = Executors.newSingleThreadExecutor()
    private var isLogcatStreaming = false
    private val dateFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)

    @Synchronized
    fun getLogFile(context: Context): File {
        val dir = context.getExternalFilesDir(null) ?: context.filesDir
        val file = File(dir, LOG_FILE_NAME)
        checkAndRotate(file)
        return file
    }

    private fun checkAndRotate(file: File) {
        try {
            if (file.exists() && file.length() > MAX_FILE_SIZE_BYTES) {
                Log.w(TAG, "Log file exceeded 50 MB (${file.length()} bytes). Deleting and creating new log file.")
                file.delete()
                file.createNewFile()
                appendLine(file, "[CatEngineLogManager] Log rotated: previous log exceeded 50 MB.")
            }
        } catch (t: Throwable) {
            Log.e(TAG, "Error checking log file size", t)
        }
    }

    /**
     * Start continuous log recording from Android logcat and native buffers.
     */
    fun start(context: Context) {
        val appContext = context.applicationContext
        executor.execute {
            try {
                val file = getLogFile(appContext)
                appendLine(file, "=== CatEngine Log Session Started: ${dateFormat.format(Date())} [PID ${Process.myPid()}] ===")

                if (!isLogcatStreaming) {
                    isLogcatStreaming = true
                    startLogcatCapture(file)
                }
            } catch (t: Throwable) {
                Log.e(TAG, "Failed to start logcat capture", t)
            }
        }
    }

    /**
     * Write an immediate structured entry to the log file.
     */
    fun log(tag: String, message: String) {
        executor.execute {
            try {
                val appContext = top.niunaijun.blackbox.BlackBoxCore.getContext() ?: return@execute
                val file = getLogFile(appContext)
                val time = dateFormat.format(Date())
                appendLine(file, "$time [$tag] $message")
            } catch (ignored: Throwable) {}
        }
    }

    private fun appendLine(file: File, line: String) {
        try {
            checkAndRotate(file)
            FileOutputStream(file, true).use { fos ->
                OutputStreamWriter(fos, Charsets.UTF_8).use { writer ->
                    writer.write(line)
                    writer.write("\n")
                    writer.flush()
                }
            }
        } catch (ignored: Throwable) {}
    }

    private fun startLogcatCapture(logFile: File) {
        Thread {
            try {
                // Clear initial logcat buffer to avoid stale historical logs
                try {
                    Runtime.getRuntime().exec("logcat -c").waitFor()
                } catch (ignored: Throwable) {}

                // Capture logs with timestamp
                val pid = Process.myPid().toString()
                val process = Runtime.getRuntime().exec(arrayOf("logcat", "-v", "time", "*:D"))
                val reader = BufferedReader(InputStreamReader(process.inputStream))

                PrintWriter(FileOutputStream(logFile, true)).use { out ->
                    var line: String?
                    while (reader.readLine().also { line = it } != null) {
                        val currentLine = line ?: break
                        // Filter relevant logs for CatEngine, Carrom, BlackBox, or fatal crashes
                        if (currentLine.contains("CatEngine") ||
                            currentLine.contains("Carrom") ||
                            currentLine.contains("NativeCore") ||
                            currentLine.contains("BlackBox") ||
                            currentLine.contains("AndroidRuntime") ||
                            currentLine.contains("FATAL") ||
                            currentLine.contains(pid)
                        ) {
                            checkAndRotate(logFile)
                            out.println(currentLine)
                            out.flush()
                        }
                    }
                }
            } catch (t: Throwable) {
                Log.e(TAG, "Logcat streaming loop stopped", t)
                isLogcatStreaming = false
            }
        }.apply {
            name = "CatEngine-LogcatRecorder"
            isDaemon = true
            start()
        }
    }

    /**
     * Share logs via standard Android system share menu.
     */
    fun shareLogs(context: Context) {
        try {
            val file = getLogFile(context)
            if (!file.exists() || file.length() == 0L) {
                appendLine(file, "=== CatEngine Diagnostics Log Captured on ${dateFormat.format(Date())} ===")
                appendLine(file, "Device: ${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL} (Android ${android.os.Build.VERSION.RELEASE})")
            }

            // Ensure file size within 50 MB
            checkAndRotate(file)

            val authority = "${context.packageName}.cat.FileProvider"
            val uri: Uri = try {
                FileProvider.getUriForFile(context, authority, file)
            } catch (e: Exception) {
                top.niunaijun.blackbox.fake.provider.FileProvider.getUriForFile(context, authority, file)
            }

            val sendIntent = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_STREAM, uri)
                putExtra(Intent.EXTRA_SUBJECT, "CatEngine Diagnostic Log")
                putExtra(Intent.EXTRA_TEXT, "CatEngine Diagnostic & Telemetry Log")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }

            val chooser = Intent.createChooser(sendIntent, "Share CatEngine Logs").apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            context.startActivity(chooser)
        } catch (t: Throwable) {
            Log.e(TAG, "Failed to share logs", t)
        }
    }

    fun shareLogsToTelegram(context: Context) {
        shareLogs(context)
    }
}
