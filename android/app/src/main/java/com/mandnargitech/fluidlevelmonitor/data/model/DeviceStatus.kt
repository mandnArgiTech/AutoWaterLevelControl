package com.mandnargitech.fluidlevelmonitor.data.model
import com.google.gson.annotations.SerializedName

data class DeviceStatus(
    @SerializedName("device")     val device: String     = "",
    @SerializedName("firmware")   val firmware: String   = "",
    @SerializedName("uptime")     val uptime: String     = "",
    @SerializedName("freeHeap")   val freeHeap: Int      = 0,
    @SerializedName("sensorOk")   val sensorOk: Boolean  = false,
    @SerializedName("sensorType") val sensorType: String = "",
    @SerializedName("level")      val level: WaterLevel  = WaterLevel(),
    @SerializedName("connection") val connection: ConnectionStatus = ConnectionStatus(),
)

data class ConnectionStatus(
    @SerializedName("wifi") val wifi: Boolean = false,
    @SerializedName("mqtt") val mqtt: Boolean = false,
    @SerializedName("ip")   val ip: String    = "",
)

data class PumpState(
    @SerializedName("state")       val state: String  = "off",
    @SerializedName("runSeconds")  val runSeconds: Int = 0,
    @SerializedName("autoEnabled") val autoEnabled: Boolean = false,
)
