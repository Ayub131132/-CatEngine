package top.niunaijun.blackboxa.view.main

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Send
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import top.niunaijun.blackboxa.R
import top.niunaijun.blackboxa.util.DeviceManager
import top.niunaijun.blackboxa.util.LicenseManager
import top.niunaijun.blackboxa.view.apps.AppsViewModel
import top.niunaijun.blackbox.widget.CatEngineComposeDialog
import top.niunaijun.blackboxa.view.legal.FirstLaunchConsentDialog
import top.niunaijun.blackboxa.view.legal.LegalDocType
import top.niunaijun.blackboxa.view.legal.LegalDocViewerDialog
import top.niunaijun.blackboxa.view.legal.LegalManager

// Dark Cyberpunk / Gaming Palette matching the UI screenshot
private val CatBg = Color(0xFF0C0D14)
private val CatCardBg = Color(0xFF161722)
private val CatCardBorder = Color(0xFF222436)
private val CatPillBg = Color(0xFF1B1C2A)
private val CatPurple = Color(0xFF5A24C6)
private val CatPurpleLight = Color(0xFF7038E8)
private val CatPurpleBadge = Color(0xFF4C1D95)
private val CatGreen = Color(0xFF4ADE80)
private val CatTextMuted = Color(0xFF9CA3AF)
private val CatTextDim = Color(0xFF6B7280)

data class SupportedGameItem(
    val id: String,
    val name: String,
    val version: String,
    val packageName: String,
    val iconRes: Int? = null,
    val isAvailable: Boolean = true
)

fun openTelegram(context: Context, target: String) {
    val fullUrl = if (target.startsWith("http://") || target.startsWith("https://")) {
        target
    } else {
        "https://t.me/${target.removePrefix("@")}"
    }

    val tgUri = if (target.contains("?direct")) {
        Uri.parse("tg://resolve?domain=catengine_news&direct")
    } else if (target.startsWith("http://") || target.startsWith("https://")) {
        Uri.parse(target)
    } else {
        Uri.parse("tg://resolve?domain=${target.removePrefix("@")}")
    }

    try {
        val intent = Intent(Intent.ACTION_VIEW, tgUri)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(intent)
    } catch (e: Exception) {
        try {
            val webIntent = Intent(Intent.ACTION_VIEW, Uri.parse(fullUrl))
            webIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(webIntent)
        } catch (ignored: Exception) {}
    }
}

