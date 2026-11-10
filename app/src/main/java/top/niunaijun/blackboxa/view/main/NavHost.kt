package top.niunaijun.blackboxa.view.main

import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import top.niunaijun.blackboxa.view.apps.AppsViewModel

@Composable
fun NavigationHost(
    viewModel: AppsViewModel
) {
    val navController = rememberNavController()
    val context = LocalContext.current

    NavHost(
        navController = navController,
        startDestination = "home"
    ) {
        composable("home") {
            MainAppScreen(
                viewModel = viewModel,
                onLaunchApp = { packageName ->
                    viewModel.launchInVirtualBox(packageName, 0) { success, msg ->
                        Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
                    }
                },
                onRefresh = {
                    viewModel.fetchAndVerifyApp(context)
                }
            )
        }
    }
}