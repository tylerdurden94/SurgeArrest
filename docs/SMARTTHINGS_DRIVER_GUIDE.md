# SmartThings controls and settings

Install **APC SurgeArrest ESP Bridge** from the shared
**[TylerDurden94’s Edge Drivers](https://bestow-regional.api.smartthings.com/invite/oDM8Q4Bkgp2L)** channel.
Users do not need the SmartThings CLI or a custom capability namespace.
This driver uses standard switch and refresh capabilities.

## Devices and controls

| Device/control | Strip function |
| --- | --- |
| Parent main switch | Outlet 1 only; not the entire strip |
| Outlet 2 / Outlet 3 | Corresponding AC outlet |
| USB charger 1 / USB charger 2 | Corresponding USB charger group |
| Strip LED | Strip indicator setting |
| Six child devices | The same six functions, available individually for rooms and routines |

Changes from either parent or child route to the same strip property. Authenticated
readings update both views. User renames are retained. Alexa may discover both
parent and children; give the children clear names and avoid confusing duplicates.
The driver does not provide power metering, energy totals, surge health, an
all-outlet master switch, or individual control of every physical USB socket.

## Settings

The parent’s **APC bridge token** must match the random token saved by USB setup.
The setting is a password field. Child devices use their parent’s token and have
no separate setup. The APC account password and strip LAN key stay out of this
SmartThings setting. No other driver preferences are exposed in v0.1.0.

## Feedback, refresh, and retries

- The ESP32 requests actual strip state every **30 seconds** and after commands.
- The hub reads the ESP32’s cached state every **60 seconds**. Refresh reads that
  cache; it does not itself force a fresh strip transaction.
- After an accepted command, the driver checks at 2, 9, 18, and 25 seconds.
- Transient hub-to-ESP32 delivery failures get at most three attempts.
- The ESP32 makes at most three delivery attempts to the strip, renewing its
  session between retries. Requests expire after 45 seconds. A new request for
  the same control replaces its older target.
- Acceptance or ciphertext delivery does not count as physical confirmation.

A reading expires to `null` after **90 seconds** without a fresh observation.
The driver emits only boolean observations, so SmartThings retains the last
known On/Off value when a reading becomes unknown. An online parent means the
ESP32 API responded; it does not prove the strip has a working session or
fresh readings. Unconfirmed commands are logged, and physical state can remain
unknown. The app does not expose these diagnostics as a separate capability.

## Routines and Alexa

Use the six child devices for individual automation. Alexa integration comes
through the SmartThings skill and linked Samsung account. It needs working
SmartThings discovery first and the usual SmartThings/Amazon internet services.
No APC-specific Alexa skill is used by this control path.

## Multiple strips

This firmware stores one strip per dedicated ESP32. Additional boards may
be discovered by the same driver, each with its own token. Multiple-strip use
has not been physically validated for this release.
