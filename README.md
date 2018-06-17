# Lightt

*Control RGBW LEDs over MQTT using ESP8266.*

## Features

- Support for basic PWM output as well as NeoPixel strips
- Full white-channel support
- Silky-smooth crossfading animation
- Transition command support
- Post-process stage
- Dimming curve
- Robust and fail-safe WiFi/MQTT connectivity

## Getting started

1. Copy `credentials_template.h` to `credentials.h` and fill in your WiFi credentials. If you're using MQTT credentials, fill those in also.
1. In `lightt.ino`, set `mqtt_server`, `client_name`, `command_topic`, `state_topic`, and `feature_white` to your desired values.
1. For further configuration options, check out WS2812B setup variables and MQTT connection function. By default, the strip is set to GRBW (try GRB for strips with no white channel) and no MQTT credentials are used.

## ESP settings

I'm using the [ESP LED Strip Board][edragon] from Electrodragon. The following IDE settings work well for flashing:

```json
{
    "board": "esp8266:esp8266:generic",
    "configuration": "CpuFrequency=80,ResetMethod=ck,CrystalFreq=26,FlashFreq=40,FlashMode=qio,FlashSize=512K0,led=2,LwIPVariant=v2mss536,Debug=Disabled,DebugLevel=None____,FlashErase=none,UploadSpeed=115200",
    "port": "COM7",
    "sketch": "lightt.ino",
    "output": "../build"
}
```

The AVR ISP programmer works well.

## Configuring the broker

I use [Mosquitto][mosquitto] on a Raspberry Pi 2B. 

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
    state_topic: "tenbar/abedroom/deskback/state"
    command_topic: "tenbar/abedroom/deskback/command"
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
    state_topic: "tenbar/abedroom/desktop/state"
    command_topic: "tenbar/abedroom/desktop/command"
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
[mosquitto]: https://mosquitto.org/
