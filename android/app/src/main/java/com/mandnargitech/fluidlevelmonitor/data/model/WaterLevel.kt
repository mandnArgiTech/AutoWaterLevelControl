package com.mandnargitech.fluidlevelmonitor.data.model
import com.google.gson.annotations.SerializedName

data class WaterLevel(
    @SerializedName("percentFilled")    val percentFilled: Float    = 0f,
    @SerializedName("percentRemaining") val percentRemaining: Float = 100f,
    @SerializedName("waterHeightCm")    val waterHeightCm: Float    = 0f,
    @SerializedName("volumeLiters")     val volumeLiters: Float     = 0f,
    @SerializedName("volumeRemaining")  val volumeRemaining: Float  = 0f,
    @SerializedName("distanceCm")       val distanceCm: Float       = 0f,
    @SerializedName("temperatureC")     val temperatureC: Float     = 0f,
    @SerializedName("valid")            val valid: Boolean          = false,
    @SerializedName("sensorOk")         val sensorOk: Boolean       = false,
    @SerializedName("state")            val state: String           = "unknown",
    @SerializedName("timestamp")        val timestamp: Long         = 0L,
)

enum class TankState(val label: String) {
    UNKNOWN("Unknown"), EMPTY("Empty"), LEVEL_LOW("Low"),
    LEVEL_MEDIUM("Medium"), LEVEL_HIGH("High"), FULL("Full"), OVERFLOW("Overflow");
    companion object {
        fun from(s: String) = when (s.lowercase()) {
            "empty" -> EMPTY
            "low" -> LEVEL_LOW
            "medium" -> LEVEL_MEDIUM
            "high" -> LEVEL_HIGH
            "full" -> FULL
            "overflow" -> OVERFLOW
            else -> UNKNOWN
        }
    }
}
