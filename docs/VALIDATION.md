# Validation and known limits — v0.1.0 beta

## Checks completed on October 23, 2026

- **17 simulated Edge driver tests** passed using the real Lua driver:
  routing, observed-only events, unknown-state handling, bounded retries,
  superseded requests, authentication errors, transport behavior, standalone
  discovery, generic labels, and parent LED control.
- **7 setup tests** passed using simulated account/USB responses: configuration
  validation, private-file overwrite protection, acknowledgement handling,
  no secret echo, no automatic sign-in retries, and minimal saved cloud output.
- The actual production C++ delivery state machine passed assertions covering
  bounded retries, 45-second expiry, replacement, confirmation correlation,
  timer wrap, and independent outlet requests.
- The standalone ESP32 firmware built successfully for `esp32dev` using
  Espressif32 7.1.3 / Arduino core 2.0.17. Flash use: **873,385 bytes (66.6%)**;
  static RAM: **49,188 bytes (15.0%)**.
- SmartThings accepted the public driver package, including its standalone
  `_surgearrest._tcp` discovery definition and three standard-capability profiles.

The APC protocol and delivery files are byte-for-byte copies of the previously
tested APC implementation. Driver control behavior is retained; publication
changes are the driver name/package identity, generic Outlet 1 label, setup
wording, and dedicated discovery service. The standalone Wi-Fi/USB/server
wrapper and setup helpers are new.

## Earlier physical evidence

The original integration controlled a reference **PH6U4X32W** through an ESP32
and SmartThings. The owner physically confirmed Outlet 1 off/on with the
strip’s WAN connection blocked, Outlet 2 off/on, and LED control from both
the parent and child. Authenticated state readback and a later SmartThings
Outlet 1 On request were verified. This evidence applies to the retained APC
control path; it is not a clean installation test of this new standalone build.

## Still unverified

- A complete flash, USB provisioning, discovery, and control test on a fresh
  physical board running this standalone firmware.
- Installation through the public channel on another user's hub/account.
- The new capture helper against a live APC/Ayla account. Its API shape matches
  the earlier bootstrap and is covered by mocks; service availability can change.
- Physical testing of every AC outlet and both USB charger groups.
- Multiple dedicated boards/strips in one installation and other APC models.
- Long-term behavior after strip network/session faults.

A prior session stall stopped authenticated observations and command fetching.
Restarting the ESP32 and renewing the session did not resolve it; power-cycling
the strip did. Its root cause remains unresolved, and v0.1.0 does not claim a
fix. Unknown ESP32 readings expire after 90 seconds, while SmartThings retains
its last observed state. See [troubleshooting](TROUBLESHOOTING.md).

No production board was flashed, no account sign-in was attempted, and no
outlet commands were sent while preparing this publication. This release is
marked **beta** until the new standalone path receives physical validation.
