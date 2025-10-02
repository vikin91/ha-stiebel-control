/*
 *  Copyright (C) 2025 Piotr Prygiel
 *
 *  This file is part of ha-stiebel-control.
 *  ha-stiebel-control is free software: : you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(CAN_MESSAGE_MQTT_LOGGER_H)
#define CAN_MESSAGE_MQTT_LOGGER_H

#include "ElsterTable.h"
#include "KElsterTable.h"

// This header provides MQTT publishing functions for CAN messages.
// It integrates with ESPHome's MQTT component to publish all CAN messages
// to a configurable MQTT topic for storage, analysis, or integration with
// Home Assistant and other systems.

// MQTT topic structure:
// Base topic: homeassistant/stiebel/can_raw/
// Published topics:
//   - homeassistant/stiebel/can_raw/<CAN_ID>/<ELSTER_NAME>  (JSON payload)

// Helper to get CAN member name from CAN ID
static inline const char* getCanMemberName(unsigned short can_id) {
    switch (can_id) {
        case 0x680:
        case 680: return "ESPCLIENT";
        
        case 0x180:
        case 180: return "PUMP";
        
        case 0x301:
        case 301: return "FE7X";
        
        case 0x302:
        case 302: return "FEK";
        
        case 0x480:
        case 480: return "MANAGER";
        
        case 0x602:
        case 602: return "FE7";
        
        case 0x100:
        case 100: return "CAN_0x100";
        
        case 0x509:
        case 509: return "CAN_0x509";
        
        case 0x514:
        case 514: return "CAN_0x514";
        
        case 0x601:
        case 601: return "CAN_0x601";
        
        case 0x603:
        case 603: return "CAN_0x603";
        
        case 0x700:
        case 700: return "CAN_0x700";
        
        default: return "UNKNOWN";
    }
}

// Helper to convert raw bytes to hex string
static inline std::string rawBytesToHex(const std::vector<unsigned char>& msg) {
    if (msg.empty()) return "";
    
    char buffer[256];
    int offset = 0;
    for (size_t i = 0; i < msg.size() && offset < 240; i++) {
        offset += sprintf(buffer + offset, "%02x", msg[i]);
        if (i < msg.size() - 1) {
            buffer[offset++] = ' ';
        }
    }
    buffer[offset] = '\0';
    return std::string(buffer);
}

// Track connection state to avoid log spam
static bool mqtt_was_connected = false;
static bool mqtt_warning_shown = false;
static unsigned long last_mqtt_check = 0;

// Publish CAN message to MQTT
// This function should be called from processCanMessage()
// It publishes a JSON payload with all message details
// 
// SAFETY: This function will NEVER crash CAN processing, even if:
// - MQTT is not configured
// - MQTT broker is unavailable
// - Network connection is lost
// - MQTT credentials are wrong
inline void publishCanMessageToMqtt(unsigned short can_id,
                                   const std::vector<unsigned char>& msg,
                                   const ElsterIndex* ei,
                                   const std::string& interpretedValue,
                                   unsigned char byte1,
                                   unsigned char byte2,
                                   int rawValue) {
    // Safety check - null pointer protection
    if (!ei) return;
    
    // Log once to confirm this function is being called
    static bool init_logged = false;
    if (!init_logged) {
        ESP_LOGI("MqttLogger", "CAN MQTT Logger initialized");
        init_logged = true;
    }
    
    // Check if MQTT is configured and connected
    // This uses ESPHome's MQTT component API
    #ifdef USE_MQTT
    if (!id(ha_mqtt).is_connected()) {
        // Only log connection issues periodically to avoid spam
        unsigned long now = millis();
        if (now - last_mqtt_check > 30000) {  // Check every 30 seconds
            last_mqtt_check = now;
            if (mqtt_was_connected) {
                ESP_LOGW("MqttLogger", "MQTT disconnected - CAN messages not being published");
                mqtt_was_connected = false;
            } else if (!mqtt_warning_shown) {
                ESP_LOGI("MqttLogger", "MQTT not connected - waiting for connection");
                mqtt_warning_shown = true;
            }
        }
        return;  // Silently skip if not connected
    }
    
    // Log reconnection (once)
    if (!mqtt_was_connected) {
        ESP_LOGI("MqttLogger", "MQTT connected - publishing CAN messages");
        mqtt_was_connected = true;
    }
    #else
    // MQTT not compiled in - silently skip
    if (!mqtt_warning_shown) {
        ESP_LOGW("MqttLogger", "MQTT not enabled in ESPHome configuration");
        mqtt_warning_shown = true;
    }
    return;
    #endif
    
    // Extract Elster index
    unsigned short elsterIndex = 0;
    if (msg.size() >= 7) {
        if (int(msg[2]) == 0xfa) {
            elsterIndex = int((msg[4]) + ((msg[3]) << 8));
        } else {
            elsterIndex = int(msg[2]);
        }
    }
    
        // Build JSON payload
        // Format: {"timestamp":123456,"sender_can_id":384,"sender_name":"PUMP","elster_idx":90,
        //          "name":"OUTSIDE_TEMP","value":"23.5","type":"dec_val",
        //          "raw_hex":"31 00 5a 00 e8","raw_value":1000}
        
        char json[512];
        int written = snprintf(json, sizeof(json),
                "{\"timestamp\":%lu,"
                "\"sender_can_id\":%u,"
                "\"sender_name\":\"%s\","
                "\"elster_idx\":%u,"
                "\"name\":\"%s\","
                "\"value\":\"%s\","
                "\"type\":\"%s\","
                "\"raw_hex\":\"%s\","
                "\"raw_value\":%d}",
                (unsigned long)(millis()),
                can_id,
                getCanMemberName(can_id),
                elsterIndex,
                ei->EnglishName ? ei->EnglishName : "UNKNOWN",
                interpretedValue.c_str(),
                ElsterTypeToName(ei->Type),
                rawBytesToHex(msg).c_str(),
                rawValue);
    
    // Check for buffer overflow
    if (written < 0 || written >= (int)sizeof(json)) {
        ESP_LOGW("MqttLogger", "JSON payload too large, truncated");
    }
    
    // Build topic: homeassistant/stiebel/can_raw/<CAN_ID_HEX>/<ELSTER_NAME>
    char topic[128];
    int topic_len = snprintf(topic, sizeof(topic), 
            "homeassistant/stiebel/can_raw/%03x/%s",
            can_id,
            ei->EnglishName ? ei->EnglishName : "UNKNOWN");
    
    // Check for topic overflow
    if (topic_len < 0 || topic_len >= (int)sizeof(topic)) {
        ESP_LOGW("MqttLogger", "MQTT topic too long, truncated");
    }
    
    #ifdef USE_MQTT
    // Publish to MQTT using ESPHome's MQTT client
    // QoS 0 = at most once (fire and forget, fastest)
    // retain = false (don't store on broker)
    bool published = id(ha_mqtt).publish(std::string(topic), std::string(json), 0, false);
    
    if (!published) {
        // Publish failed but we don't spam logs
        // This can happen during network issues
        static unsigned long last_publish_error = 0;
        unsigned long now = millis();
        if (now - last_publish_error > 60000) {  // Log once per minute
            ESP_LOGW("MqttLogger", "MQTT publish failed (will retry automatically)");
            last_publish_error = now;
        }
    }
    #endif
}

#endif // CAN_MESSAGE_MQTT_LOGGER_H

