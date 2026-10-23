# APC SurgeArrest for SmartThings and Alexa

Control an **APC PH6U4X32W** Wi-Fi surge protector from SmartThings using a
dedicated ESP32 bridge. Add its three controlled outlets, two USB charger
groups, and strip LED to SmartThings routines, and use Alexa through the
**SmartThings skill**.

**[Start with the step-by-step beginner guide](docs/BEGINNER_GUIDE.md).**

**[Install the Edge driver from TylerDurden94’s Edge Drivers](https://bestow-regional.api.smartthings.com/invite/oDM8Q4Bkgp2L).**
Enroll your SmartThings hub, open Available Drivers, and choose
**APC SurgeArrest ESP Bridge**. This is a shared channel; install only the
drivers you need. Source downloads alone do not install a driver on your hub.

## How it works

```text
SmartThings app, routines, or Alexa through the SmartThings skill
  -> SmartThings hub running APC SurgeArrest ESP Bridge
  -> dedicated ESP32 over your home network
  -> APC SurgeArrest using its authenticated Ayla LAN protocol
```

The ESP32-to-strip control path stays on your local network. The one-time
key-capture step needs your working APC Home account and the APC/Ayla cloud.
SmartThings remote access and Alexa still need internet access and their
respective services. This does not promise a fully offline SmartThings app
or a replacement for account setup when the APC cloud is unavailable.

This is a standalone SurgeArrest project. It contains only the SurgeArrest
bridge, driver, setup tools, and documentation. Use a dedicated board for it.

## What you get

| Feature | Behavior |
| --- | --- |
| Three AC outlet controls | Outlet 1, Outlet 2, and Outlet 3, each separately controlled |
| USB controls | Two controllable USB charger groups; this does not imply one switch per physical USB socket |
| Strip LED | On/off from the parent strip page and a separate child device |
| Individual devices | Six child devices for routines, rooms, and Alexa; rename them in SmartThings |
| Local state feedback | Authenticated strip readbacks, periodic reads every 30 seconds, and reads after commands |
| Bounded retries | Up to three strip delivery attempts; newer requests replace older requests for the same control |
| Open source | Firmware, Edge driver, setup tools, and automated tests |

SmartThings shows a parent strip device plus six children. The parent’s main
switch is **Outlet 1**, not a master power switch for the entire strip.
State is reported from actual strip observations, not from the requested target.
An unavailable reading becomes unknown on the ESP32 after 90 seconds; the
SmartThings app may retain its last known switch value. A displayed On/Off
value alone is not proof of a recent successful command.

## What you need

- An APC **PH6U4X32W** already set up on 2.4 GHz Wi-Fi in APC Home, with
  LAN control information available from its account. Other models are untested.
- A dedicated classic **ESP32-WROOM-32/32D** USB development board with at
  least 4 MB flash, USB data cable, and continuous USB power.
- A SmartThings hub that supports **LAN Edge drivers**, plus the SmartThings app.
- A computer for the one-time USB firmware/setup steps, and Python 3.12.
- Hub, strip, and ESP32 on a local network where they can communicate directly.

The release build targets `esp32dev` (classic ESP32). ESP32-C3, S2, S3, and
other families are not covered by this release. No soldering or electrical
modification to the surge protector is needed. Keep its normal load ratings.

## Downloads and installation

Get the **[v0.1.0 beta release](https://github.com/tylerdurden94/SurgeArrest/releases/tag/v0.1.0)**.
The beginner guide covers source download, the prebuilt firmware, USB
configuration, driver installation, discovery, and Alexa linking. Advanced
users can build the firmware and package the driver themselves.

This is a **beta**. The APC protocol and driver behavior were exercised on the
reference strip in the original integration, including a WAN-blocked control
test. The new standalone firmware wrapper and USB provisioning workflow have
build and automated checks, but have not yet had a complete installation on
a fresh physical board and another user's hub. A previous strip session stall
required power-cycling the strip; its root cause remains unresolved.
See [validation and limits](docs/VALIDATION.md).

## Documentation

- [Beginner installation guide](docs/BEGINNER_GUIDE.md)
- [SmartThings controls, settings, and Alexa](docs/SMARTTHINGS_DRIVER_GUIDE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Local API](docs/LOCAL_API.md)
- [Build and publishing instructions](docs/PUBLISHING.md)
- [Security and private configuration](SECURITY.md)
- [Release history](CHANGELOG.md) and [third-party notices](THIRD_PARTY_NOTICES.md)

| Folder | Contents |
| --- | --- |
| `esp_bridge` | Standalone ESP32 firmware and delivery state-machine tests |
| `edge_driver` | SmartThings Edge driver, standard-capability profiles, and simulated hub tests |
| `tools` | Private LAN-key capture and USB provisioning helpers |
| `tests` | Simulated setup tests; no account or hardware access |
| `docs` | Installation, feature, maintenance, and troubleshooting guides |

Independent community software. Not an official APC, Schneider Electric,
Ayla, Samsung, SmartThings, or Amazon product. [MIT license](LICENSE).
