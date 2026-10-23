# Install SurgeArrest in SmartThings

This guide uses a dedicated classic ESP32 USB development board. You do not
need to open the surge protector, solder, or leave a computer running.
The first release is a beta: allow time for setup and read the
[validation limits](VALIDATION.md) before starting.

## 1. Check the strip and network

Confirm the label says **PH6U4X32W**, that the strip works in APC Home, and
that you know the correct APC Home account password. Key capture needs that
service to be reachable; repeated wrong passwords can lock the account.
Other SurgeArrest models have not been validated.

The strip, ESP32, and SmartThings hub must share a reachable local network.
Guest Wi-Fi isolation or separate VLANs can block this. Use 2.4 GHz Wi-Fi
for the ESP32. Reserve the strip’s address in your router so it does not
change after setup. The ESP32 address may change; the driver rediscovers it.
Do not set up internet port forwarding.

## 2. Download and extract the project

1. Open the [beta release](https://github.com/tylerdurden94/SurgeArrest/releases/tag/v0.1.0).
2. Download **Source code (zip)** and **SurgeArrest-esp32-v0.1.0.bin**.
3. Extract the ZIP into a folder you can find, such as `Downloads/SurgeArrest`.
4. Open that extracted folder. You should see `README.md`, `requirements.txt`,
   `tools`, `esp_bridge`, and `edge_driver`.

The Edge ZIP is for advanced users. The shared channel installs the driver
without developer tools.

## 3. Install Python and the setup tools

Install [Python 3.12](https://www.python.org/downloads/) from python.org.
On Windows, select **Add Python to PATH** during installation.
Open a terminal **in the extracted project folder** (on Windows, right-click
the folder background and choose Open in Terminal), then run:

```powershell
python -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt esptool==4.11.0
```

For the remaining Windows examples, `.venv\Scripts\python` means the Python
installed in that project’s private environment. On macOS/Linux use
`.venv/bin/python` instead, including in the commands below.
If Windows recognizes `py` instead of `python`, use `py -3.12` for the first
command. Close other serial monitors before flashing or configuration.

## 4. Find the ESP32 USB port

Connect the dedicated ESP32 with a **data-capable** USB cable. Run:

```powershell
.venv\Scripts\python tools/configure.py --list
```

Record the port that appears when you plug in the ESP32, such as `COM6` on
Windows, `/dev/ttyUSB0` on Linux, or `/dev/cu.usbserial-...` on macOS. These
are examples; use the actual port shown for your board.
If nothing appears, try another cable and check the board maker’s USB driver.

## 5. Install the firmware on the dedicated board

**This step erases the selected board.** Use a dedicated/new board with no
configuration you need to keep. Do not select a board serving another project.
The release image contains no Wi-Fi password, strip key, or bridge token.

Place the downloaded `.bin` in the project folder. Replace `COM6` with your
port and run these two commands:

```powershell
.venv\Scripts\python -m esptool --chip esp32 --port COM6 erase_flash
.venv\Scripts\python -m esptool --chip esp32 --port COM6 --baud 460800 write_flash 0x0 SurgeArrest-esp32-v0.1.0.bin
```

The download is a **merged full image** and goes at **0x0**. A separately built
`firmware.bin` is an application image and must not be substituted here.
If connecting stalls, hold the board’s BOOT button while the tool starts
connecting, then release it. Try baud 115200 if writing fails. Wait for a
successful verification before continuing.

Developers can instead follow [build instructions](PUBLISHING.md) and upload
with PlatformIO.

## 6. Capture your strip’s LAN key once

Run this locally; never paste your APC password or LAN key into GitHub issues:

```powershell
.venv\Scripts\python tools/capture_key.py
```

Enter the APC Home account email and correct password when prompted. The
password is hidden. If the account has multiple LAN-enabled devices, select
your strip from the list. A failed sign-in is not automatically retried.

The helper saves only the selected strip’s LAN IP and LAN-key information:

| System | Default private file |
| --- | --- |
| Windows | `%LOCALAPPDATA%\SurgeArrest\config.json` |
| macOS | `~/Library/Application Support/SurgeArrest/config.json` |
| Linux | `~/.config/SurgeArrest/config.json` (or `$XDG_CONFIG_HOME`) |

It does not save your account password or account access token. The LAN key
can control the strip, so keep this file private. Existing compatible capture
files can be reused with `configure.py --config PATH`; do not upload them.
The capture helper refuses to overwrite an existing file.

If the APC/Ayla account API no longer supplies a LAN key, stop here: this
release has no alternative key-recovery method. See [troubleshooting](TROUBLESHOOTING.md).

## 7. Configure Wi-Fi and the strip over USB

```powershell
.venv\Scripts\python tools/configure.py --port COM6
```

Enter your 2.4 GHz Wi-Fi network name and password. The helper generates a
separate random **bridge token**, sends the Wi-Fi and strip configuration
over USB, and restarts the ESP32. It prints the location of your private
`bridge_token.txt` file, without displaying its contents.

If you reserved a new strip IP after capture, add `--device-ip YOUR_STRIP_IP`.
If your private config is stored elsewhere, add `--config PATH`.
Re-running setup reuses the existing token file. Retain that file so you can
copy its value into SmartThings. Do not send your LAN key to SmartThings;
the driver only needs the separate bridge token.

Leave the ESP32 powered. Give it about a minute to join Wi-Fi and establish a
strip session. Configuration does not issue outlet on/off commands.

## 8. Install the SmartThings driver

1. Open **[TylerDurden94’s Edge Drivers](https://bestow-regional.api.smartthings.com/invite/oDM8Q4Bkgp2L)**.
2. Sign in with the Samsung account that owns your SmartThings hub.
3. Accept the channel invitation and **Enroll** your hub.
4. Open **Available Drivers** for that hub.
5. Select **APC SurgeArrest ESP Bridge** and click **Install**.

If already enrolled for another driver, use the same channel and install this
driver from its Available Drivers list. Enrollment alone does not install all
drivers. See the [official shared-channel guide](https://developer.smartthings.com/docs/devices/hub-connected/enroll-in-a-shared-channel).

## 9. Discover and authorize your strip

1. Open SmartThings, choose **+ / Add device**, and **Scan nearby**. The exact
   menu wording varies by app version.
2. Look for **APC SurgeArrest** and the six child devices. They may appear in
   the room used for newly discovered devices.
3. Open the parent **APC SurgeArrest** device, then its menu and **Settings**.
4. Paste the value from your private `bridge_token.txt` into **APC bridge token**.
5. Save, return to the device, and refresh. Wait about a minute for readings.

The parent has controls for Outlet 1–3, USB charger groups 1–2, and the LED.
There are also separate children named APC Outlet 1–3, APC USB Charger 1–2,
and APC Strip LED. Rename and assign them to rooms as you prefer. **The
parent’s main switch controls only Outlet 1.**

For the first physical test, choose an outlet with a harmless test lamp and
confirm its on/off behavior. Do not use a socket powering equipment that must
stay on for this test. Confirm the physical result; a retained SmartThings
value can be stale when strip readings fail.

## 10. Use routines, remote access, and Alexa

Once the children work in SmartThings, add them to SmartThings routines or
scenes. For Alexa, enable the **SmartThings skill**, link your Samsung account,
and discover devices. Use clear child names such as Desk Lamp. Alexa support
comes through SmartThings; this firmware has no direct Alexa connection.
See [Samsung’s Alexa linking guide](https://www.samsung.com/us/support/answer/ANS10006871/).

Your computer can now be turned off. Leave the ESP32, SmartThings hub, and
strip powered and on the network. Keep the private files in a secure backup.
See the [driver guide](SMARTTHINGS_DRIVER_GUIDE.md) for controls and limitations.
