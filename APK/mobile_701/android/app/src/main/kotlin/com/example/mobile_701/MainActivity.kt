package com.jroal.mobile_701_display

import android.Manifest
import android.content.ContentValues
import android.content.pm.PackageManager
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.io.File
import java.io.IOException

class MainActivity : FlutterActivity() {
    private val permissionRequestCode = 701
    private var pendingRecord: Pair<String, MethodChannel.Result>? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "mobile_701/fuel_resets")
            .setMethodCallHandler { call, result ->
                if (call.method != "append") {
                    result.notImplemented()
                } else {
                    val row = call.argument<String>("row")
                    if (row == null) {
                        result.error("invalid_record", "Missing fuel reset record", null)
                    } else if (pendingRecord != null) {
                        result.error("busy", "A fuel reset record is already being saved", null)
                    } else if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q &&
                        ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED
                    ) {
                        pendingRecord = row to result
                        ActivityCompat.requestPermissions(
                            this, arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE), permissionRequestCode
                        )
                    } else {
                        appendRecord(row, result)
                    }
                }
            }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != permissionRequestCode) return
        val pending = pendingRecord ?: return
        pendingRecord = null
        if (grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
            appendRecord(pending.first, pending.second)
        } else {
            pending.second.error("permission_denied", "Storage permission was denied", null)
        }
    }

    private fun appendRecord(row: String, result: MethodChannel.Result) {
        try {
            val header = "DateTimeUTC,FuelEconomyMPG,GallonsRemaining\n"
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                val collection = MediaStore.Downloads.EXTERNAL_CONTENT_URI
                val relativePath = "${Environment.DIRECTORY_DOWNLOADS}/"
                val selection = "${MediaStore.MediaColumns.DISPLAY_NAME} = ? AND ${MediaStore.MediaColumns.RELATIVE_PATH} = ?"
                val cursor = contentResolver.query(
                    collection, arrayOf(MediaStore.MediaColumns._ID), selection,
                    arrayOf("FuelResets.csv", relativePath), null
                ) ?: throw IOException("Could not find FuelResets.csv")
                val uri = cursor.use {
                    if (cursor.moveToFirst()) {
                        android.content.ContentUris.withAppendedId(collection, cursor.getLong(0))
                    } else null
                } ?: contentResolver.insert(collection, ContentValues().apply {
                    put(MediaStore.MediaColumns.DISPLAY_NAME, "FuelResets.csv")
                    put(MediaStore.MediaColumns.MIME_TYPE, "text/csv")
                    put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
                }) ?: throw IOException("Could not create FuelResets.csv")
                val size = contentResolver.openFileDescriptor(uri, "r")?.use { it.statSize }
                    ?: throw IOException("Could not inspect FuelResets.csv")
                contentResolver.openOutputStream(uri, "wa")?.bufferedWriter(Charsets.UTF_8)?.use {
                    if (size == 0L) it.write(header)
                    it.write(row)
                } ?: throw IOException("Could not open FuelResets.csv")
            } else {
                @Suppress("DEPRECATION")
                val file = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "FuelResets.csv")
                val empty = !file.exists() || file.length() == 0L
                file.appendText((if (empty) header else "") + row, Charsets.UTF_8)
            }
            result.success(null)
        } catch (e: IOException) {
            result.error("storage_error", e.message, null)
        } catch (e: SecurityException) {
            result.error("storage_error", e.message, null)
        }
    }
}
