package com.mandnargitech.fluidlevelmonitor.ui.dashboard

import androidx.compose.animation.core.*
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.mandnargitech.fluidlevelmonitor.data.model.TankState
import com.mandnargitech.fluidlevelmonitor.data.model.WaterLevel
import com.mandnargitech.fluidlevelmonitor.ui.theme.*

@Composable
fun DashboardScreen(
    onNavigateToPump: () -> Unit,
    viewModel: DashboardViewModel = hiltViewModel(),
) {
    val ui by viewModel.uiState.collectAsStateWithLifecycle()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Surface0)
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        // Header
        DashboardHeader(
            deviceName = ui.status?.connection?.ip ?: "Connecting…",
            wifiOk = ui.status?.connection?.wifi == true,
        )

        // Low-level alert banner
        if (ui.showLowAlert) {
            LowLevelAlert(
                percent = ui.status?.level?.percentFilled ?: 0f,
                onNavigateToPump = onNavigateToPump,
            )
        }

        // Tank graphic + percentage
        TankCard(level = ui.status?.level, tankState = ui.tankState)

        // 4-cell stats grid
        ui.status?.level?.let { lvl ->
            StatsGrid(level = lvl, sensorType = ui.status?.sensorType ?: "")
        }

        // Quick pump control
        PumpQuickCard(
            pumpRunning = false, // will reflect RelayManager state (Phase 2)
            onNavigateToPump = onNavigateToPump,
        )

        // Error state
        ui.error?.let { err ->
            Card(
                colors = CardDefaults.cardColors(containerColor = Color(0xFF1C0A0A)),
                border = BorderStroke(1.dp, RedAlert.copy(alpha = .4f)),
                shape = RoundedCornerShape(14.dp),
            ) {
                Row(Modifier.padding(14.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Icon(Icons.Default.Warning, null, tint = RedAlert, modifier = Modifier.size(18.dp))
                    Text(err, color = RedAlert, fontSize = 13.sp)
                }
            }
        }

        Spacer(modifier = Modifier.height(80.dp))
    }
}

// ─── Header ──────────────────────────────────────────────────────────────────

@Composable
private fun DashboardHeader(deviceName: String, wifiOk: Boolean) {
    Row(
        Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Column {
            Text("Water Tank", fontSize = 22.sp, fontWeight = FontWeight.SemiBold, color = OnSurface)
            Text(deviceName, fontSize = 12.sp, color = Muted)
        }
        Box(
            Modifier
                .size(10.dp)
                .clip(RoundedCornerShape(50))
                .background(if (wifiOk) GreenOk else RedAlert)
        )
    }
}

// ─── Alert banner ────────────────────────────────────────────────────────────

@Composable
private fun LowLevelAlert(percent: Float, onNavigateToPump: () -> Unit) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Color(0xFF1C1000)),
        border = BorderStroke(1.dp, AmberWarn.copy(alpha = .5f)),
        shape = RoundedCornerShape(14.dp),
    ) {
        Row(
            Modifier.padding(14.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(Icons.Default.Warning, null, tint = AmberWarn, modifier = Modifier.size(20.dp))
            Column(Modifier.weight(1f)) {
                Text("Low water level", color = AmberWarn, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                Text(
                    "Tank at ${percent.toInt()}% — consider starting pump",
                    color = AmberWarn.copy(alpha = .7f), fontSize = 12.sp,
                )
            }
            TextButton(onClick = onNavigateToPump) {
                Text("Pump →", color = AmberWarn, fontSize = 12.sp)
            }
        }
    }
}

// ─── Tank card ───────────────────────────────────────────────────────────────

@Composable
private fun TankCard(level: WaterLevel?, tankState: TankState) {
    val percent = level?.percentFilled ?: 0f
    val animatedPct by animateFloatAsState(
        targetValue = percent / 100f,
        animationSpec = tween(1200, easing = EaseInOutQuad),
        label = "tank_fill",
    )

    Card(
        colors = CardDefaults.cardColors(containerColor = Surface1),
        border = BorderStroke(1.dp, Color(0xFF1E2A45)),
        shape = RoundedCornerShape(20.dp),
    ) {
        Column(
            Modifier.fillMaxWidth().padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            // Circular tank visual
            Box(contentAlignment = Alignment.Center, modifier = Modifier.size(160.dp)) {
                CircularTankIndicator(fill = animatedPct, state = tankState)
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        "${percent.toInt()}%",
                        fontSize = 36.sp, fontWeight = FontWeight.Bold, color = OnSurface,
                    )
                    Text(tankState.label, fontSize = 12.sp, color = stateColor(tankState))
                }
            }

            // Volume row
            if (level != null && level.sensorOk) {
                Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceEvenly,
                ) {
                    MiniStat("Filled", "${level.volumeLiters.toInt()} L")
                    Divider(Modifier.width(1.dp).height(32.dp), color = Color(0xFF1E2A45))
                    MiniStat("Remaining", "${level.volumeRemaining.toInt()} L")
                    Divider(Modifier.width(1.dp).height(32.dp), color = Color(0xFF1E2A45))
                    MiniStat("Height", "${"%.1f".format(level.waterHeightCm)} cm")
                }
            }
        }
    }
}

