package top.niunaijun.blackboxa.view.main

import androidx.compose.foundation.layout.Column
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable

@Composable
fun ErrorScreen(message: String) {
    Column() {
        Text(text = "Error: $message")
    }
}