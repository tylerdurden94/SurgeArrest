# Local API v1

The ESP32 serves HTTP on port 80, advertises `_surgearrest._tcp.local`, and
accepts strip callbacks on port 8080. All hub API requests use port 80. IPv4
discovery identifies the board by its MAC-derived bridge ID.

| Request | Authentication | Result |
| --- | --- | --- |
| `GET /v1/info` | None | `apiVersion`, `bridgeId`, firmware identifier, configured flag, supported switch names; no token or LAN key |
| `GET /v1/apc` | `X-APC-Token` | Strip session status, observations, command states, and diagnostics |
| `POST /v1/apc/refresh` | `X-APC-Token` | Queues reads; 202 is acceptance, not fresh data |
| `POST /v1/apc/{property}/on` | `X-APC-Token` | Queues an absolute On request |
| `POST /v1/apc/{property}/off` | `X-APC-Token` | Queues an absolute Off request |

Properties: `outlet1`, `outlet2`, `outlet3`, `usb_charger1`, `usb_charger2`, `led`.
Only these names and the exact `on` / `off` actions are accepted.
Missing/incorrect authentication returns 401. Unsupported actions return 400;
other methods/paths return 404. Writes do not require a request body.

The status object includes `switches` (booleans or `null`), `commands` (request
ID, target, phase, and attempt count), and `diagnostics` (registration status,
key exchanges, callback errors, and last error). `connected` describes the
local session bookkeeping, not guaranteed current physical state.
Strip observations expire after 90 seconds.

The `/local_lan` endpoints implement the strip’s Ayla callback protocol. Their
source IP is restricted to the configured strip. Encrypted datapoints require
a valid signature. These are not user control APIs and do not use the bridge
token. Do not expose either HTTP port to the internet.

USB setup accepts `HELLO`, `NETWORK_CONFIG|` followed by a JSON object with
`ssid` and `password`, `APC_CONFIG|IP|KEY_ID|LAN_KEY|BRIDGE_TOKEN`, and `RESTART`.
USB messages must end with a newline and are acknowledged without echoing
secrets. Use the supplied helper instead of entering secrets in a terminal
command or shell history.