@Composable
fun HomeScreen(
    viewModel: AppsViewModel,
    onLaunchApp: (String) -> Unit = {},
    onRefresh: () -> Unit = {}
) {
    val context = LocalContext.current
    var selectedTab by remember { mutableIntStateOf(0) }
    var showCatEngineSettingsDialog by remember { mutableStateOf(false) }
    var showConsentDialog by remember { mutableStateOf(!LegalManager.isConsentAccepted(context)) }
    var showRedeemDialog by remember { mutableStateOf(false) }
    var showMaintenanceDialog by remember { mutableStateOf(false) }
    var viewingLegalDoc by remember { mutableStateOf<LegalDocType?>(null) }

    val licenseState by viewModel.licenseState
    val isServerOnline by viewModel.isServerOnline

    val appTypeState by viewModel.appTypeState
    val isLaunching by viewModel.isLaunching
    val launchingPackage by viewModel.launchingPackage

    val carromVersion = remember(appTypeState) {
        if (appTypeState.serverVersion.isNotEmpty()) {
            appTypeState.serverVersion
        } else {
            try {
                val pi = context.packageManager.getPackageInfo("com.miniclip.carrom", 0)
                pi.versionName ?: "19.6.0"
            } catch (e: Exception) {
                "19.6.0"
            }
        }
    }

    // Only Carrom Pool supported
    val gamesList = remember(carromVersion) {
        listOf(
            SupportedGameItem(
                id = "carrom",
                name = "Carrom Pool",
                version = carromVersion,
                packageName = if (appTypeState.packageName.isNotEmpty()) appTypeState.packageName else "com.miniclip.carrom",
                iconRes = R.drawable.carrom_icon
            )
        )
    }

    Scaffold(
        containerColor = CatBg,
        bottomBar = {
            CatBottomNavigation(
                selectedTab = selectedTab,
                onTabSelected = { selectedTab = it }
            )
        }
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(horizontal = 16.dp)
        ) {
            Spacer(modifier = Modifier.height(8.dp))

            // Top Header: [Cat Engine User Pill] ... [Telegram]
            CatTopBar(
                title = "Cat Engine",
                onLogoClick = { showCatEngineSettingsDialog = true },
                onTelegramClick = { openTelegram(context, "@catengine_news") }
            )

            Spacer(modifier = Modifier.height(14.dp))

            // Main Content depending on Tab (0: Games, 1: Community/Chat, 2: Settings)
            when (selectedTab) {
                0 -> {
                    // Games Screen
                    GamesScreenTab(
                        games = gamesList,
                        isLaunching = isLaunching,
                        launchingPackage = launchingPackage,
                        onGameClick = { game ->
                            onLaunchApp(game.packageName)
                        }
                    )
                }
                1 -> {
                    // Community / Chat Screen
                    CommunityScreenTab()
                }
                2 -> {
                    // Settings Screen
                    SettingsScreenTab(
                        viewModel = viewModel,
                        onOpenLegalDoc = { doc -> viewingLegalDoc = doc },
                        onOpenRedeem = { showRedeemDialog = true }
                    )
                }
            }
        }
    }

    // Server Maintenance Dialog
    if (showMaintenanceDialog) {
        ServerMaintenanceDialog(
            viewModel = viewModel,
            onDismiss = { showMaintenanceDialog = false }
        )
    }

    // Redeem License Dialog
    if (showRedeemDialog) {
        RedeemLicenseDialog(
            viewModel = viewModel,
            onDismiss = { showRedeemDialog = false }
        )
    }

    // Cat Engine Settings Dialog (opened by tapping user pill / logo)
    if (showCatEngineSettingsDialog) {
        CatEngineComposeDialog(
            onDismiss = { showCatEngineSettingsDialog = false }
        )
    }

    // First-Launch Terms & Privacy Acceptance Modal
    if (showConsentDialog) {
        FirstLaunchConsentDialog(
            onAccept = {
                LegalManager.setConsentAccepted(context, true)
                showConsentDialog = false
            },
            onDecline = {
                (context as? Activity)?.finishAffinity()
            },
            onOpenDoc = { doc ->
                viewingLegalDoc = doc
            }
        )
    }

    // In-App Legal Document Viewer
    viewingLegalDoc?.let { doc ->
        LegalDocViewerDialog(
            docType = doc,
            onDismiss = { viewingLegalDoc = null }
        )
    }
}

/**
 * Top bar:
 * [ (UserIcon) Cat Engine ]                       [ Send / Telegram ]
 */
@Composable
private fun CatTopBar(
    title: String,
    onLogoClick: () -> Unit = {},
    onTelegramClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        // Left User Pill (Clicking opens Cat Engine settings window)
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .clip(RoundedCornerShape(24.dp))
                .background(CatPillBg)
                .border(BorderStroke(1.dp, CatCardBorder), RoundedCornerShape(24.dp))
                .clickable { onLogoClick() }
                .padding(horizontal = 14.dp, vertical = 8.dp)
        ) {
            Icon(
                imageVector = Icons.Outlined.Person,
                contentDescription = "User Icon",
                tint = CatTextMuted,
                modifier = Modifier.size(20.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = title,
                color = Color.White,
                fontSize = 15.sp,
                fontWeight = FontWeight.SemiBold
            )
        }

        // Right Action Icon (Telegram only, bell icon removed)
        IconButton(
            onClick = onTelegramClick,
            modifier = Modifier.size(38.dp)
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Outlined.Send,
                contentDescription = "Official Telegram",
                tint = CatTextMuted,
                modifier = Modifier.size(22.dp)
            )
        }
    }
}

/**
 * Tab 0: Main Games Screen
 */
