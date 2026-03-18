package com.mandnargitech.fluidlevelmonitor.ui.pump

import androidx.compose.animation.*
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.mandnargitech.fluidlevelmonitor.ui.theme.*

@Composable
fun PumpControlScreen(viewModel: PumpViewModel = hiltViewModel()) {
    val ui by viewModel.uiState.collectAsStateWithLifecycle()
    val pumpOn = ui.pumpState.state == "on"
    val isAuto = ui.pumpState.state == "auto"

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Surface0)
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Pump Control", fontSize = 22.sp, fontWeight = FontWeight.SemiBold, color = OnSurface)

        // Big pump status card
        PumpStatusCard(
            pumpOn   = pumpOn,
            isAuto   = isAuto,
            runSecs  = ui.pumpState.runSeconds,
            loading  = ui.loading,
            onToggle = {
                viewModel.setMode(if (pumpOn) "off" else "on")
            },
        )

        // Mode selector
        ModeSelector(
            current  = ui.pumpState.state,
            onSelect = viewModel::setMode,
        )

        // Auto thresholds (only shown when auto)
        AnimatedVisibility(
            visible = isAuto,
            enter   = fadeIn() + expandVertically(),
            exit    = fadeOut() + shrinkVertically(),
        ) {
            AutoThresholdCard(
                onPct  = ui.pumpOnPct,
                offPct = ui.pumpOffPct,
                onOnChanged  = viewModel::updateOnThreshold,
                onOffChanged = viewModel::updateOffThreshold,
            )
        }

        // Safety card — always visible
        SafetyCard()

        // Error
        ui.error?.let { err ->
            Card(
                colors = CardDefaults.cardColors(containerColor = Color(0xFF1C0A0A)),
                border = BorderStroke(1.dp, RedAlert.copy(alpha = .4f)),
                shape  = RoundedCornerShape(14.dp),
            ) {
                Row(Modifier.padding(14.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Icon(Icons.Default.Warning, null, tint = RedAlert, modifier = Modifier.size(18.dp))
                    Text(err, color = RedAlert, fontSize = 13.sp)
                }
            }
        }

        Spacer(Modifier.height(80.dp))
    }
}

// ─── Big status card ─────────────────────────────────────────────────────────

@Composable
private fun PumpStatusCard(
    pumpOn: Boolean,
    isAuto: Boolean,
    runSecs: Int,
    loading: Boolean,
    onToggle: () -> Unit,
) {
    val statusColor = when {
        pumpOn  -> GreenOk
        isAuto  -> Blue600
        else    -> Muted
    }
    val statusBg = when {
        pumpOn  -> Color(0xFF0D2014)
        isAuto  -> Color(0xFF0D1520)
        else    -> Surface1
    }
    val statusLabel = when {
        pumpOn  -> "RUNNING"
        isAuto  -> "STANDBY (AUTO)"
        else    -> "STOPPED"
    }

    Card(
        colors = CardDefaults.cardColors(containerColor = statusBg),
        border = BorderStroke(1.5.dp, statusColor.copy(.3f)),
        shape  = RoundedCornerShape(20.dp),
    ) {
        Column(
            Modifier.fillMaxWidth().padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            // Pulse ring + icon
            Box(contentAlignment = Alignment.Center) {
                if (pumpOn) {
                    Box(
                        Modifier
                            .size(92.dp)
                            .clip(CircleShape)
                            .background(GreenOk.copy(alpha = .12f))
                    )
                }
                Box(
                    Modifier
                        .size(76.dp)
                        .clip(CircleShape)
                        .background(statusColor.copy(.18f))
                        .border(2.dp, statusColor, CircleShape),
                    contentAlignment = Alignment.Center,
                ) {
                    Icon(
                        if (pumpOn) Icons.Default.WaterDrop else Icons.Default.WaterDrop,
                        contentDescription = null,
                        tint = statusColor,
                        modifier = Modifier.size(36.dp),
                    )
                }
            }

            Text(statusLabel, fontSize = 22.sp, fontWeight = FontWeight.Bold, color = statusColor)

            if (pumpOn && runSecs > 0) {
                val m = runSecs / 60
                val s = runSecs % 60
                Text(
                    "Runtime: %02d:%02d".format(m, s),
                    fontSize = 13.sp, color = Muted,
                )
            } else {
                Text(
                    if (isAuto) "Will start automatically at threshold" else "Pump is idle",
                    fontSize = 13.sp, color = Muted,
                )
            }

            // Toggle button
            Button(
                onClick  = onToggle,
                enabled  = !loading,
                colors   = ButtonDefaults.buttonColors(
                    containerColor = if (pumpOn) RedAlert else GreenOk,
                ),
                shape    = RoundedCornerShape(24.dp),
                modifier = Modifier.fillMaxWidth(.65f).height(48.dp),
            ) {
                if (loading) {
                    CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp, color = Color.White)
                } else {
                    Text(
                        if (pumpOn) "Stop Pump" else "Start Pump",
                        fontSize = 15.sp, fontWeight = FontWeight.SemiBold, color = Color.White,
                    )
                }
            }
        }
    }
}

