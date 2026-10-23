# Security and private setup data

Keep Wi-Fi passwords, APC Home passwords, strip LAN keys, bridge tokens,
configured flash images, and household diagnostics private. Publish only the
clean source tree and firmware built without configured NVS data.

The setup helpers store LAN configuration and a random bridge token in your
user application-data folder, outside the project. On POSIX systems, newly
created private directories/files use permissions 0700/0600. On Windows they
inherit the user profile’s access controls. Back these files up securely.
Neither helper saves the APC account password or account access token.

Configuration goes over USB and is stored in ESP32 NVS. This release does not
enable ESP32 flash encryption or secure boot. Someone with physical access can
potentially extract its configuration. USB setup is available to anyone with
access to the board.

Hub-to-ESP32 HTTP carries a bearer token without TLS. A trusted LAN is required:
someone able to observe that traffic can obtain the token. The strip’s Ayla
datapoints use AES-CBC and HMAC authentication, but LAN discovery and registration
are not a replacement for network isolation from untrusted devices. No inbound
internet access or router port forwarding is required or supported.

The public application identifiers in `tools/capture_key.py` identify the APC
Home client protocol. They are not an owner’s LAN key, Wi-Fi password, or account
token. That account API can change independently of this community software.

Report issues without including secrets. For a suspected security issue, use
GitHub private vulnerability reporting if available, or post only a request
for a private contact route. Do not put exploit data or credentials in public issues.
