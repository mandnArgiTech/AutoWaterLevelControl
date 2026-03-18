package com.mandnargitech.fluidlevelmonitor.data.repository
import com.mandnargitech.fluidlevelmonitor.data.api.FluidApi
import com.mandnargitech.fluidlevelmonitor.data.model.*
import com.mandnargitech.fluidlevelmonitor.data.mqtt.MqttRepository
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.*
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class DeviceRepository @Inject constructor(
    private val api: FluidApi,
    private val mqtt: MqttRepository,
) {
    fun statusFlow(intervalMs: Long = 5_000L): Flow<Result<DeviceStatus>> = flow {
        while (true) {
            emit(runCatching { api.getStatus() })
            delay(intervalMs)
        }
    }
    suspend fun getPumpState(): Result<PumpState>     = runCatching { api.getPumpState() }
    suspend fun setPumpState(s: String): Result<Unit> = runCatching { api.setPumpState(mapOf("state" to s)) }
}
