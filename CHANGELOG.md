# Release history

## v0.1.0 beta — 2026-10-23

- Standalone SurgeArrest ESP32 firmware for APC PH6U4X32W.
- Three outlet controls, two USB charger groups, and strip LED control.
- Parent strip plus six child devices in SmartThings using standard capabilities.
- Separate SurgeArrest mDNS discovery and public driver package identity.
- Private one-time LAN-key capture, USB Wi-Fi/key provisioning, and random bridge token.
- Authenticated observed state, bounded delivery retries, request replacement,
  and unknown-state handling retained from the tested APC integration.
- Beginner, driver, API, troubleshooting, validation, and publishing guides.
- Shared community channel and source/build/test workflow.

The standalone wrapper/setup path has not been validated end to end on a new
physical board and another hub. Earlier hardware tests cover the APC core and
driver in the original integration. A prior strip session stall required a
strip power cycle; its cause remains unresolved. This release remains beta.
