#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// MQTT settings
// =============================================================================
// MQTT topics
static const char* PUBLISH_TOPIC = "testtopics/topic1";
static const char* SUBSCRIBE_TOPIC = "testtopics/topic1";

// Timing intervals (in milliseconds)
#define PUBLISH_INTERVAL_MS   5000
#define WIFI_CHECK_INTERVAL   5000

#endif // CONFIG_H
