#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "pins.h"

// A profile ("mode") holds one 24 h curve per channel, as sorted (minute, level%) points.
// Between points the level is interpolated linearly or with a cosine ease ("smooth").
// The curve wraps around midnight. Sunrise = ramp from a low point to a high point,
// sunset = the reverse. Any number of intermediate points gives mid-day peaks, dips etc.
struct SchedPoint { uint16_t minute; float level; };
struct ChannelSchedule { uint8_t n; SchedPoint pts[MAX_POINTS]; };
struct Profile { char name[20]; bool smooth; ChannelSchedule ch[MAX_CHANNELS]; };
struct Profiles { uint8_t active; uint8_t count; Profile p[MAX_PROFILES]; };

extern Profiles profiles;

void  profilesDefaults();
bool  profilesLoad();
bool  profilesSave();
void  profilesToJson(JsonObject o);
void  profilesFromJson(JsonObjectConst o);
float scheduleLevelAt(const ChannelSchedule& s, float minuteOfDay, bool smooth);
