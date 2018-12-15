#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NeoPixelBus.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include "credentials.h"

#define pwm_freq 1024

// WiFi credentials, as defined in credentials.h.
// Feel free to remove credentials.h and just put them here directly.
// #define wifi_ssid WIFI_SSID
// #define wifi_password WIFI_PASSWORD

// MQTT credentials, as defined in credentials.h
// Feel free to remove credentials.h and just put them here directly.
#define mqtt_server "192.168.1.101"
#define mqtt_port "1883"
#define mqtt_user ""
#define mqtt_password ""

// MQTT settings
// CHANGE THESE TO SUIT YOUR DEVICE!
#define client_name "desktop"
#define command_topic "tenbar/abedroom/desktop/command"
#define state_topic "tenbar/abedroom/desktop/state"
#define availability_topic "tenbar/abedroom/desktop/availability"

// Receive and send white channel over the network
#define feature_white true

// Generated using curve_generator.py
const uint8_t dimming_curve[256] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x08, 0x08,
0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f, 0x0f,
0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x14, 0x14, 0x15, 0x15, 0x16, 0x17, 0x17, 0x18, 0x18,
0x19, 0x19, 0x1a, 0x1b, 0x1b, 0x1c, 0x1d, 0x1d, 0x1e, 0x1f, 0x1f, 0x20, 0x21, 0x21, 0x22, 0x23, 0x23,
0x24, 0x25, 0x25, 0x26, 0x27, 0x28, 0x28, 0x29, 0x2a, 0x2b, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x2f, 0x30,
0x31, 0x32, 0x33, 0x34, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3c, 0x3d, 0x3e, 0x3f,
0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
0x51, 0x52, 0x53, 0x54, 0x55, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x61, 0x62, 0x63,
0x64, 0x65, 0x66, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6e, 0x6f, 0x70, 0x71, 0x73, 0x74, 0x75, 0x76, 0x78,
0x79, 0x7a, 0x7c, 0x7d, 0x7e, 0x80, 0x81, 0x82, 0x84, 0x85, 0x86, 0x88, 0x89, 0x8a, 0x8c, 0x8d, 0x8e,
0x90, 0x91, 0x93, 0x94, 0x96, 0x97, 0x98, 0x9a, 0x9b, 0x9d, 0x9e, 0xa0, 0xa1, 0xa3, 0xa4, 0xa6, 0xa7,
0xa9, 0xaa, 0xac, 0xad, 0xaf, 0xb0, 0xb2, 0xb4, 0xb5, 0xb7, 0xb8, 0xba, 0xbb, 0xbd, 0xbf, 0xc0, 0xc2,
0xc4, 0xc5, 0xc7, 0xc8, 0xca, 0xcc, 0xcd, 0xcf, 0xd1, 0xd3, 0xd4, 0xd6, 0xd8, 0xd9, 0xdb, 0xdd, 0xdf,
0xe0, 0xe2, 0xe4, 0xe6, 0xe7, 0xe9, 0xeb, 0xed, 0xee, 0xf0, 0xf2, 0xf4, 0xf6, 0xf8, 0xf9, 0xfb, 0xfd,
0xff};

// White bias (based on 3000K warm white LED)
const float white_r = 1.0;
const float white_g = 0.796408;
const float white_b = 0.546234;

// Crossfade setup
const unsigned long int crossfade_duration_default = 180;
unsigned long int crossfade_duration = crossfade_duration_default;
unsigned long int t_last_light_update = 0;

// Timer initialisation
unsigned long int t_initialised = 0;
unsigned long int t_last_wifi_reconnect_attempt = 0;
unsigned long int t_last_mqtt_reconnect_attempt = 0;
unsigned long int t_last_logged = 0;

