package top.niunaijun.blackboxa.util

import top.niunaijun.blackboxa.data.AppsRepository
import top.niunaijun.blackboxa.view.apps.AppsFactory

object InjectionUtil {

    private val appsRepository = AppsRepository()

    fun getAppsFactory(): AppsFactory {
        return AppsFactory(appsRepository)
    }
}
