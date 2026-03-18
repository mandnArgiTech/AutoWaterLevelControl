package com.mandnargitech.fluidlevelmonitor.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.*
import com.mandnargitech.fluidlevelmonitor.ui.dashboard.DashboardScreen
import com.mandnargitech.fluidlevelmonitor.ui.pump.PumpControlScreen
import com.mandnargitech.fluidlevelmonitor.ui.sensor.SensorScreen
import com.mandnargitech.fluidlevelmonitor.ui.settings.SettingsScreen
import com.mandnargitech.fluidlevelmonitor.ui.theme.FluidLevelMonitorTheme

sealed class Screen(val route: String, val label: String, val icon: ImageVector) {
    object Dashboard : Screen("dashboard", "Dashboard", Icons.Default.WaterDrop)
    object Pump      : Screen("pump",      "Pump",      Icons.Default.Settings)
    object Sensor    : Screen("sensor",    "Sensor",    Icons.Default.Sensors)
    object Settings  : Screen("settings",  "Settings",  Icons.Default.Tune)
}

private val bottomItems = listOf(
    Screen.Dashboard, Screen.Pump, Screen.Sensor, Screen.Settings,
)

@Composable
fun FluidLevelApp() {
    FluidLevelMonitorTheme {
        val navController = rememberNavController()
        val navBackStackEntry by navController.currentBackStackEntryAsState()
        val currentDestination = navBackStackEntry?.destination

        Scaffold(
            bottomBar = {
                NavigationBar {
                    bottomItems.forEach { screen ->
                        NavigationBarItem(
                            icon     = { Icon(screen.icon, contentDescription = screen.label) },
                            label    = { Text(screen.label) },
                            selected = currentDestination?.hierarchy?.any { it.route == screen.route } == true,
                            onClick  = {
                                navController.navigate(screen.route) {
                                    popUpTo(navController.graph.findStartDestination().id) {
                                        saveState = true
                                    }
                                    launchSingleTop = true
                                    restoreState    = true
                                }
                            },
                        )
                    }
                }
            },
        ) { innerPadding ->
            NavHost(
                navController    = navController,
                startDestination = Screen.Dashboard.route,
            ) {
                composable(Screen.Dashboard.route) {
                    DashboardScreen(
                        onNavigateToPump = { navController.navigate(Screen.Pump.route) },
                    )
                }
                composable(Screen.Pump.route)     { PumpControlScreen() }
                composable(Screen.Sensor.route)   { SensorScreen() }
                composable(Screen.Settings.route) { SettingsScreen() }
            }
        }
    }
}