@Composable
private fun GamesScreenTab(
    games: List<SupportedGameItem>,
    isLaunching: Boolean = false,
    launchingPackage: String? = null,
    onGameClick: (SupportedGameItem) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Section Header: [🎮 Supported Games]                          1
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .clip(RoundedCornerShape(20.dp))
                    .background(CatPillBg)
                    .border(BorderStroke(1.dp, CatCardBorder), RoundedCornerShape(20.dp))
                    .padding(horizontal = 12.dp, vertical = 6.dp)
            ) {
                Icon(
                    imageVector = Icons.Outlined.SportsEsports,
                    contentDescription = "Games",
                    tint = CatTextMuted,
                    modifier = Modifier.size(18.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = "Supported Games",
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium
                )
            }

            Text(
                text = "${games.size}",
                color = CatTextMuted,
                fontSize = 15.sp,
                fontWeight = FontWeight.SemiBold
            )
        }

        // Supported Games Row (Horizontal list of cards)
        BoxWithConstraints(modifier = Modifier.fillMaxWidth()) {
            val count = games.size.coerceAtLeast(1)
            val totalSpacing = 12.dp * (count - 1)
            val cardWidth = if (count == 1) 130.dp else (maxWidth - totalSpacing) / count
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                games.forEach { game ->
                    val isLaunchingThis = isLaunching && (launchingPackage == null || launchingPackage == game.packageName)
                    GameCardItem(
                        game = game,
                        onClick = { onGameClick(game) },
                        modifier = Modifier.width(cardWidth),
                        isLaunching = isLaunching,
                        isLaunchingThis = isLaunchingThis
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(10.dp))
    }
}

/**
 * Single game card in Supported Games row:
 * Dark rounded rectangle card with game icon, purple version pill, and title underneath.
 */
@Composable
private fun GameCardItem(
    game: SupportedGameItem,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    isLaunching: Boolean = false,
    isLaunchingThis: Boolean = false
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(18.dp))
            .clickable(enabled = !isLaunching, onClick = onClick),
        horizontalAlignment = Alignment.Start
    ) {
        // Game container card
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(115.dp)
                .clip(RoundedCornerShape(18.dp))
                .background(CatCardBg)
                .border(BorderStroke(1.dp, if (isLaunchingThis) CatPurpleLight else CatCardBorder), RoundedCornerShape(18.dp))
                .padding(10.dp)
        ) {
            // Icon
            Box(
                modifier = Modifier
                    .size(46.dp)
                    .align(Alignment.TopCenter)
                    .clip(RoundedCornerShape(12.dp))
                    .background(Color(0xFF222436)),
                contentAlignment = Alignment.Center
            ) {
                if (isLaunchingThis) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(24.dp),
                        color = CatPurpleLight,
                        strokeWidth = 2.5.dp
                    )
                } else if (game.iconRes != null) {
                    Image(
                        painter = painterResource(id = game.iconRes),
                        contentDescription = game.name,
                        modifier = Modifier.fillMaxSize()
                    )
                } else {
                    Text(
                        text = "🎯",
                        fontSize = 24.sp
                    )
                }
            }

            // Purple Version Pill
            Box(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .clip(RoundedCornerShape(10.dp))
                    .background(if (isLaunchingThis) CatPurpleLight else CatPurpleBadge)
                    .padding(horizontal = 8.dp, vertical = 3.dp)
            ) {
                Text(
                    text = if (isLaunchingThis) "Launching..." else game.version,
                    color = Color.White,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        Spacer(modifier = Modifier.height(6.dp))

        // Title below card
        Text(
            text = game.name,
            color = Color.White,
            fontSize = 13.sp,
            fontWeight = FontWeight.Medium,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(start = 2.dp)
        )
    }
}

/**
 * Tab 2: Settings Screen
 */
