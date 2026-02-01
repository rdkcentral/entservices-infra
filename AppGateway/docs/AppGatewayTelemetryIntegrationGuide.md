# App Gateway Telemetry Integration Guide

## Overview

This guide is for developers of **external plugins** and **services** who need to report telemetry data to App Gateway for T2 (Telemetry 2.0) aggregation and reporting.

App Gateway provides a centralized telemetry collection mechanism that:
- Aggregates error statistics from all connected plugins
- Reports health metrics at configurable intervals
- Forwards events to T2 for analytics dashboards

## Quick Links

- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual flows for all telemetry scenarios
- **[Marker Reference](./AppGatewayTelemetryMarkers.md)** - Complete list of predefined markers and constants
- **[Architecture Overview](./AppGatewayTelemetry_Architecture.md)** - System architecture and design

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          App Gateway Ecosystem                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                     │
│  │    Badger    │   │ OttServices  │   │  YourPlugin  │                     │
│  │    Plugin    │   │    Plugin    │   │   (New)      │                     │
│  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘                     │
│         │                  │                  │                              │
│         │  COM-RPC         │  COM-RPC         │  COM-RPC                    │
│         │                  │                  │                              │
│         ▼                  ▼                  ▼                              │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │              IAppGatewayTelemetry Interface                  │            │
│  │  ┌─────────────────────────────────────────────────────────┐│            │
│  │  │ RecordTelemetryEvent(context, eventName, eventData)     ││            │
│  │  │ RecordTelemetryMetric(context, metricName, value, unit) ││            │
│  │  └─────────────────────────────────────────────────────────┘│            │
│  └─────────────────────────────────────────────────────────────┘            │
│                              │                                               │
│                              ▼                                               │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │              AppGatewayTelemetry (Aggregator)                │            │
│  │  • Aggregates API errors by plugin/API name                  │            │
│  │  • Aggregates external service errors by service name        │            │
│  │  • Reports at configurable intervals (default: 1 hour)       │            │
│  └──────────────────────────┬──────────────────────────────────┘            │
│                              │                                               │
│                              ▼                                               │
│                     ┌────────────────┐                                       │
│                     │   T2 Service   │                                       │
│                     │  (Analytics)   │                                       │
│                     └────────────────┘                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Quick Start

### Step 1: Include Required Headers

```cpp
#include "UtilsAppGatewayTelemetry.h"
#include "AppGatewayTelemetryMarkers.h"
```

### Step 2: Initialize Telemetry in Your Plugin

In your plugin's `Initialize()` method:

```cpp
uint32_t YourPlugin::Initialize(PluginHost::IShell* service)
{
    // ... your initialization code ...
    
    // Initialize telemetry client (queries IAppGatewayTelemetry interface)
    AGW_TELEMETRY_INIT(service);
    
    return Core::ERROR_NONE;
}
```

### Step 3: Report Errors at Failure Points

```cpp
// Report API errors
AGW_REPORT_API_ERROR(AGW_MARKER_YOURPLUGIN_API_ERROR, "GetData", "INVALID_PARAMETER");

// Report external service errors
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_MARKER_YOURPLUGIN_EXT_SERVICE_ERROR, 
                                   AGW_SERVICE_AUTH, "CONNECTION_TIMEOUT");
```

### Step 4: Deinitialize on Shutdown

In your plugin's `Deinitialize()` method:

```cpp
void YourPlugin::Deinitialize(PluginHost::IShell* service)
{
    // Deinitialize telemetry client
    AGW_TELEMETRY_DEINIT();
    
    // ... your cleanup code ...
}
```

---

## Detailed API Reference

### Macros

#### `AGW_TELEMETRY_INIT(service)`

Initializes the telemetry client by querying the `IAppGatewayTelemetry` interface from App Gateway.

| Parameter | Type | Description |
|-----------|------|-------------|
| `service` | `PluginHost::IShell*` | The service shell passed to `Initialize()` |

**Example:**
```cpp
AGW_TELEMETRY_INIT(service);
```

#### `AGW_TELEMETRY_DEINIT()`

