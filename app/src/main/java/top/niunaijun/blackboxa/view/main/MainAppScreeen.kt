package top.niunaijun.blackboxa.view.main

import androidx.compose.runtime.Composable
import top.niunaijun.blackboxa.view.apps.AppsViewModel

@Composable
fun MainAppScreen(
    viewModel: AppsViewModel,
    onLaunchApp: (String) -> Unit,
    onRefresh: () -> Unit
) {
    HomeScreen(
        viewModel = viewModel,
        onLaunchApp = onLaunchApp,
        onRefresh = onRefresh
    )
}