@Composable
private fun SettingsScreenTab(
    viewModel: AppsViewModel,
    onOpenLegalDoc: (LegalDocType) -> Unit = {},
    onOpenRedeem: () -> Unit = {}
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        val context = LocalContext.current
        val rawKey = remember { DeviceManager.getUniqueDeviceKey(context) }
        val formattedKey = remember { DeviceManager.getFormattedDeviceKey(context) }
        val license by viewModel.licenseState

        // Section: Device Identity
        Text(
            text = "DEVICE & LICENSING",
            color = CatTextDim,
            fontSize = 11.5.sp,
            fontWeight = FontWeight.Bold,
            letterSpacing = 1.sp,
            modifier = Modifier.padding(start = 4.dp, top = 4.dp, bottom = 2.dp)
        )

        // Unique 10-Digit Hardware Key (Permanent & Used for Licensing / Purchases)
        SettingsActionCard(
            icon = Icons.Outlined.Fingerprint,
            title = "Device Key / Purchase ID",
            subtitle = "$formattedKey  (Tap to copy)",
            onClick = {
                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                val clip = android.content.ClipData.newPlainText("Device Key", rawKey)
                clipboard.setPrimaryClip(clip)
                Toast.makeText(context, "Copied Device Key: $rawKey", Toast.LENGTH_SHORT).show()
            }
        )

        // License Activation Card
        SettingsActionCard(
            icon = if (license.isLicensed) Icons.Outlined.VerifiedUser else Icons.Outlined.VpnKey,
            title = if (license.isLicensed) "License: ${license.status}" else "Redeem License Key",
            subtitle = if (license.isLicensed) {
                if (license.isLifetime) "Lifetime Access • Tap to view" else "Expires: ${license.expiresAt} • Tap to view"
            } else {
                "Not Activated • Tap to redeem 6-char key"
            },
            onClick = onOpenRedeem
        )

        Spacer(modifier = Modifier.height(4.dp))

        // Section: Legal & Compliance
        Text(
            text = "LEGAL & COMPLIANCE",
            color = CatTextDim,
            fontSize = 11.5.sp,
            fontWeight = FontWeight.Bold,
            letterSpacing = 1.sp,
            modifier = Modifier.padding(start = 4.dp, top = 4.dp, bottom = 2.dp)
        )

        SettingsActionCard(
            icon = Icons.Outlined.Gavel,
            title = "Terms of Service",
            subtitle = "User agreement, acceptable use & liability terms",
            onClick = { onOpenLegalDoc(LegalDocType.TERMS_OF_SERVICE) }
        )

        SettingsActionCard(
            icon = Icons.Outlined.Policy,
            title = "Privacy Policy",
            subtitle = "Device key hashing, zero telemetry & user rights",
            onClick = { onOpenLegalDoc(LegalDocType.PRIVACY_POLICY) }
        )

        SettingsActionCard(
            icon = Icons.Outlined.WarningAmber,
            title = "Disclaimer & Fair Play Notice",
            subtitle = "Non-affiliation notice, ban risks & research use",
            onClick = { onOpenLegalDoc(LegalDocType.DISCLAIMER) }
        )

        SettingsActionCard(
            icon = Icons.Outlined.Security,
            title = "Open Source Licenses",
            subtitle = "BlackBox Apache 2.0 & third-party attributions",
            onClick = { onOpenLegalDoc(LegalDocType.OPEN_SOURCE) }
        )

        Spacer(modifier = Modifier.height(16.dp))
    }
}

/**
 * Single settings card item
 */
@Composable
private fun SettingsActionCard(
    icon: ImageVector,
    title: String,
    subtitle: String,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(CatCardBg)
            .border(BorderStroke(1.dp, CatCardBorder), RoundedCornerShape(16.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = CatTextMuted,
                modifier = Modifier.size(24.dp)
            )
            Spacer(modifier = Modifier.width(16.dp))
            Column {
                Text(
                    text = title,
                    color = Color.White,
                    fontSize = 15.sp,
                    fontWeight = FontWeight.Medium
                )
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = subtitle,
                    color = CatTextMuted,
                    fontSize = 13.sp
                )
            }
        }

        Icon(
            imageVector = Icons.Outlined.ChevronRight,
            contentDescription = "Open",
            tint = CatTextDim,
            modifier = Modifier.size(20.dp)
        )
    }
}

/**
 * Tab 1: Community / Chat Screen
 * Two boxes: Official Channel (@catengine_news) and Direct Support (https://t.me/catengine_news?direct).
 * Clicking either opens Telegram directly.
 */
