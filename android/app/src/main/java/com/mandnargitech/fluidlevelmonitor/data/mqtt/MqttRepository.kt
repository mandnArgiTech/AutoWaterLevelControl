package com.mandnargitech.fluidlevelmonitor.data.mqtt
import com.google.gson.Gson
import com.hivemq.client.mqtt.MqttGlobalPublishFilter
import com.hivemq.client.mqtt.mqtt3.Mqtt3Client
import com.mandnargitech.fluidlevelmonitor.data.model.WaterLevel
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.callbackFlow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class MqttRepository @Inject constructor() {
    private val gson = Gson()
    private val client by lazy {
        Mqtt3Client.builder()
            .identifier("FluidLM_Android")
            .buildAsync()
    }

    fun connect(host: String, port: Int, deviceTag: String) = callbackFlow<WaterLevel> {
        val topicFilter = "$deviceTag/water/#"
        val levelSuffix = "/level"
        client.connectWith().serverHost(host).serverPort(port).cleanSession(true).send()
            .thenAccept {
                client.subscribeWith().topicFilter(topicFilter).send()
                client.publishes(MqttGlobalPublishFilter.SUBSCRIBED) { pub ->
                    if (pub.topic.toString().endsWith(levelSuffix)) {
                        runCatching {
                            val level = gson.fromJson(String(pub.payloadAsBytes), WaterLevel::class.java)
                            trySend(level)
                        }
                    }
                }
            }
        awaitClose { client.disconnect() }
    }

    fun publishCommand(deviceTag: String, cmd: String) {
        val payload = "{\"command\":\"$cmd\"}"
        val topic = "$deviceTag/water/command"
        client.publishWith().topic(topic).payload(payload.toByteArray()).send()
    }
}
