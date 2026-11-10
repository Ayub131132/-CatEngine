package top.niunaijun.blackboxa.view.legal

import android.app.Activity
import android.content.Context
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Gavel
import androidx.compose.material.icons.outlined.Policy
import androidx.compose.material.icons.outlined.Security
import androidx.compose.material.icons.outlined.WarningAmber
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties

// Styling colors matching Cat Engine's Cyber Dark aesthetic
private val ModalBg = Color(0xFF12131D)
private val CardBg = Color(0xFF191A26)
private val BorderColor = Color(0xFF26283B)
private val AccentPurple = Color(0xFF7038E8)
private val AccentGreen = Color(0xFF4ADE80)
private val TextWhite = Color(0xFFF3F4F6)
private val TextMuted = Color(0xFF9CA3AF)
private val TextDim = Color(0xFF6B7280)

enum class LegalDocType(val title: String) {
    TERMS_OF_SERVICE("Terms of Service"),
    PRIVACY_POLICY("Privacy Policy"),
    DISCLAIMER("Disclaimer & Fair Play Notice"),
    OPEN_SOURCE("Open Source Licenses")
}

object LegalManager {
    private const val PREFS_NAME = "cat_engine_legal"
    private const val KEY_CONSENT_ACCEPTED = "consent_accepted_v1"
    private const val KEY_CONSENT_TIMESTAMP = "consent_timestamp_epoch"

    fun isConsentAccepted(context: Context): Boolean {
        val sp = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return sp.getBoolean(KEY_CONSENT_ACCEPTED, false)
    }

    fun setConsentAccepted(context: Context, accepted: Boolean) {
        val sp = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        sp.edit()
            .putBoolean(KEY_CONSENT_ACCEPTED, accepted)
            .putLong(KEY_CONSENT_TIMESTAMP, System.currentTimeMillis())
            .apply()
    }
}

