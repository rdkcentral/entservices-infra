# App Gateway Telemetry Markers

This document provides a comprehensive reference for all T2 telemetry markers used in the App Gateway ecosystem.

## Marker Naming Convention

The App Gateway telemetry system uses two types of markers:

### Internal Markers (App Gateway aggregated)
Pattern: `AppGw<Category>_split`
- Used by App Gateway for its own aggregated metrics
- Example: `AppGwBootstrapTime_split`, `AppGwHealthStats_split`

### Generic Plugin Markers (Category-based)
Pattern: `agw_Plugin<Category>_split`
- Single marker per category, shared across all plugins
- Plugin name is included in the payload data, not in the marker name
- Example: `agw_PluginApiError_split` with `{"plugin":"Badger","api":"GetSettings","error":"TIMEOUT"}`

This design avoids duplication of similar markers for each plugin and provides a cleaner, more maintainable approach.

---

## App Gateway Internal Markers

These markers are aggregated and reported by App Gateway itself.

| Telemetry Marker Name | Component | Details | Description |
|-----------------------|-----------|---------|-------------|
| `AppGwBootstrapTime_split` | App Gateway | `duration_ms` (uint64) - total time in milliseconds to start all App Gateway plugins<br>`plugins_loaded` (uint32) - number of plugins successfully loaded | Total time (in milliseconds) to bootstrap all App Gateway ecosystem plugins |
| `AppGwHealthStats_split` | App Gateway | `websocket_connections` (uint32) - current active WebSocket connections<br>`total_calls` (uint32) - total API calls in reporting period<br>`successful_calls` (uint32) - number of successful API calls<br>`failed_calls` (uint32) - number of failed API calls<br>`reporting_interval_sec` (uint32) - reporting interval in seconds | Aggregate health statistics emitted at configurable intervals (default 1 hour) |
| `AppGwApiErrorStats_split` | App Gateway | `reporting_interval_sec` (uint32) - reporting interval in seconds<br>`api_failures` (array) - list of API failures with format `[{"api": "<name>", "count": <uint32>}, ...]` | API failure counts aggregated over the reporting interval |
| `AppGwExtServiceError_split` | App Gateway | `reporting_interval_sec` (uint32) - reporting interval in seconds<br>`service_failures` (array) - list of service failures with format `[{"service": "<name>", "count": <uint32>}, ...]` | External service failure counts aggregated over the reporting interval |

---

## Generic Plugin Markers

These markers are shared across all plugins. The plugin name and specific details are included in the payload data.

| Telemetry Marker Name | Category | Payload Fields | Description |
|-----------------------|----------|----------------|-------------|
| `agw_PluginApiError_split` | API Errors | `plugin` (string) - plugin name (e.g., "Badger", "OttServices")<br>`api` (string) - name of the API that failed<br>`error` (string) - error code or description | Reports API failures from any plugin |
| `agw_PluginExtServiceError_split` | External Service Errors | `plugin` (string) - plugin name<br>`service` (string) - name of external service<br>`error` (string) - error code or description | Reports external service failures from any plugin |
| `agw_PluginApiLatency_split` | API Latency | `plugin` (string) - plugin name<br>`sum` (double) - sum of all latency values<br>`min` (double) - minimum latency<br>`max` (double) - maximum latency<br>`count` (uint32) - number of samples<br>`avg` (double) - average latency<br>`unit` (string) - "ms"<br>`reporting_interval_sec` (uint32) - reporting interval | API call latency metrics in milliseconds |

### Example Payloads

**API Error (JSON format):**
```json
{"plugin":"Badger","api":"GetSettings","error":"TIMEOUT"}
```

**External Service Error (JSON format):**
```json
{"plugin":"OttServices","service":"ThorPermissionService","error":"CONNECTION_TIMEOUT"}
```

**API Latency (JSON format):**
```json
{"plugin":"Badger","sum":1250.5,"min":10.2,"max":450.0,"count":25,"avg":50.02,"unit":"ms","reporting_interval_sec":3600}
```

---

## Registered Plugin Names

Use these predefined constants for the plugin name field.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_PLUGIN_APPGATEWAY` | `AppGateway` | App Gateway main plugin |
| `AGW_PLUGIN_BADGER` | `Badger` | Badger plugin |
| `AGW_PLUGIN_OTTSERVICES` | `OttServices` | OttServices plugin |

---

## Predefined External Service Names

Use these constants when reporting external service errors for consistency in analytics.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_SERVICE_THOR_PERMISSION` | `ThorPermissionService` | Thor Permission gRPC service used by OttServices for permission checks |
| `AGW_SERVICE_OTT_TOKEN` | `OttTokenService` | OTT Token gRPC service used by OttServices for CIMA token generation |
| `AGW_SERVICE_AUTH` | `AuthService` | Auth Service (COM-RPC) used for SAT/xACT token retrieval |
| `AGW_SERVICE_AUTH_METADATA` | `AuthMetadataService` | Auth metadata collection service (token, deviceId, accountId, partnerId) |
| `AGW_SERVICE_OTT_SERVICES` | `OttServices` | OttServices interface (COM-RPC) used by Badger to access OTT permissions |
| `AGW_SERVICE_LAUNCH_DELEGATE` | `LaunchDelegate` | Launch Delegate interface (COM-RPC) for app session management |
| `AGW_SERVICE_LIFECYCLE_DELEGATE` | `LifecycleDelegate` | Lifecycle Delegate for device session management |
| `AGW_SERVICE_PERMISSION` | `PermissionService` | AppGateway internal permission checking service |
| `AGW_SERVICE_AUTHENTICATION` | `AuthenticationService` | AppGateway WebSocket authentication service |