WiFiManager wifiManager;
WiFiManagerParameter custom_mqtt_server("server", "MQTT server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user, 20);
WiFiManagerParameter custom_mqtt_pass("password", "MQTT password", mqtt_password, 20);

// WiFi client setup
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long int t_last_sent = 0;
const unsigned int state_period = 5000;

// Setup for PWM strip
// Pins as per ElectroDragon LED board docs
const int red_pin = 15;
const int green_pin = 13;
const int blue_pin = 12;
const int white_pin = 14;

// Setup for signal-channels strip (WS2812B, WS2813, SK6812 etc.) over io2
// Change to suit your LED strip
// See NeoPixelBus docs for more information
const int io2_pin = 2;
const uint16_t PixelCount = 200;
NeoPixelBus<NeoGrbwFeature, NeoEsp8266Uart800KbpsMethod> strip_bus(PixelCount);

/* STATE */

// LED state, as sent and received over MQTT
bool switch_state = 0;
bool white_mode = 0;
bool stream_mode = 0;
uint8_t red_val = 255;
uint8_t green_val = 255;
uint8_t blue_val = 255;
uint8_t white_val = 255;
uint16_t coltemp_val = 333;
uint8_t brightness = 255;

// What are we transitioning to?
uint8_t red_val_target = 0;
uint8_t green_val_target = 0;
uint8_t blue_val_target = 0;
uint8_t white_val_target = 0;

// Where are we right now?
uint8_t red_val_now = 0;
uint8_t green_val_now = 0;
uint8_t blue_val_now = 0;
uint8_t white_val_now = 0;

// What are we transitioning from?
uint8_t red_val_from = 0;
uint8_t green_val_from = 0;
uint8_t blue_val_from = 0;
uint8_t white_val_from = 0;

// What is being sent to the LEDs?
uint8_t red_val_out = 0;
uint8_t green_val_out = 0;
uint8_t blue_val_out = 0;
uint8_t white_val_out = 0;

// Compensate for pre-dimmed RGB values
// which are sometimes received from Home Assistant
void clean_cols() {
  uint8_t R = red_val;
  uint8_t G = green_val;
  uint8_t B = blue_val;
  uint8_t max_col = (R>G?R:G)>B?(R>G?R:G):B;

  if (max_col < 255) {
    red_val = ((float)red_val / (float)max_col) * 255;
    green_val = ((float)green_val / (float)max_col) * 255;
    blue_val = ((float)blue_val / (float)max_col) * 255;
  }
}

void set_coltemp_val() {
  // TODO: make graph and get discontinuity point
  if (coltemp_val <= 200) {
    red_val = 255 - 1.148 * (200 - coltemp_val);
    green_val = 255 - 0.628 * (200 - coltemp_val);
    blue_val = 255;
  } else if (coltemp_val > 200 && coltemp_val <= 333) {
    red_val = 255;
    green_val = 255 - 0.39 * (coltemp_val - 200);
    blue_val = 255 - 0.87 * (coltemp_val - 200);
  } else {
    red_val = 255;
    green_val = 203 - 0.3 * (coltemp_val - 333);
    blue_val = 139 - 0.54 * (coltemp_val - 333);
  }
}

// Start transition
void update_target() {
  red_val_from = red_val_now;
  green_val_from = green_val_now;
  blue_val_from = blue_val_now;
  white_val_from = white_val_now;

  Serial.print("red_val_from to ");
  Serial.println(red_val_from);

  if (!stream_mode) {
    clean_cols();
  }
  if (white_mode) {
    set_coltemp_val();
  }

  float brightness_f = (float)switch_state * (float)brightness/255.0;
  Serial.print("brightness_f is ");
  Serial.println(brightness_f);
  
  red_val_target = brightness_f * red_val;
  green_val_target = brightness_f * green_val;
  blue_val_target = brightness_f * blue_val;
  
  white_val_target = (float)switch_state * white_val;

  Serial.print("red_val_target to ");
  Serial.println(red_val_target);

  t_last_light_update = millis();
}

// Handle MQTT packet
// If the server is setup correctly this will look
// something like s001l255r240g005b140k380t0001
//             or    vr240g005b140t0040
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  bool crossfade_duration_updated = false;
  bool temp_updated = false;
  bool stream_set = false;

  // Interpret the contents by checking each flag
  for (int i=0;i < length;i++) {
    char code_letter = payload[i];
    if (code_letter == 's') {
      // State command
      switch_state = rip_int(payload, i);
      Serial.print("| State to ");
      Serial.println(switch_state);
    } else if (code_letter == 'v') {
      // Stream mode
      stream_set = true;
    } else if (code_letter == 'l') {
      // Brightness (luminosity) command
      brightness = rip_int(payload, i);
      Serial.print("| Brightness to ");
      Serial.println(brightness);
    } else if (code_letter == 'r') {
      // Red command
      red_val = rip_int(payload, i);
      Serial.print("| Red to ");
      Serial.println(red_val);
    } else if (code_letter == 'g') {
      // Green command
      green_val = rip_int(payload, i);
      Serial.print("| Green to ");
      Serial.println(green_val);
    } else if (code_letter == 'b') {
      // Blue command
      blue_val = rip_int(payload, i);
      Serial.print("| Blue to ");
      Serial.println(blue_val);
    } else if (feature_white && (code_letter == 'k')) {
      // White command
      temp_updated = true;
      white_val = 255;
      coltemp_val = rip_int(payload, i);
      Serial.print("| Temp to ");
      Serial.println(coltemp_val);
    } else if (code_letter == 't') {
      // Transition command (seconds)
      crossfade_duration_updated = true;
      crossfade_duration = rip_int(payload, i, 4) * 1000;
      crossfade_duration = (crossfade_duration == 0) ? 50 : crossfade_duration;
      Serial.print("| Transition to ");
      Serial.println(crossfade_duration);
    } else if (code_letter == 'v') {
      // Transition command (milliseconds)
      crossfade_duration_updated = true;
      crossfade_duration = rip_int(payload, i, 4);
      crossfade_duration = (crossfade_duration == 0) ? 50 : crossfade_duration;
      Serial.print("| Transition to ");
      Serial.println(crossfade_duration);
    }
  }

  if (!crossfade_duration_updated) {
    // Reset crossfade duration if it hasn't been overridden
    // In other words, only use a special transition if
    // it was explicitly requested
    crossfade_duration = crossfade_duration_default;
  }

  white_mode = temp_updated;
  stream_mode = stream_set;

  update_target();

  if (!stream_mode) {
    send_state();
  }
}

