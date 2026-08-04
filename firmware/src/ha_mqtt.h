#pragma once
// Home Assistant integration over MQTT discovery (cube-light firmware).
//
// Publishes one HA "device" with four entities:
//   light.cube          on/off + brightness (JSON schema)
//   select.cube_pattern the pattern registry
//   select.cube_preset  saved presets (re-announced when the store changes)
//   text.cube_text      the text-3d message (song titles from automations)
//
// Design notes:
//  - All cube state lives in main.cpp; this class only talks MQTT and calls
//    the hooks. main marks state dirty (markDirty) after local changes so HA
//    stays in sync; a 30s keepalive republish covers anything missed.
//  - Availability topic + LWT so entities gray out when the cube drops.
//  - Reconnect attempts are spaced 30s apart because a dead broker blocks
//    connect() for the TCP timeout (~3s) — a visible render hiccup we don't
//    want every loop.
//  - Discovery is retained; state is not (the cube republishes on connect).

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <functional>

namespace cube {

struct HaMqttHooks {
  std::function<bool()> getPower;
  std::function<void(bool)> setPower;
  std::function<float()> getBrightness;  // 0..1
  std::function<void(float)> setBrightness;
  std::function<String()> getPattern;
  std::function<void(const String&)> setPattern;
  std::function<String()> getPreset;  // "" when no preset is active
  std::function<void(const String&)> loadPreset;
  std::function<String()> getText;
  std::function<void(const String&)> setText;
  std::function<void(JsonArray)> patternOptions;
  std::function<void(JsonArray)> presetOptions;
  // Playlist transport (play/pause/next/prev/shuffle + collection picker).
  std::function<bool()> getPlaylistOn;
  std::function<void(bool)> setPlaylistOn;
  std::function<bool()> getShuffle;
  std::function<void(bool)> setShuffle;
  std::function<void(int)> playlistStep;  // +1 next / -1 prev
  std::function<String()> getPlaylistList;             // "" = all presets
  std::function<void(const String&)> setPlaylistList;  // "" = all presets
  std::function<void(JsonArray)> playlistOptions;      // curated list names
  const char* version = "";
};

// Select-option sentinel for "no curated list" (MQTT select state can't be
// an empty string).
#define HA_ALL_PRESETS "All presets"

class HaMqtt {
 public:
  void begin(const String& host, uint16_t port, const String& user,
             const String& pass, const HaMqttHooks& hooks) {
    host_ = host;
    port_ = port;
    user_ = user;
    pass_ = pass;
    hooks_ = hooks;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[24];
    snprintf(id, sizeof(id), "cubelight-%02x%02x%02x", mac[3], mac[4], mac[5]);
    devId_ = id;
    client_.setClient(net_);
    client_.setServer(host_.c_str(), port_);
    client_.setBufferSize(4096);
    client_.setSocketTimeout(3);
    client_.setCallback([this](char* topic, uint8_t* payload, unsigned int n) {
      onMessage(topic, payload, n);
    });
    enabled_ = host_.length() > 0;
  }

  void markDirty() { dirty_ = true; }
  // Preset list changed: re-announce the preset select's options.
  void refreshDiscovery() { discovered_ = false; }

  const char* status() const {
    if (!enabled_) return "off";
    return connected_ ? "connected" : "connecting";
  }

  void loop() {
    if (!enabled_) return;
    if (!client_.connected()) {
      connected_ = false;
      const uint32_t now = millis();
      if (now - lastAttemptMs_ < 30000 && lastAttemptMs_ != 0) return;
      lastAttemptMs_ = now;
      // LWT: broker flips us offline if we vanish without a clean disconnect.
      if (!client_.connect(devId_.c_str(),
                           user_.length() ? user_.c_str() : nullptr,
                           user_.length() ? pass_.c_str() : nullptr,
                           "cube/avail", 0, true, "offline")) {
        return;
      }
      connected_ = true;
      discovered_ = false;
      client_.subscribe("cube/light/set");
      client_.subscribe("cube/pattern/set");
      client_.subscribe("cube/preset/set");
      client_.subscribe("cube/text/set");
      client_.subscribe("cube/playlist/set");
      client_.subscribe("cube/playlist/next");
      client_.subscribe("cube/playlist/prev");
      client_.subscribe("cube/shuffle/set");
      client_.subscribe("cube/playlist/list/set");
      client_.publish("cube/avail", "online", true);
    }
    client_.loop();
    if (!discovered_) {
      publishDiscovery();
      discovered_ = true;
      dirty_ = true;
    }
    const uint32_t now = millis();
    if (dirty_ || now - lastStateMs_ > 30000) {
      publishState();
      dirty_ = false;
      lastStateMs_ = now;
    }
  }

