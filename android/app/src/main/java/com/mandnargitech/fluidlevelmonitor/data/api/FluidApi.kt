package com.mandnargitech.fluidlevelmonitor.data.api
import com.mandnargitech.fluidlevelmonitor.data.model.*
import retrofit2.http.*

interface FluidApi {
    @GET("api/status")   suspend fun getStatus(): DeviceStatus
    @GET("api/level")    suspend fun getLevel(): WaterLevel
    @GET("api/pump")     suspend fun getPumpState(): PumpState
    @POST("api/pump")    suspend fun setPumpState(@Body body: Map<String, String>): Map<String, Any>
    @POST("api/restart") suspend fun restart(): Map<String, Any>
}