Releases the telemetry client and cleans up resources.

**Example:**
```cpp
AGW_TELEMETRY_DEINIT();
```

#### `AGW_REPORT_API_ERROR(api, error)`

Reports an API error event to App Gateway. The plugin name is automatically included from the `AGW_DEFINE_TELEMETRY_CLIENT` definition, and the generic marker `agw_PluginApiError_split` is used internally.

| Parameter | Type | Description |
|-----------|------|-------------|
| `api` | `const char*` | Name of the API that failed |
| `error` | `const char*` | Error code or description |

**Example:**
```cpp
AGW_REPORT_API_ERROR("AuthorizeDataField", "PERMISSION_DENIED");
```

#### `AGW_REPORT_EXTERNAL_SERVICE_ERROR(service, error)`

Reports an external service error event to App Gateway. The plugin name is automatically included from the `AGW_DEFINE_TELEMETRY_CLIENT` definition, and the generic marker `agw_PluginExtServiceError_split` is used internally.

| Parameter | Type | Description |
|-----------|------|-------------|
| `service` | `const char*` | Name of the external service (use `AGW_SERVICE_*` constants) |
| `error` | `const char*` | Error code or description |

**Example:**
```cpp
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, "CONNECTION_REFUSED");
```

#### `AGW_REPORT_API_LATENCY(apiName, latencyMs)`

Reports API call latency metric to App Gateway. The plugin name is automatically included.

| Parameter | Type | Description |
|-----------|------|-------------|
| `apiName` | `const char*` | Name of the API |
| `latencyMs` | `double` | Latency in milliseconds |

**Example:**
```cpp
AGW_REPORT_API_LATENCY("GetAppPermissions", 150.5);
```

#### `AGW_REPORT_SERVICE_LATENCY(serviceName, latencyMs)`

Reports external service call latency metric to App Gateway.

| Parameter | Type | Description |
|-----------|------|-------------|
| `serviceName` | `const char*` | Predefined service name from `AppGatewayTelemetryMarkers.h` |
| `latencyMs` | `double` | Latency in milliseconds |

**Example:**
```cpp
AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_TOKEN, 200.0);
```

#### `AGW_SCOPED_API_TIMER(varName, apiName)`

Creates an RAII-style timer that automatically tracks API latency. On destruction, it reports the elapsed time. If an error occurs, call `SetFailed(errorCode)` to mark the API call as failed.

| Parameter | Type | Description |
|-----------|------|-------------|
| `varName` | identifier | Variable name for the timer |
| `apiName` | `const char*` | Name of the API being timed |

**Example:**
```cpp
Core::hresult MyPlugin::GetData(const string& key, string& value) {
    AGW_SCOPED_API_TIMER(timer, "GetData");
    
    auto result = FetchFromCache(key, value);
    if (result != Core::ERROR_NONE) {
        timer.SetFailed(AGW_ERROR_NOT_FOUND);
        return result;
    }
    
    return Core::ERROR_NONE;
    // Timer automatically reports success latency on destruction
}
```

---

## Generic Markers

All markers are defined in `AppGatewayTelemetryMarkers.h`. The system uses **generic category-based markers** where the plugin name is included in the payload data rather than the marker name.

### Category Markers

| Marker | Description |
|--------|-------------|
| `AGW_MARKER_PLUGIN_API_ERROR` | API failures from any plugin (plugin name in payload) |
| `AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR` | External service failures from any plugin |
| `AGW_MARKER_PLUGIN_API_LATENCY` | API call latency metrics from any plugin |

### Plugin Name Constants

