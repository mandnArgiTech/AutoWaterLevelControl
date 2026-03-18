package com.mandnargitech.fluidlevelmonitor.ui.theme

import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Brand palette — aligns with SudarshanChakra design system
val Blue600   = Color(0xFF1E6EF5)
val Blue800   = Color(0xFF0C44B4)
val TankBlue  = Color(0xFF1A4080)
val WaterBlue = Color(0xFF2979FF)
val GreenOk   = Color(0xFF22C55E)
val AmberWarn = Color(0xFFF59E0B)
val RedAlert  = Color(0xFFEF4444)
val Surface0  = Color(0xFF0F1117)
val Surface1  = Color(0xFF161B2E)
val Surface2  = Color(0xFF1C2340)
val OnSurface = Color(0xFFE8EAF6)
val Muted     = Color(0xFF8890AA)

private val DarkColorScheme = darkColorScheme(
    primary        = Blue600,
    onPrimary      = Color.White,
    primaryContainer    = Blue800,
    onPrimaryContainer  = Color(0xFFD0E4FF),
    secondary      = GreenOk,
    onSecondary    = Color(0xFF051A05),
    background     = Surface0,
    onBackground   = OnSurface,
    surface        = Surface1,
    onSurface      = OnSurface,
    surfaceVariant = Surface2,
    outline        = Color(0xFF2A3450),
    error          = RedAlert,
    onError        = Color.White,
)

@Composable
fun FluidLevelMonitorTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        typography  = Typography(),
        content     = content,
    )
}