val TERMS_OF_SERVICE_TEXT = """
CAT ENGINE - TERMS OF SERVICE (USER AGREEMENT)
Last Updated: November 2026

1. ACCEPTANCE OF TERMS
By downloading, installing, launching, or using Cat Engine ("the Software", "the Application", "we", "us", or "our"), you ("User" or "you") agree to be bound by these Terms of Service. If you do not agree to these terms in their entirety, you must immediately uninstall and discontinue using the Software.

2. ELIGIBILITY AND AGE REQUIREMENTS
You represent that you are at least 13 years of age (or 16 to 18 where required by applicable local law in your jurisdiction). If you are under the age of majority, you may only use this software under the direct supervision of a parent or legal guardian who agrees to be bound by these Terms.

3. NATURE OF THE SOFTWARE (EDUCATIONAL & RESEARCH UTILITY)
Cat Engine is an independent software application utilizing an isolated Android userspace virtualization environment (built upon the open-source BlackBox virtualization framework) combined with mathematical simulation, physics raycasting, and visual overlay tools. 
The Software is strictly provided for:
- Academic research into userspace Android runtime sandboxing.
- Computer vision, trajectory calculation, and geometric physics simulation testing.
- Offline and sandbox demonstration purposes.

4. INDEPENDENT UTILITY & DISCLAIMER OF AFFILIATION
Cat Engine is an entirely independent third-party project. It is NOT affiliated, endorsed, certified, partnered, or sponsored by Miniclip S.A., Tencent Games, Garena, or any other game developer, publisher, or platform. 
All game names, trademarks, character names, and logos (including "Carrom Pool", "8 Ball Pool", and "Miniclip") belong exclusively to their respective intellectual property owners. Use of these names inside Cat Engine is solely descriptive and nominative to indicate device software compatibility.

5. ASSUMPTION OF RISK & THIRD-PARTY GAME POLICIES
You explicitly acknowledge and agree that:
(a) Online multiplayer game operators (including Miniclip S.A.) maintain strict Terms of Service that may prohibit the use of third-party overlays, virtual containers, or trajectory guideline utilities.
(b) The use of any third-party virtualization software or assistant tools in combination with online game accounts may result in detection, penalties, temporary suspension, permanent account termination, or forfeiture of in-game purchases and rank.
(c) YOU ASSUME 100% OF ALL RISKS ASSOCIATED WITH USING THE SOFTWARE. CAT ENGINE AND ITS DEVELOPERS EXPRESSLY DISCLAIM ANY LIABILITY, LOSS, OR RESPONSIBILITY FOR ANY ACCOUNT PENALTIES, BANS, OR DAMAGES OF ANY KIND ARISING FROM YOUR USE OF THE SOFTWARE.

6. PROHIBITED USES
You agree not to use Cat Engine to:
- Violate any local, national, or international law, regulation, or cybercrime statute.
- Interfere with or attack the servers, API endpoints, or services of Cat Engine or any third-party service provider.
- Redistribute or resell modified builds of Cat Engine containing malware, spyware, or malicious payloads.
- Circumvent or tamper with licensing infrastructure.

7. DISCLAIMER OF WARRANTIES ("AS IS")
THE SOFTWARE IS PROVIDED ON AN "AS IS" AND "AS AVAILABLE" BASIS, WITHOUT WARRANTIES OF ANY KIND, WHETHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, INCLUDING WITHOUT LIMITATION WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE, NON-INFRINGEMENT, OR UNINTERRUPTED AVAILABILITY. 

8. LIMITATION OF LIABILITY
TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, IN NO EVENT SHALL THE DEVELOPERS, CONTRIBUTORS, OR DISTRIBUTORS OF CAT ENGINE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, CONSEQUENTIAL, OR PUNITIVE DAMAGES (INCLUDING LOSS OF PROFITS, DATA, USE, GOODWILL, OR IN-GAME ASSETS), EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

9. SEVERABILITY & MODIFICATIONS
If any provision of these Terms is determined to be unlawful or unenforceable, that provision shall be severed without affecting the validity and enforceability of any remaining provisions. We reserve the right to amend these Terms at any time by updating them within the Software or through our official Telegram communication channels.
""".trimIndent()

val PRIVACY_POLICY_TEXT = """
CAT ENGINE - PRIVACY POLICY
Last Updated: November 2026

We take your privacy seriously. This Privacy Policy details what information Cat Engine processes, how it is handled, and your rights regarding your data.

1. DATA WE COLLECT AND PROCESS
Cat Engine operates with a strict data-minimization approach. We do NOT collect personal identity documents, email addresses, names, or contact lists.

(a) Anonymous 10-Digit Hardware Key:
To authenticate licenses, prevent unauthorized multi-device cloning, and deliver the correct engine offsets, Cat Engine generates a permanent, non-reversible 10-digit numerical identifier. This key is created by hashing the following hardware parameters with SHA-256:
- Android System SSAID (Settings.Secure.ANDROID_ID)
- Hardware Widevine DRM TEE Unique Identifier
- Device hardware profile (Brand, Model, Manufacturer, Device code)
This hash is strictly one-way: it is mathematically impossible to reverse-engineer your hardware identity or personal details from this 10-digit key.

(b) Technical Query Parameters:
When Cat Engine checks for compatible game configurations, it transmits:
- Target Package Name (e.g., com.miniclip.carrom)
- Target Version Code (e.g., 1491)
- The 10-digit Device Key
- Timestamp of interaction

2. DATA WE NEVER COLLECT OR ACCESS
Cat Engine does NOT read, collect, store, or transmit:
- Your real name, phone number, or physical address.
- Your personal contacts, SMS messages, or call logs (all corresponding dangerous permissions have been explicitly removed from the Android Manifest).
- Your GPS coordinates or real-time location.
- Your photos, audio recordings, or personal camera feeds.
- Your account passwords, banking details, or payment card information.

3. HOW WE USE THE DATA
Collected technical parameters are used solely for:
- Authenticating active licenses and authorized device seats.
- Fetching correct memory offsets and layout configurations for the installed game build.
- Maintaining server health, diagnosing crashes, and preventing malicious API abuse.

4. THIRD-PARTY INFRASTRUCTURE & NETWORK SECURITY
API requests are made over secure TLS (HTTPS) to our configured backend endpoints (including Cloudflare secure edge infrastructure). We do not sell, rent, or trade your device key or technical metadata to third-party data brokers or advertising networks.

5. DATA RETENTION AND DELETION
Device keys and interaction timestamps are stored in secure server logs. If you wish to delete your device key registration from our server database, you may submit a request directly through our official Telegram direct link (https://t.me/catengine_news?direct) with your 10-digit Device Key, and the entry will be permanently removed within 72 hours.

6. COMPLIANCE WITH GLOBAL PRIVACY REGULATIONS
Cat Engine respects user privacy rights under the European General Data Protection Regulation (GDPR), the California Consumer Privacy Act (CCPA), and India's Digital Personal Data Protection Act (DPDP). Because no personal names or communications are logged, processing is limited strictly to legitimate technological authentication.

7. CONTACT
For privacy inquiries, questions, or data removal requests, contact our official support via Telegram: https://t.me/catengine_news?direct or join @catengine_news.
""".trimIndent()

