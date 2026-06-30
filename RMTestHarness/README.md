# RMTestHarness

Native Thunder plugin used by `com.comcast.rmtestapp` to validate Category-2
Essos/ERM resource arbitration scenarios.

## Purpose
This plugin wraps `libessos-resmgr` and exposes JSON-RPC APIs for:
- init/destroy logical RM clients
- request/release resources
- monitor granted/revoked callbacks
- query owner/count/state/caps/AV state
- cleanup all contexts

## Callsign
`org.rdk.RMTestHarness`

## Example JSON-RPC methods
- `org.rdk.RMTestHarness.1.ping`
- `org.rdk.RMTestHarness.1.initClient`
- `org.rdk.RMTestHarness.1.requestResource`
- `org.rdk.RMTestHarness.1.getEvents`
- `org.rdk.RMTestHarness.1.cleanupAll`

## Example curl
```bash
curl -H "Content-Type: application/json" -X POST \
-d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.RMTestHarness.1.ping"}' \
http://127.0.0.1:9998/jsonrpc