 private:
  void onMessage(const char* topic, uint8_t* payload, unsigned int n) {
    // Copy: PubSubClient reuses its buffer, and select/text payloads are
    // plain (non-terminated) strings.
    String body;
    body.reserve(n);
    for (unsigned int i = 0; i < n; i++) body += (char)payload[i];
    const String t = topic;
    if (t == "cube/light/set") {
      JsonDocument doc;
      if (deserializeJson(doc, body)) return;
      if (!doc["brightness"].isNull())
        hooks_.setBrightness(constrain(doc["brightness"].as<int>(), 0, 255) / 255.0f);
      if (!doc["state"].isNull()) hooks_.setPower(String(doc["state"] | "ON") == "ON");
    } else if (t == "cube/pattern/set") {
      hooks_.setPattern(body);
    } else if (t == "cube/preset/set") {
      hooks_.loadPreset(body);
    } else if (t == "cube/text/set") {
      hooks_.setText(body);
    } else if (t == "cube/playlist/set") {
      hooks_.setPlaylistOn(body == "ON");
    } else if (t == "cube/playlist/next") {
      hooks_.playlistStep(+1);
    } else if (t == "cube/playlist/prev") {
      hooks_.playlistStep(-1);
    } else if (t == "cube/shuffle/set") {
      hooks_.setShuffle(body == "ON");
    } else if (t == "cube/playlist/list/set") {
      hooks_.setPlaylistList(body == HA_ALL_PRESETS ? String("") : body);
    }
    dirty_ = true;  // echo the new state back promptly
  }

  void deviceBlock(JsonObject dev, bool full) {
    dev["ids"].to<JsonArray>().add(devId_);
    if (full) {
      dev["name"] = "Cube Light";
      dev["mf"] = "cube-light";
      dev["mdl"] = "10x10x10 WS2811 cube";
      dev["sw"] = hooks_.version;
    }
  }

  void publishConfig(const char* component, const char* object, JsonDocument& doc) {
    String topic = "homeassistant/";
    topic += component;
    topic += "/" + devId_ + "/";
    topic += object;
    topic += "/config";
    String out;
    serializeJson(doc, out);
    client_.publish(topic.c_str(), out.c_str(), true);
  }