val DISCLAIMER_TEXT = """
CAT ENGINE - DISCLAIMER & FAIR PLAY NOTICE
IMPORTANT NOTICE: PLEASE READ CAREFULLY BEFORE USE

1. NON-AFFILIATION NOTICE
Cat Engine is an independent software tool created solely for technology research and demonstration purposes. Cat Engine is NOT endorsed, sponsored, certified, or connected in any manner with Miniclip S.A., Tencent Games, Garena, or any subsidiary or affiliate thereof.
"Carrom Pool" and "8 Ball Pool" are registered trademarks of Miniclip S.A.

2. FAIR PLAY AND GAME INTEGRITY
We respect the intellectual property of game developers and the integrity of online gaming communities. 
- Cat Engine is intended exclusively for offline practice, geometrical physics analysis, and research into algorithmic trajectory calculation.
- We strongly discourage using the Software to disrupt competitive multiplayer environments or gain unfair advantages in tournaments, ranked ladders, or public matchmaking.
- Developers of online games actively employ anti-cheat heuristics and automated telemetry detection. Utilizing virtualization tools or screen overlay guidelines in online modes may trigger automated bans, suspensions, or loss of in-game ranking.

3. SOLE RESPONSIBILITY
By using Cat Engine, you explicitly agree that you are solely responsible for your actions and adherence to the terms and conditions of third-party games and platforms. Cat Engine authors, developers, and distributors disclaim all liability for any consequences resulting from your use of this software.
""".trimIndent()

val OPEN_SOURCE_LICENSES_TEXT = """
OPEN SOURCE SOFTWARE NOTICES AND ATTRIBUTIONS

Cat Engine utilizes open source libraries and components. We gratefully acknowledge the open-source community:

1. BlackBox Virtual Engine
   Copyright (c) niunaijun
   Licensed under the Apache License, Version 2.0 (the "License");
   You may obtain a copy of the License at:
   http://www.apache.org/licenses/LICENSE-2.0

2. Android Jetpack & Jetpack Compose
   Copyright (c) The Android Open Source Project
   Licensed under the Apache License, Version 2.0.

3. Square Retrofit & OkHttp
   Copyright (c) 2013-2024 Square, Inc.
   Licensed under the Apache License, Version 2.0.

4. Google Gson
   Copyright (c) 2008-2024 Google Inc.
   Licensed under the Apache License, Version 2.0.

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
""".trimIndent()

/**
 * Scrollable In-App Document Viewer Dialog
 */