Use these constants when defining the telemetry client:

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_PLUGIN_APPGATEWAY` | `"AppGateway"` | App Gateway main plugin |
| `AGW_PLUGIN_BADGER` | `"Badger"` | Badger plugin |
| `AGW_PLUGIN_OTTSERVICES` | `"OttServices"` | OttServices plugin |

### Service Name Constants

Use these constants when reporting external service errors:

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_SERVICE_THOR_PERMISSION` | `"ThorPermissionService"` | Thor Permission gRPC service |
| `AGW_SERVICE_OTT_TOKEN` | `"OttTokenService"` | OTT Token gRPC service |
| `AGW_SERVICE_AUTH` | `"AuthService"` | Auth Service (COM-RPC) |
| `AGW_SERVICE_AUTH_METADATA` | `"AuthMetadataService"` | Auth metadata collection |
| `AGW_SERVICE_OTT_SERVICES` | `"OttServices"` | OttServices interface |
| `AGW_SERVICE_LAUNCH_DELEGATE` | `"LaunchDelegate"` | Launch Delegate interface |
| `AGW_SERVICE_LIFECYCLE_DELEGATE` | `"LifecycleDelegate"` | Lifecycle Delegate interface |
| `AGW_SERVICE_PERMISSION` | `"PermissionService"` | Internal permission service |
| `AGW_SERVICE_AUTHENTICATION` | `"AuthenticationService"` | WebSocket authentication |

### Predefined Error Codes

Use these for consistency in error reporting:

| Constant | Value |
|----------|-------|
| `AGW_ERROR_INTERFACE_UNAVAILABLE` | `"INTERFACE_UNAVAILABLE"` |
| `AGW_ERROR_INTERFACE_NOT_FOUND` | `"INTERFACE_NOT_FOUND"` |
| `AGW_ERROR_CLIENT_NOT_INITIALIZED` | `"CLIENT_NOT_INITIALIZED"` |
| `AGW_ERROR_CONNECTION_REFUSED` | `"CONNECTION_REFUSED"` |
| `AGW_ERROR_CONNECTION_TIMEOUT` | `"CONNECTION_TIMEOUT"` |
| `AGW_ERROR_TIMEOUT` | `"TIMEOUT"` |

### Metric Units

| Constant | Value |
|----------|-------|
| `AGW_UNIT_MILLISECONDS` | `"ms"` |
| `AGW_UNIT_SECONDS` | `"sec"` |
| `AGW_UNIT_COUNT` | `"count"` |
| `AGW_UNIT_BYTES` | `"bytes"` |
| `AGW_UNIT_PERCENT` | `"percent"` |

---

## Usage Examples

### Example 1: Simple API Error Reporting

```cpp
Core::hresult OttServicesImplementation::GetAppPermissions(const string& appId, ...) {
    if (!_perms) {
        LOGERR("PermissionsClient not initialized");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, "CLIENT_NOT_INITIALIZED");
        return Core::ERROR_UNAVAILABLE;
    }
    // ... rest of implementation
}
```

### Example 2: Automatic Latency Tracking with Scoped Timer

```cpp
std::string Badger::GetDeviceSessionId(const Exchange::GatewayContext& context, const string& appId) {
    // Track API latency using scoped timer
    AGW_SCOPED_API_TIMER(timer, "GetDeviceSessionId");
    
    string deviceSessionId = "app_session_id.not.set";

    if (!mDelegateFactory) {
        LOGERR("DelegateFactory not initialized.");
        timer.SetFailed(AGW_ERROR_NOT_AVAILABLE);
        return deviceSessionId;
    }
    
    auto lifecycle = mDelegateFactory->getDelegate<LifecycleDelegate>();
    if (!lifecycle) {
        LOGERR("LifecycleDelegate not available.");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_LIFECYCLE_DELEGATE, AGW_ERROR_NOT_AVAILABLE);
        timer.SetFailed(AGW_ERROR_NOT_AVAILABLE);
        return deviceSessionId;
    }
    
    if (lifecycle->GetDeviceSessionId(context, deviceSessionId) != Core::ERROR_NONE) {
        LOGERR("Failed to get device session ID");
        AGW_REPORT_API_ERROR("GetDeviceSessionId", AGW_ERROR_FETCH_FAILED);
        timer.SetFailed(AGW_ERROR_FETCH_FAILED);
        return deviceSessionId;
    }
    
    return deviceSessionId;
    // Timer automatically reports success latency on destruction
}
```

### Example 3: Manual Service Latency Tracking

