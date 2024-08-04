# Lightt

> [!IMPORTANT]
> In the years since this repository was updated, I have moved to [WLED](https://kno.wled.ge/) and [ESPHome](https://esphome.io/) for my LED strip needs. This repo remains as an archive.

*Control RGBW LEDs over MQTT using ESP8266.*

## Features

- Support for basic PWM output as well as NeoPixel strips
- Full white-channel support
- Silky-smooth crossfading animations
- Transition command support
- Post-process stage
- Dimming curve
- Robust and fail-safe WiFi/MQTT connectivity

## Getting started

1. In `lightt.ino`, set `feature_white` to your desired value depending on whether you want white channel handling.
1. Flash the board and connect a device to the resulting AP for configuring WiFi and MQTT settings [as below](#configuring).
1. For further configuration options, check out WS2812B setup variables and MQTT connection function. By default, the strip is set to GRBW (try GRB for strips with no white channel) and no MQTT credentials are used.

## Configuring

Each time the board is powered up, one of two things will happen:

- **If no known WiFi network is found:** An access point will be created. Connect a device to this AP and you'll be redirect via captive portal to configure WiFi and MQTT settings. The device will reboot once a new WiFi network has been configured.
- **If a known WiFi network is found:** The device will connect to WiFi. For the following minute, you'll be able to access the configuration options by going to the ESP's IP address in a web browser. One minute after power-on (or when you click the "save" button in the web interface), the board will leave configuration mode and try to connect to MQTT. You may need to power cycle the board if this happens before you were able to save settings.

## ESP settings

I'm using the [ESP LED Strip Board][edragon] from Electrodragon. The following IDE settings work well for flashing:

```json
{
    "board": "esp8266:esp8266:generic",
    "configuration": "xtal=80,vt=flash,exception=enabled,ResetMethod=ck,CrystalFreq=26,FlashFreq=40,FlashMode=qio,eesz=4M1M,led=2,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=115200",
    "port": "COM7",
    "sketch": "lightt.ino",
    "output": "../build"
}
```

The AVR ISP programmer works well. Ensure you select a board with SPIFFS storage enabled (the `eesz=4M1M` in the json above should sort that out for you).

## Configuring the broker

I run Eclipse Mosquitto in a Docker container based on the [official Eclipse Mosquitto Docker image][mosquittodocker]. This is running on an [Odroid HC2][hc2]. In my case I've docker-composed the image with Home Assistant to make a tidy pairing.

## MQTT message format

**Command**: Lightt expects a string consisting of several groups, where each group is a character followed by three numbers. `hello`

## Configuring Home Assistant

Home Assistant is my Home Automation software of choice. These lights are `mqtt_template` type lights because this enables transition support.

### MQTT

Put the following in your `configuration.yaml`:

```yaml
mqtt:
  broker: 127.0.0.1
```

### RGB light
```yaml
light deskback:
  - platform: mqtt_template
    name: "Desk surround light"
    retain: true
    qos: 1
    state_topic: "tenbar/abedroom/deskback/state"
    command_topic: "tenbar/abedroom/deskback/command"
    availability_topic: "tenbar/abedroom/deskback/availability"
    command_on_template: "s001
      {%- if brightness is defined -%}
      {{ 'l%03d' | format(brightness) }}
      {%- endif -%}
      {%- if red is defined and green is defined and blue is defined -%}
      {{ 'r%03dg%03db%03d' | format(red, green, blue) }}
      {%- endif -%}
      {%- if transition is defined -%}
      {{ 't%04d' | format(transition) }}
      {%- endif -%}"
    command_off_template: "s000
      {%- if transition is defined -%}
      {{ 't%04d' | format(transition) }}
      {%- endif -%}"
    state_template: "{% if '1' in value.split(',')[0] %}on{% else %}off{% endif %}"
    brightness_template: "{{ value.split(',')[1] }}"
    red_template: "{{ value.split(',')[2] }}"
    green_template: "{{ value.split(',')[3] }}"
    blue_template: "{{ value.split(',')[4] }}"
```

### RGBW light
```yaml
light desktop:
  - platform: mqtt_template
    name: "Desktop light"
    retain: true
    qos: 1
    state_topic: "tenbar/abedroom/desktop/state"
    command_topic: "tenbar/abedroom/desktop/command"
    availability_topic: "tenbar/abedroom/desktop/availability"
    command_on_template: "s001
      {%- if brightness is defined -%}
      {{ 'l%03d' | format(brightness) }}
      {%- endif -%}
      {%- if red is defined and green is defined and blue is defined -%}
      {{ 'r%03dg%03db%03d' | format(red, green, blue) }}
      {%- endif -%}
      {%- if white_value is defined -%}
      {{ 'w%03d' | format(white_value) }}
      {%- endif -%}
      {%- if transition is defined -%}
      {{ 't%04d' | format(transition) }}
      {%- endif -%}"
    command_off_template: "s000
      {%- if transition is defined -%}
      {{ 't%04d' | format(transition) }}
      {%- endif -%}"
    state_template: "{% if '1' in value.split(',')[0] %}on{% else %}off{% endif %}"
    brightness_template: "{{ value.split(',')[1] }}"
    red_template: "{{ value.split(',')[2] }}"
    green_template: "{{ value.split(',')[3] }}"
    blue_template: "{{ value.split(',')[4] }}"
    white_value_template: "{{ value.split(',')[5] }}"
```

### Automation ideas

I setup a wake-up light. It's pretty cool.

```yaml
script:
  wake_up:
    alias: Wake up
    sequence:
      - alias: Surround to red
        service: light.turn_on
        data:
          entity_id: light.desk_surround_light
          brightness: 255
          rgb_color: [255,63,0]
          transition: 800
      - alias: Top to orange
        service: light.turn_on
        data:
          entity_id: light.desktop_light
          brightness: 195
          white_value: 72
          rgb_color: [255,125,0]
          transition: 800
      - delay:
          seconds: 901
      - alias: Surround to blue
        service: light.turn_on
        data:
          entity_id: light.desk_surround_light
          brightness: 255
          rgb_color: [0,255,255]
          transition: 900
      - alias: Top to light green
        service: light.turn_on
        data:
          entity_id: light.desktop_light
          brightness: 255
          white_value: 231
          rgb_color: [0,255,63]
          transition: 900
```

This gets called every morning from an automation:

```yaml

automation: 
  - alias: 'Wake-up light (weekday)'
    trigger:
      platform: time
      at: '07:30:00'
    condition:
      - condition: time
        weekday:
        - mon
        - tue
        - wed
        - thu
        - fri
      - condition: state
        entity_id: group.albert_bedroom
        state: 'off'
    action:
      service: script.turn_on
      entity_id: script.wake_up
  - alias: 'Wake-up light (weekend)'
    trigger:
      platform: time
      at: '08:45:00'
    condition:
      - condition: time
        weekday:
        - sat
        - sun
      - condition: state
        entity_id: group.albert_bedroom
        state: 'off'
    action:
      service: script.turn_on
      entity_id: script.wake_up
```

[edragon]: http://www.electrodragon.com/product/esp-led-strip-board/
[mosquittodocker]: https://hub.docker.com/_/eclipse-mosquitto
[hc2]: https://www.hardkernel.com/shop/odroid-hc2-home-cloud-two/