---

## Predefined Error Codes

Use these constants when reporting errors for consistency in analytics.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_ERROR_INTERFACE_UNAVAILABLE` | `INTERFACE_UNAVAILABLE` | COM-RPC interface is not available |
| `AGW_ERROR_INTERFACE_NOT_FOUND` | `INTERFACE_NOT_FOUND` | COM-RPC interface could not be found |
| `AGW_ERROR_CLIENT_NOT_INITIALIZED` | `CLIENT_NOT_INITIALIZED` | Service client not initialized |
| `AGW_ERROR_CONNECTION_REFUSED` | `CONNECTION_REFUSED` | Connection to service was refused |
| `AGW_ERROR_CONNECTION_TIMEOUT` | `CONNECTION_TIMEOUT` | Connection to service timed out |
| `AGW_ERROR_TIMEOUT` | `TIMEOUT` | Operation timed out |
| `AGW_ERROR_PERMISSION_DENIED` | `PERMISSION_DENIED` | Permission check failed |
| `AGW_ERROR_INVALID_RESPONSE` | `INVALID_RESPONSE` | Service returned invalid response |
| `AGW_ERROR_INVALID_REQUEST` | `INVALID_REQUEST` | Request parameters were invalid |
| `AGW_ERROR_NOT_AVAILABLE` | `NOT_AVAILABLE` | Service or resource not available |
| `AGW_ERROR_FETCH_FAILED` | `FETCH_FAILED` | Failed to fetch data from service |
| `AGW_ERROR_UPDATE_FAILED` | `UPDATE_FAILED` | Failed to update data in service |
| `AGW_ERROR_COLLECTION_FAILED` | `COLLECTION_FAILED` | Failed to collect metadata |
| `AGW_ERROR_GENERAL` | `GENERAL_ERROR` | General/unspecified error |

---

## Metric Units

Use these standard units when reporting metrics via `RecordTelemetryMetric()`.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_UNIT_MILLISECONDS` | `ms` | Time in milliseconds |
| `AGW_UNIT_SECONDS` | `sec` | Time in seconds |
| `AGW_UNIT_COUNT` | `count` | Count/quantity |
| `AGW_UNIT_BYTES` | `bytes` | Size in bytes |
| `AGW_UNIT_KILOBYTES` | `KB` | Size in kilobytes |
| `AGW_UNIT_MEGABYTES` | `MB` | Size in megabytes |
| `AGW_UNIT_KBPS` | `kbps` | Bitrate in kilobits per second |
| `AGW_UNIT_MBPS` | `Mbps` | Bitrate in megabits per second |
| `AGW_UNIT_PERCENT` | `percent` | Percentage value |

---

## Data Format

App Gateway supports two output formats for telemetry data:

### JSON Format (Default)
Self-describing format with field names included.

```json
{"websocket_connections":12,"total_calls":1543,"successful_calls":1520,"failed_calls":23,"reporting_interval_sec":3600}
```

### COMPACT Format
Comma-separated values with parentheses grouping for arrays. Smaller payload size.

```
12,1543,1520,23,3600
```

For arrays:
```
3600,(GetData,5),(SetConfig,2),(LoadResource,1)
```

---

## Adding Telemetry to a New Plugin

When adding telemetry to a new plugin, follow these steps:

### 1. Add Plugin Name Constant

In `AppGatewayTelemetryMarkers.h`, add a constant for your plugin name:

```cpp
#define AGW_PLUGIN_MYPLUGIN       "MyPlugin"
```

### 2. Include Required Headers

In your plugin source file:

```cpp
#include <helpers/UtilsAppGatewayTelemetry.h>
```

### 3. Initialize Telemetry Client

In your plugin class, define the telemetry client:

```cpp
// In header or implementation
AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_MYPLUGIN)
```

### 4. Report Telemetry Events

Use the simplified macros - no need to specify markers:

```cpp
// Report API error
AGW_REPORT_API_ERROR("GetSettings", AGW_ERROR_TIMEOUT);

// Report external service error
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_MY_SERVICE, "CONNECTION_FAILED");
```

### 5. Add Plugin-Specific Service Names (Optional)

If your plugin uses specific external services, add constants:

```cpp
#define AGW_SERVICE_MY_EXTERNAL_SERVICE   "MyExternalService"
```

Then update this documentation to include the new service constant.

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.1 | 2026-01-31 | - | Refactored to generic category-based markers |
| 1.0 | 2026-01-31 | - | Initial release |