```cpp
Core::hresult Badger::AuthorizeDataField(const std::string& appId, const char* requiredDataField) {
    AGW_SCOPED_API_TIMER(apiTimer, "AuthorizeDataField");
    
    // ... check cache first ...
    
    // Track external service call latency
    auto serviceCallStart = std::chrono::steady_clock::now();
    
    Exchange::IOttServices* ottServices = GetOttServices();
    if (ottServices == nullptr) {
        LOGERR("OttServices interface not available");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE);
        apiTimer.SetFailed(AGW_ERROR_INTERFACE_UNAVAILABLE);
        return Core::ERROR_UNAVAILABLE;
    }
    
    RPC::IStringIterator* permissionsIterator = nullptr;
    if (ottServices->GetAppPermissions(appId, false, permissionsIterator) != Core::ERROR_NONE) {
        LOGERR("GetAppPermissions failed");
        AGW_REPORT_API_ERROR("GetAppPermissions", AGW_ERROR_PERMISSION_DENIED);
        apiTimer.SetFailed(AGW_ERROR_PERMISSION_DENIED);
        return Core::ERROR_PRIVILIGED_REQUEST;
    }
    
    // Track external service call latency
    auto serviceCallEnd = std::chrono::steady_clock::now();
    auto serviceLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        serviceCallEnd - serviceCallStart).count();
    AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_SERVICES, static_cast<double>(serviceLatencyMs));
    
    // ... process permissions ...
    
    return Core::ERROR_NONE;
}
```

### Example 4: Combined Event and Metric Reporting

```cpp
Core::hresult OttServicesImplementation::GetAppCIMAToken(const string& appId, string& token) {
    // Check cache first
    const std::string cacheKey = std::string("platform:") + appId;
    if (_tokenCache.Get(cacheKey, token)) {
        return Core::ERROR_NONE;  // Cache hit - fast path
    }
    
    // Track token service call latency
    auto tokenServiceStart = std::chrono::steady_clock::now();
    
    // Fetch SAT and xACT
    std::string sat, xact;
    uint64_t satExpiry = 0, xactExpiry = 0;
    
    if (!FetchSat(sat, satExpiry)) {
        LOGERR("FetchSat failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, "FETCH_SAT_FAILED");
        return Core::ERROR_UNAVAILABLE;
    }
    
    if (!FetchXact(appId, xact, xactExpiry)) {
        LOGERR("FetchXact failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, "FETCH_XACT_FAILED");
        return Core::ERROR_UNAVAILABLE;
    }
    
    // Get platform token
    std::string err;
    uint32_t expiresInSec = 0;
    const bool ok = _token->GetPlatformToken(appId, xact, sat, token, expiresInSec, err);
    if (!ok) {
        LOGERR("GetPlatformToken failed: %s", err.c_str());
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_TOKEN, err.c_str());
        return Core::ERROR_UNAVAILABLE;
    }
    
    // Report service latency metric
    auto tokenServiceEnd = std::chrono::steady_clock::now();
    auto tokenLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        tokenServiceEnd - tokenServiceStart).count();
    AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_TOKEN, static_cast<double>(tokenLatencyMs));
    
    // Cache the token
    // ...
    
    return Core::ERROR_NONE;
}
```

### Example 5: Custom Metric Reporting (Badger, OttServices)

```cpp
// In Badger.cpp: Track PermissionGroup failure count
static uint32_t permissionGroupFailureCount = 0;
// ... inside failure handling ...
++permissionGroupFailureCount;
AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, permissionGroupFailureCount, AGW_UNIT_COUNT);

// In OttServicesImplementation.cpp: Track gRPC failure count
static uint32_t grpcFailureCount = 0;
// ... inside failure handling ...
++grpcFailureCount;
AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, grpcFailureCount, AGW_UNIT_COUNT);
```

---

## Adding Markers for a New Plugin

If you're integrating a new plugin, you need to add your plugin's markers to `AppGatewayTelemetryMarkers.h`.

### Step 1: Add Plugin Section

Add a new section in `AppGatewayTelemetryMarkers.h`:

