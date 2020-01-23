# SmartWhoopGate32

By [Marcel Verdult](https://github.com/marcelverdult)
and [Jan Ryklikas](https://github.com/soulofnoob)

## Summary (Work in Progress)

  In this project we created "Smart" TinyWhoop drone gates for indoor flying.

  They are connected to a 5,8GHz video receiver and can change their color based on frequencys in range.

  Currently we use one color for every frequency inside the Race-Band.

  The different Lighting modes can be controlled via [MQTT](#mqtt-reference)

 ![AnimationGIF](Doku/WingAnimation.gif)

## Parts List

- ESP32 (we used the "DOIT Esp32 DevKit v1")
- WS2812B LED strip (we used the 60LED/m variant and 90 LEDs per gate)
- RX5808 5,8 GHz receiver (potentionally needs to be modified for SPI)
- 5V 3A power supply
- DC barrel Jack
- Lasercutted / 3D printed housing
- ca 160cm of stiff tubing at least 9.8mm in diameter (we needed to cut off ca 2mm of the led strip to make it fit)

## To make the code work

1. Wait until i made a propper tutorial ^^
2. try to find the wifi and MQTT server settings and change to your network and server
3. find the firmware.h file and change the FIRMWARE_VERSION to 0.0 that way you force the controller to update its eeprom with the data you just set
4. the controller will restart, connect to wifi, and pull the newest binary.
5. if you want your own code to stay on it, incrase the version number to whatever is higher than the current one to keep it from updating
6. happy hacking :)

## Commands
  
| Command        | Parameters                                         |
| ----------     | -------------------------------------------------- |
| power          | `0` / `off` = turn OFF <br/> `1` / `on` = turn ON <br/> `2` / `toggle` = toggle ON/OFF |
| mode           | `11..16` = set LED mode                            |
| brightness     | `0..255` = set LED brightness                      |
| restart        | send empty message to trigger restart              |
| update         | send empty message to trigger update               |
| autoUpdate     | `0` / `off` = turn OFF <br/> `1` / `on` = turn ON  |
| resetRSSI      | send empty message to trigger rssi reset           |
| autoresetRSSI  | `0` / `off` = turn OFF <br/> `1` / `on` = turn ON  |
| maxRSSI&lt;x>  | `0..4000` <br/> x = `0..7`                         |
| logRSSI        | `0` / `off` = turn OFF <br/> `1` / `on` = turn ON  |
| name           | `<name>`                                           |
<!-- | network&lt;x>  | `<ssid>;<pass>;<mqtt>` <br/> x = `0..4`            | -->

## MQTT Reference

### Command Topics

`gates/gate<x>/cmnd/%command%`  
`gates/all/cmnd/%command%`

**Example:**  
To turn LEDs on Send:  
Topic: `gates/all/cmnd/power` Payload: `ON`

### Status Topics

`gates/gate<x>/stat/%command%`  
`gates/all/stat/%command%`

**Example:**  
To confirm that power is on the gate responds:  
Topic: `gates/all/stat/power` Payload: `1`

### Telemetry Topics

`gates/gate<x>/tele`

**Example:**  
The gate periodically sends messages to this topic, like:  
Topic: `gates/gate<x>/tele` Payload: `uptime: 4300ms, FW: 2.7. Checking for updates..`

### Fallback Topics

All gates subscribe to a Fallback topic, in case the name was accidentaly changed.  
If you want so send a message to the fallback topic, substitute `gate<x>` for the MAC adress without `:`.

**Example:**  
MAC: `80:C1:89:F5:AE:44`  
Topic: `gates/80C189F5AE44/cmnd/%command%`

## Backlog functionality

Via a Serial terminal or the [MQTT](#mqtt-reference) Topics: `gates/gate<x>/backlog` and `gates/all/backlog`,
you can set variables in batch.

All [Commands](#Commands) from above can be used and chained as follows: 

`<command>: <value>; <command2>: <value2>;`...

**Example:**  
Turning LEDs on, switching to mode 16 and setting brightness to 100:  
`power: ON; mode: 16; brightness: 100;`

## Changelog

see [Changelog](changelog.md)