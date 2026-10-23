# Troubleshooting

| Symptom | What to check |
| --- | --- |
| USB port missing | Use a USB data cable, try another port, and install the board maker’s USB driver if needed. |
| Flash cannot connect | Confirm a classic ESP32 board and correct port, close serial monitors, and try holding BOOT during initial connection. |
| Firmware will not start | The release merged image goes at 0x0. A standalone application `firmware.bin` goes at 0x10000 with its matching bootloader/partitions. Do not mix these images. |
| Capture sign-in fails | Verify the APC Home password in the official app. Avoid repeated retries. Check internet access; account APIs may change or be retired. |
| No LAN key returned | Confirm the strip is already in APC Home and LAN-enabled. v0.1.0 has no substitute for an unavailable LAN key. |
| Setup does not acknowledge | Check the selected USB port and that this standalone firmware is flashed. Close other serial programs and press EN/reset before retrying. |
| No Wi-Fi connection | Use an exact 2.4 GHz SSID/password; re-run USB setup. Configuration is saved and a restart applies the network settings. |
| SmartThings finds nothing | Install the driver on the correct LAN Edge hub, finish USB setup, and allow local traffic/mDNS between hub and ESP32. Guest Wi-Fi/VLAN isolation often prevents discovery. |
| Device appears but controls do nothing | Enter the private **bridge token** in the parent’s settings. Do not use the LAN key or account password there. |
| ESP32 responds but strip does not | Confirm the strip’s reserved IP matches setup; re-run with `--device-ip` if it changed. Close competing local-control sessions. |
| Stale switch readings | The strip may be unreachable even while the ESP32 is online. Check `/v1/apc` diagnostics and observe the physical outlet. Last known SmartThings values are retained when state is unknown. |
| Duplicate-looking controls | Parent and six child devices are intentional. Parent main is Outlet 1, not master power. Rename children for rooms/Alexa. |
| Alexa finds no devices | First make the child work in SmartThings, then link the SmartThings skill to the correct Samsung account and discover devices. |

## Strip session recovery

A previous integration experienced a stall in which the strip stopped returning
authenticated state and stopped fetching commands. Renewing its session and
restarting the bridge did not recover it; switching the strip’s main power off
and on did. The cause remains unresolved and this release does not claim to fix it.

Inspect connected equipment before cycling the strip: doing so interrupts power
to its loads. Restarting only the ESP32 is a less disruptive first step, but may
not resolve this particular strip condition. After recovery, verify readings and
the physical state of the affected control.

## Get help

Open an [issue](https://github.com/tylerdurden94/SurgeArrest/issues) with the strip
model, ESP32 family, firmware version, driver version, and a description of the
problem. Remove LAN tokens, Wi-Fi information, device identifiers, private IPs,
and account information from logs/screenshots. Never attach `config.json`, token
files, or a configured flash backup. See [security notes](../SECURITY.md).