// Pull out int from byte array given
// flag char position and number length
int rip_int(byte* payload, int pos, unsigned int num_len) {
  Serial.print("| Ripping chars ");
  int cumsum = 0;
  int multiplier = 1;
  for (int i=pos+num_len; i>pos; i--) {
    Serial.print(i);
    cumsum = cumsum + multiplier * ((char)payload[i] - '0');

    Serial.print("(");
    Serial.print(cumsum);
    Serial.print(")");

    multiplier = multiplier * 10;
    Serial.print(", ");
  }
  Serial.print("about to return ");
  Serial.print(cumsum);
  return cumsum;
}

// Default-length (3) overload of the above
int rip_int(byte* payload, int pos) {
  return rip_int(payload, pos, 3);
}

// Send a message to the MQTT broker containing the
// current state
void send_state() {
  Serial.print("Sending message to [");
  Serial.print(state_topic);
  Serial.print("] ");

  int vals[6] = {switch_state, brightness, red_val, green_val, blue_val, coltemp_val};
  char output[] = "000,000,000,000,000,000";

  int valindex = 0;
  int multiplier = 100;
  for (int i=0;i<sizeof(output);i++) {
    int digit_diff = vals[valindex] / multiplier;
    output[i] = digit_diff + '0';
    if (multiplier == 1) {
      valindex++;
      i++;
      multiplier = 100;
    } else {
      vals[valindex] = vals[valindex] - digit_diff * multiplier;
      multiplier = multiplier / 10;
    }
  }

  Serial.print(output);
  
  if (client.publish(state_topic, output, true)) {
    Serial.println(" Success");
  } else {
    Serial.println(" Failure");
  }

  t_last_sent = millis();
}

// Subscribe to MQTT commands for this device
void mqttSetup() {
  client.subscribe(command_topic, 1);

  if (client.publish(availability_topic, "online", true)) {
    Serial.println("Registered availability");
  } else {
    Serial.println("Failure");
  }
}

