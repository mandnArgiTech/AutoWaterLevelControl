package com.mandnargitech.fluidlevelmonitor.ui.settings

import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mandnargitech.fluidlevelmonitor.ui.theme.*

@Composable
fun SettingsScreen() {
    var deviceAddress  by remember { mutableStateOf("FluidMonitor.local") }
    var lowAlert       by remember { mutableStateOf(true) }
    var highAlert      by remember { mutableStateOf(true) }
    var lowThreshold   by remember { mutableIntStateOf(20) }
    var highThreshold  by remember { mutableIntStateOf(95) }
    var scEnabled      by remember { mutableStateOf(false) }
    var scBroker       by remember { mutableStateOf("") }
    var showRestartDlg by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Surface0)
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text("Settings", fontSize = 22.sp, fontWeight = FontWeight.SemiBold, color = OnSurface)

        // ── Device ────────────────────────────────────────────────────────────
        SectionLabel("Device")
        SettingsCard {
            SettingsInputRow("Device address", deviceAddress) { deviceAddress = it }
            SettingsDivider()
            SettingsValueRow("Poll interval", "5 seconds")
        }

        // ── Alerts ────────────────────────────────────────────────────────────
        SectionLabel("Alerts")
        SettingsCard {
            SettingsToggleRow("Low level alert", lowAlert) { lowAlert = it }
            SettingsDivider()
            SettingsSliderRow("Low threshold", lowThreshold, 5..50, Blue600) { lowThreshold = it }
            SettingsDivider()
            SettingsToggleRow("High level alert", highAlert) { highAlert = it }
            SettingsDivider()
            SettingsSliderRow("High threshold", highThreshold, 60..99, AmberWarn) { highThreshold = it }
            SettingsDivider()
            SettingsValueRow("Alert cooldown", "30 min")
        }

        // ── SudarshanChakra ───────────────────────────────────────────────────
        SectionLabel("SudarshanChakra Integration")
        SettingsCard {
            SettingsToggleRow("Cloud sync", scEnabled) { scEnabled = it }
            SettingsDivider()
            SettingsInputRow("SC Broker URL", scBroker.ifEmpty { "Not configured" }) { scBroker = it }
            SettingsDivider()
            SettingsValueRow("Auth token", if (scEnabled && scBroker.isNotEmpty()) "Configured" else "—")
        }

        if (scEnabled) {
            Card(
                colors = CardDefaults.cardColors(containerColor = Blue600.copy(.1f)),
                border = BorderStroke(1.dp, Blue600.copy(.3f)),
                shape  = RoundedCornerShape(12.dp),
            ) {
                Text(
                    "When SudarshanChakra is enabled, MQTT routes through the SC broker and REST calls go via the SC API gateway. Phases 1–3 work without this enabled.",
                    fontSize = 12.sp, color = Blue600.copy(.8f),
                    modifier = Modifier.padding(12.dp), lineHeight = 18.sp,
                )
            }
        }

        // ── Danger zone ───────────────────────────────────────────────────────
        SectionLabel("Device actions")
        SettingsCard {
            Row(
                Modifier.fillMaxWidth().clickable { showRestartDlg = true }.padding(14.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Restart device", fontSize = 13.sp, color = RedAlert)
                Text("→", fontSize = 14.sp, color = RedAlert)
            }
            SettingsDivider()
            Row(
                Modifier.fillMaxWidth().padding(14.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Factory reset", fontSize = 13.sp, color = RedAlert)
                Text("→", fontSize = 14.sp, color = RedAlert)
            }
        }

        Spacer(Modifier.height(80.dp))
    }

    if (showRestartDlg) {
        AlertDialog(
            onDismissRequest = { showRestartDlg = false },
            title = { Text("Restart device?") },
            text  = { Text("The sensor will be offline for ~10 seconds during reboot.") },
            confirmButton = {
                TextButton(onClick = { showRestartDlg = false }) { Text("Restart", color = RedAlert) }
            },
            dismissButton = {
                TextButton(onClick = { showRestartDlg = false }) { Text("Cancel") }
            },
            containerColor = Surface1,
        )
    }
}

// ─── Shared composables ───────────────────────────────────────────────────────

@Composable
private fun SectionLabel(text: String) {
    Text(text.uppercase(), fontSize = 10.sp, color = Muted, letterSpacing = 0.8.sp,
        modifier = Modifier.padding(start = 4.dp, bottom = 2.dp))
}

@Composable
private fun SettingsCard(content: @Composable ColumnScope.() -> Unit) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Surface1),
        border = BorderStroke(1.dp, Color(0xFF1E2A45)),
        shape  = RoundedCornerShape(14.dp),
    ) {
        Column(Modifier.fillMaxWidth()) { content() }
    }
}

@Composable
private fun SettingsDivider() = Divider(color = Color(0xFF111825), thickness = 0.5.dp)

@Composable
private fun SettingsValueRow(label: String, value: String) {
    Row(
        Modifier.fillMaxWidth().padding(14.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, fontSize = 13.sp, color = OnSurface)
        Text(value, fontSize = 12.sp, color = Muted)
    }
}

@Composable
private fun SettingsInputRow(label: String, value: String, onChange: (String) -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(14.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, fontSize = 13.sp, color = OnSurface)
        BasicTextField(
            value = value,
            onValueChange = onChange,
            textStyle = LocalTextStyle.current.copy(
                fontSize = 12.sp, color = Blue600,
            ),
            modifier = Modifier.widthIn(max = 180.dp),
        )
    }
}

@Composable
private fun SettingsToggleRow(label: String, checked: Boolean, onToggle: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 14.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, fontSize = 13.sp, color = OnSurface)
        Switch(
            checked = checked,
            onCheckedChange = onToggle,
            colors = SwitchDefaults.colors(checkedThumbColor = Color.White, checkedTrackColor = Blue600),
        )
    }
}

@Composable
private fun SettingsSliderRow(
    label: String, value: Int, range: IntRange, color: Color, onChange: (Int) -> Unit,
) {
    Column(Modifier.padding(horizontal = 14.dp, vertical = 8.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, fontSize = 13.sp, color = OnSurface)
            Text("$value%", fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = color)
        }
        Slider(
            value = value.toFloat(),
            onValueChange = { onChange(it.toInt()) },
            valueRange = range.first.toFloat()..range.last.toFloat(),
            colors = SliderDefaults.colors(thumbColor = color, activeTrackColor = color),
            modifier = Modifier.padding(top = 4.dp),
        )
    }
}