@Composable
private fun CommunityScreenTab() {
    val context = LocalContext.current
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        SettingsActionCard(
            icon = Icons.AutoMirrored.Outlined.Send,
            title = "Official Channel",
            subtitle = "@catengine_news",
            onClick = { openTelegram(context, "@catengine_news") }
        )

        SettingsActionCard(
            icon = Icons.Outlined.Chat,
            title = "Direct Support",
            subtitle = "t.me/catengine_news?direct",
            onClick = { openTelegram(context, "https://t.me/catengine_news?direct") }
        )
    }
}

/**
 * Bottom Navigation Bar:
 * [ 🎮 Controller ]   [ 💬 Chat ]   [ ⚙️ Settings ]
 */
@Composable
private fun CatBottomNavigation(
    selectedTab: Int,
    onTabSelected: (Int) -> Unit
) {
    Surface(
        color = Color(0xFF13141E),
        shape = RoundedCornerShape(26.dp),
        border = BorderStroke(1.dp, CatCardBorder),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 12.dp)
            .height(64.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = Arrangement.SpaceAround,
            verticalAlignment = Alignment.CenterVertically
        ) {
            CatBottomNavItem(
                icon = Icons.Outlined.SportsEsports,
                isSelected = selectedTab == 0,
                onClick = { onTabSelected(0) }
            )

            CatBottomNavItem(
                icon = Icons.Outlined.ChatBubbleOutline,
                isSelected = selectedTab == 1,
                onClick = { onTabSelected(1) }
            )

            CatBottomNavItem(
                icon = Icons.Outlined.Settings,
                isSelected = selectedTab == 2,
                onClick = { onTabSelected(2) }
            )
        }
    }
}

@Composable
private fun CatBottomNavItem(
    icon: ImageVector,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
        modifier = Modifier
            .clickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = onClick
            )
            .padding(horizontal = 24.dp, vertical = 6.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = if (isSelected) CatGreen else CatTextDim,
            modifier = Modifier.size(24.dp)
        )

        Spacer(modifier = Modifier.height(4.dp))

        // Green indicator line below active tab
        if (isSelected) {
            Box(
                modifier = Modifier
                    .width(22.dp)
                    .height(3.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(CatGreen)
            )
        } else {
            Spacer(modifier = Modifier.height(3.dp))
        }
    }
}

/**
 * Dialog to Redeem and Activate 6-Character License Keys
 */