@Composable
private fun CircularTankIndicator(fill: Float, state: TankState) {
    val trackColor = Color(0xFF1E2A45)
    val fillColor  = stateColor(state)
    val sweepAngle = fill * 300f   // 300° arc (leaving 60° gap at bottom)

    Canvas(modifier = Modifier.size(160.dp)) {
        val strokeWidth = 14.dp.toPx()
        val inset = strokeWidth / 2
        val arcSize = androidx.compose.ui.geometry.Size(
            size.width - strokeWidth,
            size.height - strokeWidth,
        )

        // Track
        drawArc(
            color = trackColor,
            startAngle = 120f, sweepAngle = 300f,
            useCenter = false,
            topLeft = androidx.compose.ui.geometry.Offset(inset, inset),
            size = arcSize,
            style = Stroke(strokeWidth, cap = StrokeCap.Round),
        )
        // Fill
        if (sweepAngle > 0f) {
            drawArc(
                brush = Brush.sweepGradient(
                    listOf(fillColor.copy(alpha = .7f), fillColor),
                ),
                startAngle = 120f, sweepAngle = sweepAngle,
                useCenter = false,
                topLeft = androidx.compose.ui.geometry.Offset(inset, inset),
                size = arcSize,
                style = Stroke(strokeWidth, cap = StrokeCap.Round),
            )
        }
    }
}

@Composable
private fun MiniStat(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(value, fontSize = 14.sp, fontWeight = FontWeight.SemiBold, color = OnSurface)
        Text(label, fontSize = 10.sp, color = Muted)
    }
}

// ─── Stats grid ──────────────────────────────────────────────────────────────

@Composable
private fun StatsGrid(level: WaterLevel, sensorType: String) {
    Row(
        Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        StatCell(Modifier.weight(1f), "Distance", "${"%.1f".format(level.distanceCm)} cm", sensorType)
        StatCell(Modifier.weight(1f), "Temperature", "${"%.1f".format(level.temperatureC)}°C", "sensor")
    }
}

@Composable
private fun StatCell(modifier: Modifier, label: String, value: String, sub: String) {
    Card(
        modifier = modifier,
        colors = CardDefaults.cardColors(containerColor = Surface1),
        border = BorderStroke(1.dp, Color(0xFF1E2A45)),
        shape = RoundedCornerShape(14.dp),
    ) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(label, fontSize = 10.sp, color = Muted, letterSpacing = 0.5.sp)
            Text(value, fontSize = 20.sp, fontWeight = FontWeight.Bold, color = OnSurface)
            Text(sub, fontSize = 10.sp, color = Color(0xFF3A4060))
        }
    }
}

// ─── Pump quick ──────────────────────────────────────────────────────────────

@Composable
private fun PumpQuickCard(pumpRunning: Boolean, onNavigateToPump: () -> Unit) {
    Card(
        colors = CardDefaults.cardColors(
            containerColor = if (pumpRunning) Color(0xFF0D200D) else Color(0xFF0D1520),
        ),
        border = BorderStroke(1.dp, if (pumpRunning) GreenOk.copy(.3f) else Color(0xFF1E2A45)),
        shape = RoundedCornerShape(14.dp),
    ) {
        Row(
            Modifier.fillMaxWidth().padding(14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column {
                Text(
                    if (pumpRunning) "🟢  Pump — Running" else "🔴  Pump — Stopped",
                    fontWeight = FontWeight.SemiBold, fontSize = 14.sp, color = OnSurface,
                )
                Text(
                    "Auto mode · tap to manage",
                    fontSize = 11.sp, color = Muted, modifier = Modifier.padding(top = 2.dp),
                )
            }
            FilledTonalButton(
                onClick = onNavigateToPump,
                colors = ButtonDefaults.filledTonalButtonColors(containerColor = Blue600),
                shape = RoundedCornerShape(20.dp),
            ) {
                Text("Pump →", color = Color.White, fontSize = 12.sp)
            }
        }
    }
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

private fun stateColor(state: TankState) = when (state) {
    TankState.EMPTY, TankState.LEVEL_LOW -> AmberWarn
    TankState.LEVEL_MEDIUM               -> WaterBlue
    TankState.LEVEL_HIGH, TankState.FULL -> GreenOk
    TankState.OVERFLOW                   -> RedAlert
    TankState.UNKNOWN                    -> Muted
}