@Composable
fun LegalDocViewerDialog(
    docType: LegalDocType,
    onDismiss: () -> Unit
) {
    val content = when (docType) {
        LegalDocType.TERMS_OF_SERVICE -> TERMS_OF_SERVICE_TEXT
        LegalDocType.PRIVACY_POLICY -> PRIVACY_POLICY_TEXT
        LegalDocType.DISCLAIMER -> DISCLAIMER_TEXT
        LegalDocType.OPEN_SOURCE -> OPEN_SOURCE_LICENSES_TEXT
    }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(0.94f)
                .fillMaxHeight(0.85f),
            shape = RoundedCornerShape(20.dp),
            color = ModalBg,
            border = BorderStroke(1.dp, BorderColor)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(20.dp)
            ) {
                // Header with icon & close button
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = when (docType) {
                                LegalDocType.TERMS_OF_SERVICE -> Icons.Outlined.Gavel
                                LegalDocType.PRIVACY_POLICY -> Icons.Outlined.Policy
                                LegalDocType.DISCLAIMER -> Icons.Outlined.WarningAmber
                                LegalDocType.OPEN_SOURCE -> Icons.Outlined.Security
                            },
                            contentDescription = null,
                            tint = AccentGreen,
                            modifier = Modifier.size(22.dp)
                        )
                        Spacer(modifier = Modifier.width(10.dp))
                        Text(
                            text = docType.title,
                            color = TextWhite,
                            fontSize = 17.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    IconButton(
                        onClick = onDismiss,
                        modifier = Modifier.size(32.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Outlined.Close,
                            contentDescription = "Close",
                            tint = TextMuted
                        )
                    }
                }

                Spacer(modifier = Modifier.height(14.dp))
                HorizontalDivider(color = BorderColor)
                Spacer(modifier = Modifier.height(14.dp))

                // Scrollable Document Body
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .background(CardBg)
                        .padding(14.dp)
                ) {
                    Text(
                        text = content,
                        color = TextWhite.copy(alpha = 0.9f),
                        fontSize = 13.sp,
                        lineHeight = 19.sp,
                        fontFamily = FontFamily.SansSerif,
                        modifier = Modifier.verticalScroll(rememberScrollState())
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))

                Button(
                    onClick = onDismiss,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(46.dp),
                    shape = RoundedCornerShape(12.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = AccentPurple)
                ) {
                    Text(text = "Close", color = Color.White, fontWeight = FontWeight.SemiBold)
                }
            }
        }
    }
}

/**
 * First-Launch Agreement Modal
 * Prompts the user to review and accept the Terms of Service & Privacy Policy before entering the app.
 */