@Composable
fun RedeemLicenseDialog(
    viewModel: AppsViewModel,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    var keyInput by remember { mutableStateOf("") }
    var isLoading by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var isSuccess by remember { mutableStateOf(false) }

    val rawKey = remember { DeviceManager.getUniqueDeviceKey(context) }
    val formattedKey = remember { DeviceManager.getFormattedDeviceKey(context) }
    val currentLicense = viewModel.licenseState.value

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(0.92f)
                .wrapContentHeight(),
            shape = RoundedCornerShape(22.dp),
            color = Color(0xFF12131D),
            border = BorderStroke(1.dp, Color(0xFF26283B))
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(22.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Header Icon
                Box(
                    modifier = Modifier
                        .size(54.dp)
                        .clip(RoundedCornerShape(27.dp))
                        .background(Color(0xFF1E1F30))
                        .border(BorderStroke(1.dp, Color(0xFF2E314D)), RoundedCornerShape(27.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = if (currentLicense.isLicensed) Icons.Outlined.VerifiedUser else Icons.Outlined.VpnKey,
                        contentDescription = "License",
                        tint = if (currentLicense.isLicensed) CatGreen else CatPurpleLight,
                        modifier = Modifier.size(28.dp)
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))

                Text(
                    text = if (currentLicense.isLicensed) "Active License" else "Redeem License Key",
                    color = Color.White,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )

                Spacer(modifier = Modifier.height(6.dp))

                Text(
                    text = if (currentLicense.isLicensed) {
                        "Your device has an active license (${currentLicense.status}). You can enter a new key below to renew or extend access."
                    } else {
                        "Enter your 6-character license key to activate Cat Engine on this device."
                    },
                    color = CatTextMuted,
                    fontSize = 12.5.sp,
                    textAlign = TextAlign.Center,
                    lineHeight = 17.sp
                )

                Spacer(modifier = Modifier.height(14.dp))

                // Device ID pill with 1-tap copy
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .background(CatCardBg)
                        .border(BorderStroke(1.dp, CatCardBorder), RoundedCornerShape(12.dp))
                        .clickable {
                            val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                            val clip = android.content.ClipData.newPlainText("Device ID", rawKey)
                            clipboard.setPrimaryClip(clip)
                            Toast.makeText(context, "Copied Device ID: $rawKey", Toast.LENGTH_SHORT).show()
                        }
                        .padding(horizontal = 14.dp, vertical = 10.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "Your Device ID (Single-Device Binding)",
                            color = CatTextDim,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Medium
                        )
                        Text(
                            text = formattedKey,
                            color = CatGreen,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(text = "Copy", color = Color(0xFF93C5FD), fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                        Spacer(modifier = Modifier.width(4.dp))
                        Icon(imageVector = Icons.Outlined.ContentCopy, contentDescription = "Copy", tint = Color(0xFF93C5FD), modifier = Modifier.size(16.dp))
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))

                // 6-Character Key Input
                OutlinedTextField(
                    value = keyInput,
                    onValueChange = { input ->
                        if (input.length <= 6) {
                            keyInput = input.uppercase().filter { it.isLetterOrDigit() }
                        }
                    },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    placeholder = {
                        Text(
                            text = "6-CHAR KEY",
                            color = CatTextDim,
                            fontSize = 14.sp,
                            letterSpacing = 2.sp,
                            fontFamily = FontFamily.Monospace,
                            modifier = Modifier.fillMaxWidth(),
                            textAlign = TextAlign.Center
                        )
                    },
                    textStyle = androidx.compose.ui.text.TextStyle(
                        color = Color.White,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        letterSpacing = 4.sp,
                        fontFamily = FontFamily.Monospace,
                        textAlign = TextAlign.Center
                    ),
                    shape = RoundedCornerShape(12.dp),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedBorderColor = CatPurpleLight,
                        unfocusedBorderColor = CatCardBorder,
                        focusedContainerColor = Color(0xFF1B1C2A),
                        unfocusedContainerColor = Color(0xFF161722)
                    ),
                    keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Characters)
                )

                // Status / Error / Success Message
                if (message != null) {
                    Spacer(modifier = Modifier.height(10.dp))
                    Text(
                        text = message ?: "",
                        color = if (isSuccess) CatGreen else Color(0xFFEF4444),
                        fontSize = 12.sp,
                        textAlign = TextAlign.Center,
                        fontWeight = FontWeight.Medium
                    )
                }

                Spacer(modifier = Modifier.height(18.dp))

                // Action Buttons
                Button(
                    onClick = {
                        if (keyInput.length != 6) {
                            message = "License key must be exactly 6 characters"
                            isSuccess = false
                            return@Button
                        }
                        isLoading = true
                        message = null
                        viewModel.redeemLicense(context, keyInput) { success, msg ->
                            isLoading = false
                            isSuccess = success
                            message = msg
                            if (success) {
                                Toast.makeText(context, "Activated successfully!", Toast.LENGTH_SHORT).show()
                            }
                        }
                    },
                    enabled = !isLoading && keyInput.length == 6,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(48.dp),
                    shape = RoundedCornerShape(12.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = CatPurple,
                        disabledContainerColor = Color(0xFF232435),
                        disabledContentColor = CatTextDim
                    )
                ) {
                    if (isLoading) {
                        CircularProgressIndicator(color = Color.White, modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
                    } else {
                        Text(text = "Redeem & Activate", color = Color.White, fontWeight = FontWeight.Bold)
                    }
                }

                Spacer(modifier = Modifier.height(10.dp))

                // Telegram Support Button
                OutlinedButton(
                    onClick = { openTelegram(context, "https://t.me/catengine_news?direct") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(44.dp),
                    shape = RoundedCornerShape(12.dp),
                    border = BorderStroke(1.dp, CatCardBorder),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFF93C5FD))
                ) {
                    Icon(imageVector = Icons.AutoMirrored.Outlined.Send, contentDescription = null, modifier = Modifier.size(16.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Get License on Telegram", fontSize = 13.sp, fontWeight = FontWeight.Medium)
                }

                Spacer(modifier = Modifier.height(6.dp))

                TextButton(
                    onClick = onDismiss,
                    modifier = Modifier.padding(top = 2.dp)
                ) {
                    Text(text = "Close", color = CatTextMuted, fontSize = 13.sp)
                }
            }
        }
    }
}

