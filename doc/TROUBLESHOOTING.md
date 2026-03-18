# FluidLevelMonitor - Troubleshooting Guide

## Table of Contents
1. [Quick Diagnostics](#quick-diagnostics)
2. [Sensor Issues](#sensor-issues)
3. [WiFi Problems](#wifi-problems)
4. [MQTT Issues](#mqtt-issues)
5. [Web Interface Problems](#web-interface-problems)
6. [Calibration Issues](#calibration-issues)
7. [Performance Issues](#performance-issues)
8. [Error Codes](#error-codes)

---

## Quick Diagnostics

### LED Status Check

| LED Pattern | Meaning | Action |
|-------------|---------|--------|
| Solid ON | WiFi connected | Normal operation |
| Slow blink (1Hz) | AP mode | Connect to device AP |
| Fast blink (5Hz) | Connecting | Wait for connection |
| Off | No power / crash | Check power supply |

### Serial Monitor Check

Connect via serial (115200 baud) to see debug output:

```bash
# PlatformIO
pio device monitor

# Arduino IDE
Tools → Serial Monitor → 115200 baud
```

### Health Check API

```bash
curl http://<device-ip>/api/status
```

Check for:
- `freeHeap` > 10000 (memory OK)
- `sensor.valid` = true (sensor working)
- `wifi.connected` = true (network OK)

---

## Sensor Issues

### Problem: Sensor Not Responding

**Symptoms:**
- Error code 100 (ERR_SENSOR_INIT)
- Error code 101 (ERR_SENSOR_TIMEOUT)
- Distance always -1

**Solutions:**

1. **Check wiring:**
   ```
   US-100:
   - VCC → 5V (not 3.3V!)
   - GND → GND
   - RX → D1 (GPIO5)
   - TX → D2 (GPIO4)
   ```

2. **Check US-100 jumper** - must be set for UART mode

3. **Verify pin configuration:**
   ```json
   {
     "sensor": {
       "hardware": {
         "rxPin": 5,
         "txPin": 4
       }
     }
   }
   ```

4. **Test sensor standalone:**
   - Connect to Arduino with example code
   - Verify sensor responds

---

### Problem: Erratic Readings

**Symptoms:**
- Readings jump randomly
- High error rate in sensor status
- Inconsistent water level display

**Solutions:**

1. **Increase filtering:**
   ```json
   {
     "filterEnabled": true,
     "medianFilterSize": 7,
     "movingAvgWindow": 15
   }
   ```

2. **Check for interference:**
   - Move sensor away from motors
   - Use shielded cables
   - Add ferrite beads

3. **Check mounting:**
   - Sensor must be level
   - Keep >5cm from walls
   - Avoid direct sunlight

4. **Verify power supply:**
   - Use quality 5V supply
   - Add capacitor (100µF) near sensor

---

### Problem: Readings Always at Maximum

**Symptoms:**
- Shows maximum distance
- Error code 104 (ERR_SENSOR_DISTANCE_MAX)

**Solutions:**

1. **Check sensor orientation** - must point at water

2. **Check for obstructions:**
   - Clear sensor path
   - Clean sensor face

3. **Verify range:**
   - US-100: max 450cm
   - HC-SR04: max 400cm
   - TF-Luna: max 800cm

4. **Check tank depth** - may exceed sensor range

---

### Problem: Readings Always at Minimum

**Symptoms:**
- Shows minimum distance (2cm)
- Error code 103 (ERR_SENSOR_DISTANCE_MIN)

**Solutions:**

1. **Check for blockage** directly in front of sensor

2. **Check for condensation** on sensor face

3. **Verify mounting height** - sensor may be underwater!

4. **Check calibration offset** - may be negative

---

## WiFi Problems

### Problem: Can't Connect to WiFi

**Symptoms:**
- Device stuck in AP mode
- "Connection failed" in serial log

**Solutions:**

1. **Verify credentials:**
   - Check SSID spelling (case sensitive!)
   - Verify password
   - No special characters issues

2. **Check network:**
   - Router must be 2.4GHz (not 5GHz only)
   - Device must be in range
   - Too many devices on network

3. **Reset WiFi settings:**
   - Factory reset via API: `POST /api/reset`
   - Reconfigure in AP mode

4. **Check serial log for specific error:**
   ```
   [WiFiManager] Connection result: WRONG_PASSWORD
   [WiFiManager] Connection result: NO_AP_FOUND
   ```

---

### Problem: WiFi Disconnects Frequently

**Symptoms:**
- Intermittent connectivity
- MQTT disconnects
- Web interface unavailable

**Solutions:**

1. **Improve signal:**
   - Move closer to router
   - Remove metal obstructions
   - Use WiFi extender

2. **Check power supply:**
   - Unstable power causes WiFi drops
   - Use quality power supply
   - Add capacitor on power input

3. **Reduce interference:**
   - Change router channel
   - Move away from microwave/other 2.4GHz devices

4. **Check router settings:**
   - Disable AP isolation
   - Check DHCP lease time
   - Enable PMF if supported

---

### Problem: Can't Access AP Mode

**Symptoms:**
- Don't see `FluidLM_XXXXXX` network
- Can't connect to 192.168.4.1

**Solutions:**

1. **Wait 30 seconds** after power-on

2. **Check phone/laptop:**
   - Forget old network with similar name
   - Disable mobile data
   - Try different device

3. **Force AP mode:**
   - Edit config to set `apMode: true`
   - Or hold GPIO0 (flash button) during boot

4. **Factory reset:**
   - Erase flash and re-upload
   ```bash
   pio run --target erase
   pio run --target upload
   pio run --target uploadfs
   ```

---

## MQTT Issues

### Problem: MQTT Won't Connect

**Symptoms:**
- Error code 300 (ERR_MQTT_CONNECT)
- "MQTT connection failed" in log

**Solutions:**

1. **Verify broker settings:**
   ```json
   {
     "mqtt": {
       "server": "192.168.1.50",
       "port": 1883,
       "username": "",
       "password": ""
     }
   }
   ```

2. **Test broker:**
   ```bash
   # Test connection
   mosquitto_pub -h 192.168.1.50 -t test -m "hello"
   ```

3. **Check firewall:**
   - Port 1883 must be open
   - Device must be on same network (or port forwarded)

4. **Check authentication:**
   - Verify username/password
   - Check broker ACL rules

---

### Problem: MQTT Messages Not Received

**Symptoms:**
- Connected but no messages in broker
- Home Assistant not updating

**Solutions:**

1. **Verify topic:**
   - Default: `home/water/level`
   - Subscribe manually to verify:
   ```bash
   mosquitto_sub -h broker -t "home/water/#"
   ```

2. **Check publish interval:**
   - Default: 60 seconds
   - Reduce for testing

3. **Force publish:**
   ```bash
   # Via MQTT command
   mosquitto_pub -h broker -t "home/water/command" -m '{"command":"read"}'
   
   # Via API
   curl http://device/api/mqtt/publish
   ```

---

## Web Interface Problems

### Problem: Page Won't Load

**Symptoms:**
- Browser timeout
- "Connection refused"

**Solutions:**

1. **Verify IP address:**
   - Check serial output for IP
   - Use mDNS: `http://fluidmonitor.local`

2. **Check network:**
   - Device and browser must be on same network
   - Not accessible from internet (unless port forwarded)

3. **Clear browser cache:**
   - Force refresh: Ctrl+F5
   - Try incognito mode

4. **Check filesystem:**
   - index.html must be uploaded
   ```bash
   pio run --target uploadfs
   ```

---

### Problem: Configuration Changes Don't Save

**Symptoms:**
- Settings revert after restart
- "Error saving configuration"

**Solutions:**

1. **Check filesystem:**
   - LittleFS must be initialized
   - Check free space

2. **Check permissions:**
   - File not read-only

3. **Verify JSON format:**
   - Use JSON validator
   - Check for special characters

4. **Check serial log** for specific error

---

## Calibration Issues

### Problem: Calibration Not Working

**Symptoms:**
- Level percentage always wrong
- Calibration values don't save

**Solutions:**

1. **Use WebSocket calibration page:**
   - Open Calibration tab
   - Wait for real-time readings to stabilize
   - Enter known distance

2. **Manual calibration via API:**
   ```bash
   curl -X POST http://device/api/config/sensor \
     -H "Content-Type: application/json" \
     -d '{"offsetMm": 50}'
   ```

3. **Understanding offset:**
   ```
   Offset = (Tank height) - (Sensor mounting height from bottom)
   
   Example:
   - Tank height: 170 cm
   - Sensor is 10 cm above tank top
   - When tank is full, sensor reads 10 cm
   - Offset = 100 mm (to account for sensor position)
   ```

---

## Performance Issues

### Problem: High Memory Usage

**Symptoms:**
- `freeHeap` < 10000 bytes
- Random crashes
- Slow response

**Solutions:**

1. **Reduce debug output:**
   ```json
   {
     "system": {
       "debugEnabled": false
     }
   }
   ```

2. **Reduce filter sizes:**
   ```json
   {
     "medianFilterSize": 5,
     "movingAvgWindow": 10
   }
   ```

3. **Increase sensor interval:**
   ```json
   {
     "readInterval": 1000
   }
   ```

4. **Check for memory leaks** - monitor heap over time

---

### Problem: Slow Response

**Symptoms:**
- Web page loads slowly
- API responses delayed

**Solutions:**

1. **Check WiFi signal:**
   - RSSI should be > -70 dBm
   - Move closer to router

2. **Reduce concurrent connections:**
   - Close unused browser tabs
   - Limit MQTT clients

3. **Check serial output** - may be flooding with debug

---

## Error Codes

### Sensor Errors (100-199)

| Code | Name | Description | Solution |
|------|------|-------------|----------|
| 100 | ERR_SENSOR_INIT | Initialization failed | Check wiring, verify pins |
| 101 | ERR_SENSOR_TIMEOUT | Response timeout | Check connection, verify mode |
| 102 | ERR_SENSOR_INVALID_DATA | Bad data received | Check for EMI, verify wiring |
| 103 | ERR_SENSOR_DISTANCE_MIN | Below minimum range | Remove obstruction |
| 104 | ERR_SENSOR_DISTANCE_MAX | Above maximum range | Verify sensor points at water |
| 105 | ERR_SENSOR_TEMP_INVALID | Bad temperature | Sensor specific issue |

### WiFi Errors (200-299)

| Code | Name | Description | Solution |
|------|------|-------------|----------|
| 200 | ERR_WIFI_NO_SSID | No SSID configured | Configure WiFi |
| 201 | ERR_WIFI_CONNECT | Connection failed | Check credentials |
| 202 | ERR_WIFI_TIMEOUT | Connection timeout | Check signal strength |

### MQTT Errors (300-399)

| Code | Name | Description | Solution |
|------|------|-------------|----------|
| 300 | ERR_MQTT_CONNECT | Connection failed | Check broker settings |
| 301 | ERR_MQTT_PUBLISH | Publish failed | Check connection |
| 302 | ERR_MQTT_SUBSCRIBE | Subscribe failed | Check topic permissions |
| 310 | ERR_MQTT_DISABLED | MQTT disabled | Enable in config |

### Configuration Errors (400-499)

| Code | Name | Description | Solution |
|------|------|-------------|----------|
| 400 | ERR_CONFIG_LOAD | Load failed | Check filesystem |
| 401 | ERR_CONFIG_SAVE | Save failed | Check free space |
| 402 | ERR_CONFIG_PARSE | JSON parse error | Validate JSON |
| 403 | ERR_CONFIG_VALIDATE | Validation failed | Check values |

---

## Getting Help

### Before Asking

1. Check serial output for errors
2. Verify wiring connections
3. Try factory reset
4. Update to latest firmware

### Information to Provide

When reporting issues, include:
- Firmware version
- Sensor type
- Serial log output
- Configuration (redact passwords)
- Steps to reproduce

### Resources

- [GitHub Issues](https://github.com/your-repo/issues)
- [User Guide](USER_GUIDE.md)
- [Developer Guide](DEVELOPER_GUIDE.md)

---

*For technical documentation, see the [Developer Guide](DEVELOPER_GUIDE.md).*

