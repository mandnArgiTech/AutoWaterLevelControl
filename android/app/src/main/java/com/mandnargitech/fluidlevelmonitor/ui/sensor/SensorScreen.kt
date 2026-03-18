package com.mandnargitech.fluidlevelmonitor.ui.sensor

import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
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
import com.mandnargitech.fluidlevelmonitor.ui.dashboard.DashboardViewModel
import com.mandnargitech.fluidlevelmonitor.ui.theme.*

@Composable
fun SensorScreen(viewModel: DashboardViewModel = hiltViewModel()) {
    val ui by viewModel.uiState.collectAsStateWithLifecycle()
    val status = ui.status

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Surface0)
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Sensor Status", fontSize = 22.sp, fontWeight = FontWeight.SemiBold, color = OnSurface)

        // Connectivity
        InfoCard(title = "Connectivity") {
            ConnRow("WiFi", status?.connection?.ip ?: "—",
                if (status?.connection?.wifi == true) "Connected" else "Disconnected",
                status?.connection?.wifi == true)
            ConnRow("MQTT", "broker",
                if (status?.connection?.mqtt == true) "Connected" else "Disconnected",
                status?.connection?.mqtt == true)
        }

        // Sensor readings
        InfoCard(title = "Sensor — ${status?.sensorType ?: "Unknown"}") {
            KVRow("Status",      if (status?.sensorOk == true) "OK" else "Not responding")
            KVRow("Distance",    status?.level?.distanceCm?.let { "${"%.1f".format(it)} cm" } ?: "—")
            KVRow("Temperature", status?.level?.temperatureC?.let { "${"%.1f".format(it)}°C" } ?: "—")
        }

        // Filter pipeline visual
        InfoCard(title = "Filter pipeline") {
            FilterStage("Median filter",      Blue600,   "Spike removal")
            FilterStage("Moving average",     GreenOk,   "Ripple smoothing")
            FilterStage("Kalman filter",      Color(0xFFA855F7), "Optimal estimation")
        }

        // System info
        InfoCard(title = "System") {
            KVRow("Firmware",  status?.firmware ?: "—")
            KVRow("Uptime",    status?.uptime ?: "—")
            KVRow("Free heap", status?.freeHeap?.let { "$it B" } ?: "—")
        }

        Spacer(Modifier.height(80.dp))
    }
}

@Composable
private fun InfoCard(title: String, content: @Composable ColumnScope.() -> Unit) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Surface1),
        border = BorderStroke(1.dp, Color(0xFF1E2A45)),
        shape  = RoundedCornerShape(14.dp),
    ) {
        Column(Modifier.fillMaxWidth().padding(16.dp), verticalArrangement = Arrangement.spacedBy(0.dp)) {
            Text(title.uppercase(), fontSize = 10.sp, color = Muted, letterSpacing = 0.6.sp,
                modifier = Modifier.padding(bottom = 10.dp))
            content()
        }
    }
}

@Composable
private fun KVRow(key: String, value: String) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 7.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(key, fontSize = 13.sp, color = Muted)
        Text(value, fontSize = 13.sp, fontWeight = FontWeight.Medium, color = OnSurface)
    }
    Divider(color = Color(0xFF131825), thickness = 0.5.dp)
}

@Composable
private fun ConnRow(label: String, detail: String, status: String, ok: Boolean) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Box(Modifier.size(9.dp).clip(CircleShape).background(if (ok) GreenOk else RedAlert))
        Column(Modifier.weight(1f)) {
            Text(label, fontSize = 13.sp, color = OnSurface, fontWeight = FontWeight.Medium)
            Text(detail, fontSize = 11.sp, color = Muted)
        }
        Text(status, fontSize = 11.sp, color = if (ok) GreenOk else RedAlert,
            fontWeight = FontWeight.SemiBold)
    }
    Divider(color = Color(0xFF131825), thickness = 0.5.dp)
}

@Composable
private fun FilterStage(name: String, color: Color, description: String) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Box(Modifier.size(10.dp).clip(CircleShape).background(color))
        Column(Modifier.weight(1f)) {
            Text(name, fontSize = 13.sp, color = OnSurface)
            Text(description, fontSize = 11.sp, color = Muted)
        }
        Text("Active", fontSize = 11.sp, color = color, fontWeight = FontWeight.SemiBold)
    }
    Divider(color = Color(0xFF131825), thickness = 0.5.dp)
}