```cpp
//=============================================================================
// YOURPLUGIN MARKERS
// Used by YourPlugin when reporting telemetry to AppGateway
//=============================================================================

/**
 * @brief YourPlugin API error event marker
 * @details Reports API failures within YourPlugin
 * @eventData { "api": "<apiName>", "error": "<errorDetails>", "plugin": "YourPlugin" }
 */
#define AGW_MARKER_YOURPLUGIN_API_ERROR             "agw_YourPluginApiError_split"

/**
 * @brief YourPlugin external service error event marker
 * @details Reports external service failures from YourPlugin
 * @eventData { "service": "<serviceName>", "error": "<errorDetails>", "plugin": "YourPlugin" }
 */
#define AGW_MARKER_YOURPLUGIN_EXT_SERVICE_ERROR     "agw_YourPluginExtServiceError_split"

/**
 * @brief YourPlugin API latency metric marker
 * @details Reports API call latency in milliseconds
 * @unit AGW_UNIT_MILLISECONDS
 */
#define AGW_MARKER_YOURPLUGIN_API_LATENCY           "agw_YourPluginApiLatency_split"
```

### Step 2: Add Service Names (if new services)

If your plugin interacts with new external services, add them:

```cpp
/**
 * @brief Your New Service
 * @details Description of the service
 */
#define AGW_SERVICE_YOUR_SERVICE                    "YourServiceName"
```

### Naming Convention

All markers must follow this pattern:

```
agw_<PluginName><Category><Type>_split
```

| Component | Description | Examples |
|-----------|-------------|----------|
| `agw_` | App Gateway prefix (mandatory) | - |
| `<PluginName>` | Name of your plugin | `Badger`, `OttServices`, `YourPlugin` |
| `<Category>` | Category of telemetry | `Api`, `ExtService`, `Token` |
| `<Type>` | Type of data | `Error`, `Latency`, `Stats` |
| `_split` | Suffix for structured data | - |

---

## Complete Integration Example

Here's a complete example for a hypothetical `FbSettings` plugin:

### 1. Add Markers to `AppGatewayTelemetryMarkers.h`

```cpp
//=============================================================================
// FBSETTINGS PLUGIN MARKERS
//=============================================================================

#define AGW_MARKER_FBSETTINGS_API_ERROR             "agw_FbSettingsApiError_split"
#define AGW_MARKER_FBSETTINGS_EXT_SERVICE_ERROR     "agw_FbSettingsExtServiceError_split"
#define AGW_MARKER_FBSETTINGS_API_LATENCY           "agw_FbSettingsApiLatency_split"
```

### 2. Integrate in Plugin Implementation

```cpp
// FbSettingsImplementation.cpp

#include "FbSettingsImplementation.h"
#include "UtilsAppGatewayTelemetry.h"
#include "AppGatewayTelemetryMarkers.h"

namespace WPEFramework {
namespace Plugin {

    uint32_t FbSettingsImplementation::Initialize(PluginHost::IShell* service)
    {
        _service = service;
        
        // Initialize telemetry
        AGW_TELEMETRY_INIT(service);
        
        // Initialize settings backend
        if (!InitializeBackend()) {
            AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_MARKER_FBSETTINGS_EXT_SERVICE_ERROR,
                                               "SettingsBackend", "INIT_FAILED");
            return Core::ERROR_UNAVAILABLE;
        }
        
        return Core::ERROR_NONE;
    }

    void FbSettingsImplementation::Deinitialize(PluginHost::IShell* service)
    {
        AGW_TELEMETRY_DEINIT();
        // ... cleanup ...
    }

    uint32_t FbSettingsImplementation::GetSetting(const string& key, string& value)
    {
        if (key.empty()) {
            AGW_REPORT_API_ERROR(AGW_MARKER_FBSETTINGS_API_ERROR,
                                  "GetSetting", "INVALID_KEY");
            return Core::ERROR_BAD_REQUEST;
        }

        if (!_backend->Fetch(key, value)) {
            AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_MARKER_FBSETTINGS_EXT_SERVICE_ERROR,
                                               "SettingsBackend", "FETCH_FAILED");
            return Core::ERROR_UNAVAILABLE;
        }

        return Core::ERROR_NONE;
    }

    uint32_t FbSettingsImplementation::SetSetting(const string& key, const string& value)
    {
        if (key.empty()) {
            AGW_REPORT_API_ERROR(AGW_MARKER_FBSETTINGS_API_ERROR,
                                  "SetSetting", "INVALID_KEY");
            return Core::ERROR_BAD_REQUEST;
        }

        auto start = std::chrono::steady_clock::now();
        
        if (!_backend->Store(key, value)) {
            AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_MARKER_FBSETTINGS_EXT_SERVICE_ERROR,
                                               "SettingsBackend", "STORE_FAILED");
            return Core::ERROR_UNAVAILABLE;
        }

        auto end = std::chrono::steady_clock::now();
        double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        AGW_REPORT_METRIC(AGW_MARKER_FBSETTINGS_API_LATENCY, latencyMs, AGW_UNIT_MILLISECONDS);

        return Core::ERROR_NONE;
    }

} // namespace Plugin
} // namespace WPEFramework
```

