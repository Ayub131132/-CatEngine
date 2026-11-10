package top.niunaijun.blackboxa.view.main

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.core.widget.ContentLoadingProgressBar
import top.niunaijun.blackbox.BlackBoxCore
import top.niunaijun.blackboxa.R
import top.niunaijun.blackboxa.util.InjectionUtil
import top.niunaijun.blackboxa.view.apps.AppsViewModel

class MainActivity : ComponentActivity() {

    private val viewModel: AppsViewModel by viewModels {
        InjectionUtil.getAppsFactory()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        BlackBoxCore.get().onBeforeMainActivityOnCreate(this)

        // Fetch server app config and verify against device
        viewModel.fetchAndVerifyApp(this)

        setContent {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    primary = Color(0xFF5A24C6),
                    surface = Color(0xFF161722),
                    background = Color(0xFF0C0D14)
                )
            ) {
                NavigationHost(viewModel)
            }
        }
    }
}