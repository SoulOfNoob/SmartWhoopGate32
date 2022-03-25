# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.10] - 2022-03-25

### Added

- WiFi reconnect if lost during operation

## [2.9] - 2020-06-03

### Added

- rssi logging in JSON

### Changed

- MIN_TUNE_TIME from 50 to 40

### Removed

- serial logging in rx lib

## [2.8] - 2020-02-03

### Added

- demo mode for display

### Changed

- fixed flickering (again)
- mqtt sending only when connected
- mqtt reconnect maximal every 5 min

## [2.7] - 2020-01-22

### Added

- Changelog
- Backlog functionality (MQTT and Serial)
- New Command interpretation
- Conditional debug function
- Debug Level

### Changed

- Massive refactor
- moved fastled initialization to animations lib
- moved eeprom operations to system lib