@Composable
fun FirstLaunchConsentDialog(
    onAccept: () -> Unit,
    onDecline: () -> Unit,
    onOpenDoc: (LegalDocType) -> Unit
) {
    var hasAgreed by remember { mutableStateOf(false) }

    Dialog(
        onDismissRequest = { /* Must explicitly accept or decline */ },
        properties = DialogProperties(
            dismissOnBackPress = false,
            dismissOnClickOutside = false,
            usePlatformDefaultWidth = false
        )
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(0.92f)
                .wrapContentHeight(),
            shape = RoundedCornerShape(22.dp),
            color = ModalBg,
            border = BorderStroke(1.dp, BorderColor)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(22.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Security / Shield badge icon
                Box(
                    modifier = Modifier
                        .size(52.dp)
                        .clip(RoundedCornerShape(26.dp))
                        .background(Color(0xFF1E1F30))
                        .border(BorderStroke(1.dp, Color(0xFF2E314D)), RoundedCornerShape(26.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Outlined.Security,
                        contentDescription = "Security",
                        tint = AccentGreen,
                        modifier = Modifier.size(28.dp)
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))

                Text(
                    text = "Welcome to Cat Engine",
                    color = TextWhite,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center
                )

                Spacer(modifier = Modifier.height(8.dp))

                Text(
                    text = "Please review and accept our User Agreement & Privacy Policy to continue.",
                    color = TextMuted,
                    fontSize = 13.sp,
                    textAlign = TextAlign.Center,
                    lineHeight = 18.sp
                )

                Spacer(modifier = Modifier.height(16.dp))

                // Key bullet points box
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .background(CardBg)
                        .border(BorderStroke(1.dp, BorderColor), RoundedCornerShape(12.dp))
                        .padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    ConsentBullet(
                        title = "Independent Research Tool",
                        desc = "Not affiliated with or endorsed by Miniclip S.A. or any game publisher."
                    )
                    ConsentBullet(
                        title = "Account Ban Assumption of Risk",
                        desc = "You assume 100% of the risk if using third-party tools with online game accounts."
                    )
                    ConsentBullet(
                        title = "Privacy & Hardware Hashing",
                        desc = "An anonymous 10-digit device key is used for licensing. No personal files, SMS, or contacts are ever accessed."
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))

                // Clickable links to full docs
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceEvenly
                ) {
                    Text(
                        text = "Terms of Service",
                        color = Color(0xFF93C5FD),
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier
                            .clickable { onOpenDoc(LegalDocType.TERMS_OF_SERVICE) }
                            .padding(4.dp)
                    )
                    Text(
                        text = "•",
                        color = TextDim,
                        fontSize = 12.sp,
                        modifier = Modifier.padding(4.dp)
                    )
                    Text(
                        text = "Privacy Policy",
                        color = Color(0xFF93C5FD),
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier
                            .clickable { onOpenDoc(LegalDocType.PRIVACY_POLICY) }
                            .padding(4.dp)
                    )
                    Text(
                        text = "•",
                        color = TextDim,
                        fontSize = 12.sp,
                        modifier = Modifier.padding(4.dp)
                    )
                    Text(
                        text = "Disclaimer",
                        color = Color(0xFF93C5FD),
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier
                            .clickable { onOpenDoc(LegalDocType.DISCLAIMER) }
                            .padding(4.dp)
                    )
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Checkbox acknowledgment
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { hasAgreed = !hasAgreed }
                        .padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Checkbox(
                        checked = hasAgreed,
                        onCheckedChange = { hasAgreed = it },
                        colors = CheckboxDefaults.colors(
                            checkedColor = AccentGreen,
                            uncheckedColor = TextMuted,
                            checkmarkColor = Color(0xFF0C0D14)
                        )
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "I have read, understood, and agree to the Terms, Privacy Policy, and Fair Play Notice.",
                        color = TextWhite,
                        fontSize = 12.sp,
                        lineHeight = 16.sp
                    )
                }

                Spacer(modifier = Modifier.height(18.dp))

                // Accept & Decline buttons
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    OutlinedButton(
                        onClick = onDecline,
                        modifier = Modifier
                            .weight(1f)
                            .height(46.dp),
                        shape = RoundedCornerShape(12.dp),
                        border = BorderStroke(1.dp, BorderColor),
                        colors = ButtonDefaults.outlinedButtonColors(contentColor = TextMuted)
                    ) {
                        Text(text = "Exit", fontSize = 14.sp)
                    }

                    Button(
                        onClick = onAccept,
                        enabled = hasAgreed,
                        modifier = Modifier
                            .weight(1.4f)
                            .height(46.dp),
                        shape = RoundedCornerShape(12.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = AccentPurple,
                            disabledContainerColor = Color(0xFF2A2B3D),
                            disabledContentColor = TextDim
                        )
                    ) {
                        Text(
                            text = "Accept & Continue",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = if (hasAgreed) Color.White else TextDim
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ConsentBullet(title: String, desc: String) {
    Column {
        Text(
            text = "• $title",
            color = AccentGreen,
            fontSize = 12.sp,
            fontWeight = FontWeight.SemiBold
        )
        Spacer(modifier = Modifier.height(2.dp))
        Text(
            text = desc,
            color = TextMuted,
            fontSize = 11.5.sp,
            lineHeight = 15.sp,
            modifier = Modifier.padding(start = 10.dp)
        )
    }
}