// Attempt once to connect to the MQTT broker
void mqttConnect() {
  Serial.print("Attempting MQTT connection to  ");
  Serial.print(mqtt_server);
  Serial.print(":");
  Serial.print(1883);
  Serial.print("... ");
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // Check the PubSubClient.connect docs to see
  // how to use username/password auth for connection
  if (client.connect(client_name, NULL, NULL, availability_topic, 0, 1, "offline")) {
    mqttSetup();
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

// An intermediate function to alter our colour destination
void rgb_preprocess() {

}

// Crossfade magic
// Get the instantaneous colour state at the current time
void rgb_interpolate() {
  if (((millis() - t_last_light_update) <= crossfade_duration) && (crossfade_duration > 5)) {
    // Not at target yet -> We're transitioning so do math
    Serial.print(crossfade_duration);
    red_val_now = map(millis() - t_last_light_update, 0, crossfade_duration, red_val_from, red_val_target);
    green_val_now = map(millis() - t_last_light_update, 0, crossfade_duration, green_val_from, green_val_target);
    blue_val_now = map(millis() - t_last_light_update, 0, crossfade_duration, blue_val_from, blue_val_target);
    white_val_now = map(millis() - t_last_light_update, 0, crossfade_duration, white_val_from, white_val_target);
  } else {
    // Reached target -> We've finished transitioning so don't do math
    red_val_now = red_val_target;
    green_val_now = green_val_target;
    blue_val_now = blue_val_target;
    white_val_now = white_val_target;

    // Reset crossfade duration now we're done
    // crossfade_duration = crossfade_duration_default;
  }
}

// An intermediate function to map our colour to actual output variables
// This is where non-colour things like dimming curves or strobing should happen
void rgb_postprocess() {
  red_val_out = red_val_now;
  green_val_out = green_val_now;
  blue_val_out = blue_val_now;
  white_val_out = white_val_now;

  apply_white_bias();
  apply_dimming_curve();
}

void apply_dimming_curve() {
  red_val_out = dimming_curve[(uint8_t)(red_val_out + white_r * white_val_out)] - dimming_curve[(uint8_t)(white_r * white_val_out)];
  green_val_out =dimming_curve[(uint8_t)(green_val_out + white_g * white_val_out)] - dimming_curve[(uint8_t)(white_g * white_val_out)];
  blue_val_out = dimming_curve[(uint8_t)(blue_val_out + white_b * white_val_out)] - dimming_curve[(uint8_t)(white_b * white_val_out)];
  white_val_out = dimming_curve[white_val_out];
}

void apply_white_bias() {
  uint16_t R = (float)red_val_out / white_r;
  uint16_t G = (float)green_val_out / white_g;
  uint16_t B = (float)blue_val_out / white_b;
  uint8_t min_col = (R<G?R:G)<B?(R<G?R:G):B;
  //Serial.print("mincol");
  //Serial.print(min_col);

  white_val_out = min_col;
  red_val_out = red_val_out - white_r * white_val_out;
  green_val_out = green_val_out - white_g * white_val_out;
  blue_val_out = blue_val_out - white_b * white_val_out;
}

// Send the output colour to hardware, for realsies
void rgb_display() {
  if (millis() - t_last_logged > 1000) {
    t_last_logged = millis();
    Serial.print("now(");
    Serial.print(red_val_now);
    Serial.print(",");
    Serial.print(green_val_now);
    Serial.print(",");
    Serial.print(blue_val_now);
    Serial.print(",");
    Serial.print(white_val_now);
    Serial.print(") -> ");
    Serial.print("out(");
    Serial.print(red_val_out);
    Serial.print(",");
    Serial.print(green_val_out);
    Serial.print(",");
    Serial.print(blue_val_out);
    Serial.print(",");
    Serial.print(white_val_out);
    Serial.println(")");
  }

  // PWM update
  analogWrite(red_pin, red_val_out);
  analogWrite(green_pin, green_val_out);
  analogWrite(blue_pin, blue_val_out);
  analogWrite(white_pin, white_val_out);

  // Bus update
  RgbwColor out_bus(red_val_out, green_val_out, blue_val_out, white_val_out);
  for (int i = 0;i < PixelCount;i++) {
    strip_bus.SetPixelColor(i, out_bus);
  }
}

void setup() {
  // PWM setup
  analogWriteFreq(pwm_freq);
  analogWriteRange(255);

  Serial.begin(115200);

  // Pin setup
  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);
  pinMode(white_pin, OUTPUT);

  // Initial output (a flash may still occur on powerup)
  analogWrite(red_pin, 0);
  analogWrite(green_pin, 0);
  analogWrite(blue_pin, 0);
  analogWrite(white_pin, 0);

  strip_bus.Begin();
  strip_bus.Show();

  t_initialised = millis();
  t_last_sent = t_initialised;

  wifiManager.autoConnect("Lightt");

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_pass.getValue());

  wifiManager.setConfigPortalTimeout(60);
  wifiManager.startConfigPortal();
}

void loop() {
  // Are we conencted to MQTT broker?
  if (!client.connected()) {
    unsigned long now = millis();

    // If not, we should try to connect at boot then every 5 seconds
    if (now - t_last_mqtt_reconnect_attempt > 5000UL || t_last_mqtt_reconnect_attempt == 0) {
      t_last_mqtt_reconnect_attempt = now;
      mqttConnect();
    }
    return;
  }

  // Send state to the broker regularly so it knows what we're up to
  if ((millis() - t_last_sent) > state_period) {
    send_state();
  }
  
  // Prepare colour
  rgb_interpolate();
  rgb_postprocess();
  
  // Output colour
  rgb_display();
  strip_bus.Show();

  // Do MQTT stuff
  client.loop();
}