/**
 * Dialog shown when backend servers are unreachable or undergoing maintenance.
 * Blocks launching the game to protect account integrity and prevent unsynced offset crashes.
 */
@Composable
fun ServerMaintenanceDialog(
    viewModel: AppsViewModel,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    val isChecking by viewModel.isCheckingServer

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(0.92f)
                .wrapContentHeight(),
            shape = RoundedCornerShape(22.dp),
            color = Color(0xFF12131D),
            border = BorderStroke(1.dp, Color(0xFF3B2527))
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(22.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Maintenance Warning Icon
                Box(
                    modifier = Modifier
                        .size(56.dp)
                        .clip(RoundedCornerShape(28.dp))
                        .background(Color(0xFF2B191B))
                        .border(BorderStroke(1.dp, Color(0xFF52282C)), RoundedCornerShape(28.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Outlined.CloudOff,
                        contentDescription = "Maintenance",
                        tint = Color(0xFFF87171),
                        modifier = Modifier.size(30.dp)
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))

                Text(
                    text = "Server Under Maintenance",
                    color = Color.White,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center
                )

                Spacer(modifier = Modifier.height(8.dp))

                Text(
                    text = "Our servers are currently undergoing maintenance or are temporarily unreachable. Game launching is paused to ensure offset integrity and prevent game account detection.",
                    color = CatTextMuted,
                    fontSize = 12.5.sp,
                    textAlign = TextAlign.Center,
                    lineHeight = 18.sp
                )

                Spacer(modifier = Modifier.height(16.dp))

                // Status Info Pill
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .background(Color(0xFF1B1622))
                        .border(BorderStroke(1.dp, Color(0xFF33253D)), RoundedCornerShape(12.dp))
                        .padding(horizontal = 14.dp, vertical = 10.dp),
                    horizontalArrangement = Arrangement.Center,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Box(
                        modifier = Modifier
                            .size(8.dp)
                            .clip(CircleShape)
                            .background(Color(0xFFEF4444))
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Status: Server Offline / Maintenance",
                        color = Color(0xFFFCA5A5),
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium
                    )
                }

                Spacer(modifier = Modifier.height(20.dp))

                // Retry Button
                Button(
                    onClick = {
                        viewModel.fetchAndVerifyApp(context)
                        viewModel.checkServerHealth { isOnline ->
                            if (isOnline) {
                                Toast.makeText(context, "Server connected successfully!", Toast.LENGTH_SHORT).show()
                                onDismiss()
                            } else {
                                Toast.makeText(context, "Server is still unreachable. Please try again shortly.", Toast.LENGTH_SHORT).show()
                            }
                        }
                    },
                    enabled = !isChecking,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(48.dp),
                    shape = RoundedCornerShape(12.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = CatPurple,
                        disabledContainerColor = Color(0xFF232435),
                        disabledContentColor = CatTextDim
                    )
                ) {
                    if (isChecking) {
                        CircularProgressIndicator(color = Color.White, modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
                    } else {
                        Icon(imageVector = Icons.Outlined.Refresh, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(text = "Retry Connection", color = Color.White, fontWeight = FontWeight.Bold)
                    }
                }

                Spacer(modifier = Modifier.height(10.dp))

                // Telegram Updates Button
                OutlinedButton(
                    onClick = { openTelegram(context, "@catengine_news") },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(44.dp),
                    shape = RoundedCornerShape(12.dp),
                    border = BorderStroke(1.dp, CatCardBorder),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFF93C5FD))
                ) {
                    Icon(imageVector = Icons.AutoMirrored.Outlined.Send, contentDescription = null, modifier = Modifier.size(16.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Check Updates on Telegram", fontSize = 13.sp, fontWeight = FontWeight.Medium)
                }

                Spacer(modifier = Modifier.height(6.dp))

                TextButton(onClick = onDismiss) {
                    Text(text = "Close", color = CatTextMuted, fontSize = 13.sp)
                }
            }
        }
    }
}
