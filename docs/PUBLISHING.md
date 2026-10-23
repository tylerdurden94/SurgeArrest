# Build and publish SurgeArrest

The public repository is [tylerdurden94/SurgeArrest](https://github.com/tylerdurden94/SurgeArrest).
The normal user installation route is the existing shared
**[TylerDurden94’s Edge Drivers](https://bestow-regional.api.smartthings.com/invite/oDM8Q4Bkgp2L)** channel.
Do not create another channel for routine releases of this driver.

## Verify source and build firmware

From the repository root, with Python 3.12 and a C++17 compiler installed:

```text
python -m pip install -r requirements-dev.txt platformio==6.2.0
python edge_driver/test_driver.py
python -m unittest discover -s tests -v
python esp_bridge/test_firmware.py
pio run -d esp_bridge
```

The firmware test accepts `--cxx PATH` or `--zig PATH` if `g++` is not available.
It tests the production delivery state machine, without contacting a strip.
The Edge tests simulate a hub and LAN transport. Setup tests mock USB/account
responses. These do not replace a complete hardware installation.

Build target: classic ESP32 `esp32dev`, 4 MB flash, PlatformIO Espressif32
**7.1.3**, Arduino core **2.0.17**. This project does not use Bluetooth libraries.
PlatformIO produces `esp_bridge/.pio/build/esp32-wroom-32d/firmware.bin` and its
matching bootloader/partition images. `pio run -d esp_bridge -t upload
--upload-port YOUR_PORT` uploads to a dedicated board. USB configuration is a
separate step. Never publish a readback of a configured board’s flash.

For a merged new-board download, use esptool 4.11.0 with the matching build files:

```text
python -m esptool --chip esp32 merge_bin -o SurgeArrest-esp32-v0.1.0.bin --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 BOOTLOADER_BIN 0x8000 PARTITIONS_BIN 0xe000 BOOT_APP0_BIN 0x10000 FIRMWARE_BIN
```

Replace the uppercase placeholders with paths to the build’s files.
`boot_app0.bin` comes from the matching Arduino core’s `tools/partitions`
directory. The result is flashed at **0x0** after erasing a dedicated board.
Provide checksums and exact source tag with every binary release.

## Publish the Edge driver

Install the official [SmartThings CLI](https://github.com/SmartThingsCommunity/smartthings-cli)
and sign in to the maintainer’s SmartThings account. Package from `edge_driver`:

```text
smartthings edge:drivers:package edge_driver
```

Record the returned driver ID and exact timestamp version. The public package
key is `tylerdurden94.apc.surgearrest`; retain it for future updates. This is
separate from the original private deployment package. The public driver
discovers the standalone `_surgearrest._tcp` service.

Assign the intended version to the existing shared channel:

```text
smartthings edge:channels:assign DRIVER_ID DRIVER_VERSION --channel 36c02108-d699-4e4b-bed9-d019f5d3af9a
smartthings edge:channels:drivers 36c02108-d699-4e4b-bed9-d019f5d3af9a --json
```

Check that **APC SurgeArrest ESP Bridge** appears with the intended ID/version
alongside the other published drivers. Do not remove or replace those drivers.
The channel’s existing invitation applies to this driver; no new invitation is
needed. This follows the official [Driver Channels guide](https://developer.smartthings.com/docs/devices/hub-connected/driver-channels).

Users enroll their hub and select this specific driver under Available Drivers.
Community publishing is distinct from Works with SmartThings certification.
This project makes no certification claim.

## GitHub release

Publish a source tag and a beta GitHub Release with:

- A clean source tree containing only the standalone project.
- The build-generated merged ESP32 image, without configured NVS data.
- A driver package ZIP for advanced users (the channel remains recommended).
- SHA-256 checksums, setup links, and explicit validation limits.

Update `smartthings-release.json` with the driver version, firmware version,
source tag, and shared invitation. Wait for the GitHub source and firmware
checks to pass. Inspect source, history, and downloads for credentials,
personal identifiers, diagnostics, and mixed-project code before publication.

For updates, test on a dedicated board/test hub before promoting an existing
public driver version: users enrolled in its channel may receive updates.
Keep its identity, child keys, and profiles compatible unless a migration is
deliberately implemented and tested.
