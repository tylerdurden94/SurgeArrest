# Third-party notices

Original project code and documentation are distributed under the MIT license
in [LICENSE](LICENSE). Third-party dependencies retain their own licenses.

- [Arduino core for ESP32](https://github.com/espressif/arduino-esp32), provided
  by PlatformIO Espressif32 7.1.3: LGPL-2.1; bundled components have their own notices.
- [ESP-IDF](https://github.com/espressif/esp-idf), including cJSON and mbedTLS:
  see the respective upstream component licenses (including Apache-2.0 and MIT).
- [SmartThings Edge libraries](https://github.com/SmartThingsCommunity/SmartThingsEdgeDrivers):
  platform-provided runtime libraries, subject to their upstream licenses.
- [pyserial 3.5](https://github.com/pyserial/pyserial): BSD-3-Clause.
- [Requests 2.32.5](https://github.com/psf/requests): Apache-2.0.
- [esptool 4.11.0](https://github.com/espressif/esptool/tree/v4.11.0): GPL-2.0-or-later;
  installed separately for flashing, not embedded in the supplied firmware.
- Lupa (MIT), Lua (MIT), PyYAML (MIT), and PlatformIO Core (Apache-2.0)
  are development/test dependencies, installed separately.

The community [APCBridge](https://github.com/jogeraca/APCBridge) and its
[AylaLocalAPI ancestry](https://github.com/jakecrowley/AylaLocalAPI) were useful
protocol references. No APCBridge/AylaLocalAPI source or binaries are bundled
here. The capture helper is an independent implementation of the account API
requests needed to obtain the owner’s LAN configuration. No manufacturer app,
account backup, proprietary firmware, or personal device data is distributed.

For the prebuilt ESP32 image, corresponding bridge source is the `v0.1.0` tag.
The pinned PlatformIO platform resolves Arduino core 2.0.17 and its supporting
SDK/toolchain packages. These upstream sources remain available from the
linked projects and the PlatformIO package manifest. The application uses
mbedTLS AES/HMAC routines and the core's HTTP/Wi-Fi/mDNS services.
