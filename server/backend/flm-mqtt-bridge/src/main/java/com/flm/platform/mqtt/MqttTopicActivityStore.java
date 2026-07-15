package com.flm.platform.mqtt;

import org.springframework.stereotype.Component;
import java.time.Duration;
import java.time.Instant;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

/**
 * In-memory feed of recent MQTT traffic seen by the Java bridge
 * (device topics + $SYS metrics). Not durable across restarts.
 */
@Component
public class MqttTopicActivityStore {

    private static final int MAX_PAYLOAD_CHARS = 4096;
    private static final int MAX_TOPICS = 500;

    public record TopicActivity(
        String topic,
        Instant lastSeenAt,
        long messageCount,
        String lastPayload,
        boolean retained,
        boolean sys
    ) {}

    private static final class MutableActivity {
        volatile Instant lastSeenAt = Instant.EPOCH;
        volatile long messageCount;
        volatile String lastPayload = "";
        volatile boolean retained;
        final boolean sys;

        MutableActivity(boolean sys) {
            this.sys = sys;
        }
    }

    private final ConcurrentHashMap<String, MutableActivity> topics = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, String> sysValues = new ConcurrentHashMap<>();
    private volatile Instant lastMessageAt;

    public void record(String topic, String payload, boolean retained) {
        if (topic == null || topic.isBlank()) return;
        boolean sys = topic.startsWith("$SYS/");
        String clipped = clip(payload);

        topics.compute(topic, (k, existing) -> {
            MutableActivity a = existing != null ? existing : new MutableActivity(sys);
            a.lastSeenAt = Instant.now();
            a.messageCount++;
            a.lastPayload = clipped;
            a.retained = retained;
            return a;
        });
        lastMessageAt = Instant.now();

        if (sys) {
            sysValues.put(topic, clipped);
        }

        if (topics.size() > MAX_TOPICS) {
            trimOldest();
        }
    }

    public List<TopicActivity> listTopics(boolean includeSys, Duration activeWindow) {
        Instant cutoff = activeWindow != null ? Instant.now().minus(activeWindow) : Instant.EPOCH;
        return topics.entrySet().stream()
            .filter(e -> includeSys || !e.getValue().sys)
            .map(e -> toActivity(e.getKey(), e.getValue()))
            .sorted(Comparator
                .comparing((TopicActivity t) -> t.lastSeenAt().isBefore(cutoff) ? 1 : 0)
                .thenComparing(TopicActivity::lastSeenAt, Comparator.reverseOrder()))
            .collect(Collectors.toList());
    }

    public Optional<TopicActivity> getTopic(String topic) {
        MutableActivity a = topics.get(topic);
        return a == null ? Optional.empty() : Optional.of(toActivity(topic, a));
    }

    public Map<String, Object> brokerStats() {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("clientsConnected", intOrNull("$SYS/broker/clients/connected"));
        m.put("clientsMaximum", intOrNull("$SYS/broker/clients/maximum"));
        m.put("clientsTotal", intOrNull("$SYS/broker/clients/total"));
        m.put("messagesReceived", longOrNull("$SYS/broker/messages/received"));
        m.put("messagesSent", longOrNull("$SYS/broker/messages/sent"));
        m.put("bytesReceived", longOrNull("$SYS/broker/bytes/received"));
        m.put("bytesSent", longOrNull("$SYS/broker/bytes/sent"));
        m.put("brokerVersion", sysValues.get("$SYS/broker/version"));
        m.put("brokerUptime", sysValues.get("$SYS/broker/uptime"));
        m.put("trackedTopics", topics.size());
        m.put("activeTopics60s", activeCount(Duration.ofSeconds(60)));
        m.put("lastMessageAt", lastMessageAt != null ? lastMessageAt.toString() : null);
        m.put("sys", Map.copyOf(sysValues));
        return m;
    }

    public int activeCount(Duration window) {
        Instant cutoff = Instant.now().minus(window);
        int n = 0;
        for (MutableActivity a : topics.values()) {
            if (!a.sys && a.lastSeenAt.isAfter(cutoff)) n++;
        }
        return n;
    }

    private Integer intOrNull(String key) {
        String v = sysValues.get(key);
        if (v == null || v.isBlank()) return null;
        try {
            return Integer.parseInt(v.trim());
        } catch (NumberFormatException e) {
            return null;
        }
    }

    private Long longOrNull(String key) {
        String v = sysValues.get(key);
        if (v == null || v.isBlank()) return null;
        try {
            return Long.parseLong(v.trim());
        } catch (NumberFormatException e) {
            return null;
        }
    }

    private static TopicActivity toActivity(String topic, MutableActivity a) {
        return new TopicActivity(topic, a.lastSeenAt, a.messageCount, a.lastPayload, a.retained, a.sys);
    }

    private static String clip(String payload) {
        if (payload == null) return "";
        if (payload.length() <= MAX_PAYLOAD_CHARS) return payload;
        return payload.substring(0, MAX_PAYLOAD_CHARS) + "…";
    }

    private void trimOldest() {
        topics.entrySet().stream()
            .sorted(Comparator.comparing(e -> e.getValue().lastSeenAt))
            .limit(Math.max(1, topics.size() - MAX_TOPICS + 50))
            .map(Map.Entry::getKey)
            .filter(t -> !t.startsWith("$SYS/"))
            .forEach(topics::remove);
    }
}