// ─── Mode selector ───────────────────────────────────────────────────────────

@Composable
private fun ModeSelector(current: String, onSelect: (String) -> Unit) {
    Text(
        "Operating mode",
        fontSize = 11.sp, color = Muted,
        letterSpacing = 0.8.sp,
        modifier = Modifier.padding(start = 4.dp),
    )
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf(
            Triple("off",  "Always Off", Icons.Default.Stop),
            Triple("auto", "Auto",       Icons.Default.AutoMode),
            Triple("on",   "Always On",  Icons.Default.PlayArrow),
        ).forEach { (mode, label, icon) ->
            val selected = current == mode
            val selColor = when (mode) {
                "off"  -> RedAlert
                "auto" -> Blue600
                else   -> GreenOk
            }
            Card(
                modifier = Modifier.weight(1f).clickable { onSelect(mode) },
                colors = CardDefaults.cardColors(
                    containerColor = if (selected) selColor.copy(.15f) else Surface1,
                ),
                border = BorderStroke(1.5.dp, if (selected) selColor else Color(0xFF1E2A45)),
                shape  = RoundedCornerShape(14.dp),
            ) {
                Column(
                    Modifier.padding(12.dp).fillMaxWidth(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    Icon(icon, null,
                        tint = if (selected) selColor else Muted,
                        modifier = Modifier.size(22.dp),
                    )
                    Text(label,
                        fontSize = 11.sp,
                        color = if (selected) selColor else Muted,
                        fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
                    )
                }
            }
        }
    }
}

// ─── Auto thresholds ─────────────────────────────────────────────────────────

@Composable
private fun AutoThresholdCard(
    onPct: Int, offPct: Int,
    onOnChanged: (Int) -> Unit,
    onOffChanged: (Int) -> Unit,
) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Surface1),
        border = BorderStroke(1.dp, Color(0xFF1E2A45)),
        shape  = RoundedCornerShape(14.dp),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Auto thresholds", fontSize = 11.sp, color = Muted, letterSpacing = 0.5.sp)

            ThresholdRow("Start pump below", onPct, 5..50, Blue600) { onOnChanged(it) }
            ThresholdRow("Stop pump above",  offPct, 50..99, GreenOk) { onOffChanged(it) }
        }
    }
}

@Composable
private fun ThresholdRow(
    label: String,
    value: Int,
    range: IntRange,
    color: Color,
    onChange: (Int) -> Unit,
) {
    Column {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(label, fontSize = 13.sp, color = OnSurface.copy(.8f))
            Text("$value%", fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = color)
        }
        Slider(
            value  = value.toFloat(),
            onValueChange = { onChange(it.toInt()) },
            valueRange     = range.first.toFloat()..range.last.toFloat(),
            colors = SliderDefaults.colors(thumbColor = color, activeTrackColor = color),
        )
    }
}

// ─── Safety card ─────────────────────────────────────────────────────────────

@Composable
private fun SafetyCard() {
    Card(
        colors = CardDefaults.cardColors(containerColor = Color(0xFF150D05)),
        border = BorderStroke(1.dp, AmberWarn.copy(.3f)),
        shape  = RoundedCornerShape(14.dp),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Safety limits", fontSize = 11.sp, color = AmberWarn.copy(.7f), letterSpacing = 0.5.sp)
            SafetyRow("Max run time", "30 min")
            SafetyRow("Dry-run guard", "Stops if level < 5%")
            SafetyRow("Concurrent control", "Device + App in sync")
            Text(
                "⚠  Safety cutoffs are enforced by the firmware and cannot be overridden from the app.",
                fontSize = 11.sp, color = AmberWarn.copy(.5f), lineHeight = 16.sp,
            )
        }
    }
}

@Composable
private fun SafetyRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, fontSize = 12.sp, color = OnSurface.copy(.6f))
        Text(value, fontSize = 12.sp, fontWeight = FontWeight.SemiBold, color = AmberWarn)
    }
}