  void publishDiscovery() {
    {
      JsonDocument doc;
      doc["name"] = "Cube";
      doc["uniq_id"] = devId_ + "_light";
      doc["schema"] = "json";
      doc["brightness"] = true;
      doc["cmd_t"] = "cube/light/set";
      doc["stat_t"] = "cube/light/state";
      doc["avty_t"] = "cube/avail";
      deviceBlock(doc["dev"].to<JsonObject>(), true);
      publishConfig("light", "light", doc);
    }
    {
      JsonDocument doc;
      doc["name"] = "Pattern";
      doc["uniq_id"] = devId_ + "_pattern";
      doc["cmd_t"] = "cube/pattern/set";
      doc["stat_t"] = "cube/pattern/state";
      doc["avty_t"] = "cube/avail";
      hooks_.patternOptions(doc["options"].to<JsonArray>());
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("select", "pattern", doc);
    }
    {
      // HA rejects a select with no options; announce the entity only once
      // at least one preset exists (refreshDiscovery() re-runs this on save).
      JsonDocument doc;
      doc["name"] = "Preset";
      doc["uniq_id"] = devId_ + "_preset";
      doc["cmd_t"] = "cube/preset/set";
      doc["stat_t"] = "cube/preset/state";
      doc["avty_t"] = "cube/avail";
      JsonArray opts = doc["options"].to<JsonArray>();
      hooks_.presetOptions(opts);
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      if (opts.size() > 0)
        publishConfig("select", "preset", doc);
      else
        client_.publish(("homeassistant/select/" + devId_ + "/preset/config").c_str(),
                        "", true);  // empty retained payload removes the entity
    }
    {
      JsonDocument doc;
      doc["name"] = "Text";
      doc["uniq_id"] = devId_ + "_text";
      doc["cmd_t"] = "cube/text/set";
      doc["stat_t"] = "cube/text/state";
      doc["avty_t"] = "cube/avail";
      doc["max"] = 64;
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("text", "text", doc);
    }
    // Playlist transport: play/pause + shuffle as switches, next/prev as
    // buttons, plus the curated-collection picker.
    {
      JsonDocument doc;
      doc["name"] = "Playlist";
      doc["uniq_id"] = devId_ + "_playlist";
      doc["cmd_t"] = "cube/playlist/set";
      doc["stat_t"] = "cube/playlist/state";
      doc["avty_t"] = "cube/avail";
      doc["icon"] = "mdi:playlist-play";
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("switch", "playlist", doc);
    }
    {
      JsonDocument doc;
      doc["name"] = "Shuffle";
      doc["uniq_id"] = devId_ + "_shuffle";
      doc["cmd_t"] = "cube/shuffle/set";
      doc["stat_t"] = "cube/shuffle/state";
      doc["avty_t"] = "cube/avail";
      doc["icon"] = "mdi:shuffle-variant";
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("switch", "shuffle", doc);
    }
    {
      JsonDocument doc;
      doc["name"] = "Next preset";
      doc["uniq_id"] = devId_ + "_next";
      doc["cmd_t"] = "cube/playlist/next";
      doc["avty_t"] = "cube/avail";
      doc["icon"] = "mdi:skip-next";
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("button", "next", doc);
    }
    {
      JsonDocument doc;
      doc["name"] = "Previous preset";
      doc["uniq_id"] = devId_ + "_prev";
      doc["cmd_t"] = "cube/playlist/prev";
      doc["avty_t"] = "cube/avail";
      doc["icon"] = "mdi:skip-previous";
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("button", "prev", doc);
    }
    {
      JsonDocument doc;
      doc["name"] = "Playlist collection";
      doc["uniq_id"] = devId_ + "_playlist_list";
      doc["cmd_t"] = "cube/playlist/list/set";
      doc["stat_t"] = "cube/playlist/list/state";
      doc["avty_t"] = "cube/avail";
      doc["icon"] = "mdi:playlist-star";
      JsonArray opts = doc["options"].to<JsonArray>();
      opts.add(HA_ALL_PRESETS);
      hooks_.playlistOptions(opts);
      deviceBlock(doc["dev"].to<JsonObject>(), false);
      publishConfig("select", "playlist_list", doc);
    }
  }

  void publishState() {
    {
      JsonDocument doc;
      doc["state"] = hooks_.getPower() ? "ON" : "OFF";
      doc["brightness"] = (int)lroundf(constrain(hooks_.getBrightness(), 0.0f, 1.0f) * 255);
      String out;
      serializeJson(doc, out);
      client_.publish("cube/light/state", out.c_str());
    }
    client_.publish("cube/pattern/state", hooks_.getPattern().c_str());
    const String preset = hooks_.getPreset();
    if (preset.length()) client_.publish("cube/preset/state", preset.c_str());
    client_.publish("cube/text/state", hooks_.getText().c_str());
    client_.publish("cube/playlist/state", hooks_.getPlaylistOn() ? "ON" : "OFF");
    client_.publish("cube/shuffle/state", hooks_.getShuffle() ? "ON" : "OFF");
    const String list = hooks_.getPlaylistList();
    client_.publish("cube/playlist/list/state",
                    list.length() ? list.c_str() : HA_ALL_PRESETS);
  }

  WiFiClient net_;
  PubSubClient client_;
  HaMqttHooks hooks_;
  String host_, user_, pass_, devId_;
  uint16_t port_ = 1883;
  bool enabled_ = false;
  bool connected_ = false;
  bool discovered_ = false;
  bool dirty_ = true;
  uint32_t lastAttemptMs_ = 0;
  uint32_t lastStateMs_ = 0;
};

}  // namespace cube
