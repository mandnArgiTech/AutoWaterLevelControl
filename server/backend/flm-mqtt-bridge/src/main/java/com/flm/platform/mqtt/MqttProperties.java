package com.flm.platform.mqtt;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "flm.mqtt")
public class MqttProperties {
    private String brokerUrl = "tcp://localhost:1883";
    private String clientId = "flm-platform-bridge";
    private String username;
    private String password;
    private boolean tlsEnabled;
    private String caCertPath;
    /** Catch water, system/announce|health, and {tag}/{capability}/telemetry. */
    private String topicFilter = "+/+/#";
    private int qos = 1;

    public String getBrokerUrl() { return brokerUrl; }
    public void setBrokerUrl(String brokerUrl) { this.brokerUrl = brokerUrl; }
    public String getClientId() { return clientId; }
    public void setClientId(String clientId) { this.clientId = clientId; }
    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }
    public String getPassword() { return password; }
    public void setPassword(String password) { this.password = password; }
    public boolean isTlsEnabled() { return tlsEnabled; }
    public void setTlsEnabled(boolean tlsEnabled) { this.tlsEnabled = tlsEnabled; }
    public String getCaCertPath() { return caCertPath; }
    public void setCaCertPath(String caCertPath) { this.caCertPath = caCertPath; }
    public String getTopicFilter() { return topicFilter; }
    public void setTopicFilter(String topicFilter) { this.topicFilter = topicFilter; }
    public int getQos() { return qos; }
    public void setQos(int qos) { this.qos = qos; }
}