---

## Best Practices

### 1. Report at the Point of Failure

Place telemetry calls immediately after detecting a failure:

```cpp
// ✓ Good - report at failure point
if (!service->Connect()) {
    LOGERR("Connection failed");
    AGW_REPORT_EXTERNAL_SERVICE_ERROR(marker, service, "CONNECTION_FAILED");
    return Core::ERROR_UNAVAILABLE;
}

// ✗ Bad - reporting too late or in wrong location
```

### 2. Use Consistent Error Codes

Use predefined error codes when applicable:

```cpp
// ✓ Good - uses predefined constant
AGW_REPORT_EXTERNAL_SERVICE_ERROR(marker, AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_TIMEOUT);

// ✓ Also good - specific error when predefined doesn't exist
AGW_REPORT_EXTERNAL_SERVICE_ERROR(marker, AGW_SERVICE_AUTH, "INVALID_TOKEN_FORMAT");
```

### 3. Don't Over-Report

Report significant errors, not every minor issue:

```cpp
// ✓ Good - report service unavailability
if (!interface) {
    AGW_REPORT_EXTERNAL_SERVICE_ERROR(marker, service, AGW_ERROR_INTERFACE_UNAVAILABLE);
}

// ✗ Bad - don't report expected/handled conditions
if (cache.empty()) {
    // This is normal, don't report as error
    RefreshCache();
}
```

### 4. Use Simplified Macros

Use the simplified macros that automatically include the plugin name:

```cpp
// ✓ Good - uses simplified macro
AGW_REPORT_API_ERROR("GetData", "FAILED");

// ✓ Good - uses service constant
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, "CONNECTION_REFUSED");
```

### 5. Include Context in Logging

The telemetry macros don't replace logging—use both:

```cpp
// ✓ Good - log with context, then report telemetry
LOGERR("GetAppPermissions failed for appId='%s': %s", appId.c_str(), error.c_str());
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, error.c_str());
```

---

## Troubleshooting

### Telemetry Not Being Reported

1. **Check if App Gateway is running**: The `IAppGatewayTelemetry` interface is only available when App Gateway plugin is active.

2. **Verify initialization**: Ensure `AGW_TELEMETRY_INIT(service)` is called in `Initialize()`.

3. **Check logs**: Look for initialization warnings:
   ```
   [WARN] AppGatewayTelemetry: Failed to acquire IAppGatewayTelemetry interface
   ```

### Interface Query Fails

If telemetry initialization fails, the macros will safely do nothing. Check:

1. App Gateway plugin is loaded before your plugin
2. Plugin activation order in configuration
3. COM-RPC connectivity between plugins

### Markers Not Recognized in T2

1. Ensure markers end with `_split` suffix
2. Verify marker is defined in `AppGatewayTelemetryMarkers.h`
3. Check T2 configuration includes the new marker

---

## Support

For questions or issues with telemetry integration:

1. Review existing implementations in `Badger` and `OttServices` plugins
2. Check `AppGatewayTelemetryMarkers.h` for all available markers
3. Contact the App Gateway team for new marker requests

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-01-31 | Initial release |
