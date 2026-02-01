# App Gateway Telemetry Sequence Diagrams

This directory contains detailed sequence diagrams for all App Gateway telemetry scenarios, illustrating the complete flow from event occurrence to T2 reporting.

## Diagrams Overview

| Diagram | Scenario | Key Plugins | Description |
|---------|----------|-------------|-------------|
| [01_Bootstrap_Time_Tracking.md](./01_Bootstrap_Time_Tracking.md) | Bootstrap Time | AppGateway | Measures total time to initialize all plugins during system startup |
| [02_Health_Stats_Reporting.md](./02_Health_Stats_Reporting.md) | Health Stats | AppGateway, AppGatewayResponder | Tracks WebSocket connections and API call statistics, reported periodically |
| [03_API_Error_Reporting_Badger.md](./03_API_Error_Reporting_Badger.md) | API Errors | Badger | Example of reporting API failures using generic markers via COM-RPC |
| [04_External_Service_Error_OttServices.md](./04_External_Service_Error_OttServices.md) | External Service Errors | OttServices | Example of reporting gRPC service errors with multi-plugin aggregation |
| [05_Metric_Latency_Tracking.md](./05_Metric_Latency_Tracking.md) | Latency Metrics | Badger, OttServices | API and service latency tracking using scoped timers and manual timing |

## Telemetry Architecture

### Four Core Scenarios

1. **Bootstrap Time Tracking**
   - Measured once during AppGateway initialization
   - Reports total time and number of plugins loaded
   - Marker: `AppGwBootstrapTime_split`

2. **Health Stats Reporting**
   - Continuous tracking of WebSocket connections and API calls
   - Periodic reporting (default: every hour)
   - Marker: `AppGwHealthStats_split`

3. **API Error Reporting**
   - Plugins report API failures via COM-RPC
   - Uses generic marker with plugin name in payload
   - Marker: `agw_PluginApiError_split` (individual), `AppGwApiErrorStats_split` (aggregated)

4. **External Service Error Reporting**
   - Plugins report external service (gRPC, COM-RPC) failures
   - Aggregated across all plugins
   - Marker: `agw_PluginExtServiceError_split` (individual), `AppGwExtServiceError_split` (aggregated)

5. **Latency Metric Tracking**
   - API latency: Automatic via scoped timers
   - Service latency: Manual timing of external service calls
   - Markers: `agw_PluginApiLatency_split`, `agw_PluginServiceLatency_split`

### Generic Marker System

The telemetry system uses **generic category-based markers** where:
- Single marker per category (e.g., `agw_PluginApiError_split`)
- Plugin name included in payload data
- Avoids marker duplication across plugins
- Enables cross-plugin aggregation

**Example:**
```json
{
  "plugin": "Badger",
  "api": "GetDeviceSessionId",
  "error": "TIMEOUT"
}
```

## COM-RPC Interface

Plugins communicate with AppGatewayTelemetry via the `IAppGatewayTelemetry` interface:

```cpp
// Event reporting (errors)
Core::hresult RecordTelemetryEvent(const GatewayContext& context,
                                   const string& eventName,
                                   const string& eventData);

// Metric reporting (latency, counts)
Core::hresult RecordTelemetryMetric(const GatewayContext& context,
                                    const string& metricName,
                                    const double metricValue,
                                    const string& metricUnit);
```

## Helper Macros

Plugins use convenience macros from `UtilsAppGatewayTelemetry.h`:

| Macro | Purpose |
|-------|---------|
| `AGW_TELEMETRY_INIT(service, pluginName)` | Initialize telemetry client |
| `AGW_REPORT_API_ERROR(api, error)` | Report API failure |
| `AGW_REPORT_EXTERNAL_SERVICE_ERROR(service, error)` | Report service failure |
| `AGW_REPORT_API_LATENCY(api, latencyMs)` | Report API latency |
| `AGW_REPORT_SERVICE_LATENCY(service, latencyMs)` | Report service latency |
| `AGW_SCOPED_API_TIMER(var, apiName)` | Auto-track API latency (RAII) |

## Reference Plugins

### Badger Plugin
- Demonstrates automatic API latency tracking with scoped timers
- Shows manual service latency measurement for OttServices calls
- Reports external service errors (LifecycleDelegate, OttServices)
- Example APIs: `GetDeviceSessionId`, `AuthorizeDataField`

### OttServices Plugin
- Demonstrates API latency tracking for permission operations
- Shows service latency for gRPC calls (ThorPermissionService, OttTokenService)
- Reports gRPC client initialization errors
- Example APIs: `GetAppPermissions`, `GetAppCIMAToken`

## Data Flow Summary

```
Plugin API Call
    ↓
Error Detection / Timing
    ↓
AGW_REPORT_* Macro
    ↓
TelemetryClient (Plugin)
    ↓
COM-RPC → IAppGatewayTelemetry
    ↓
AppGatewayTelemetry (Aggregation)
    ↓
Periodic Timer (1 hour)
    ↓
Format (JSON or COMPACT)
    ↓
T2 Telemetry Server
```

## Mermaid Diagram Rendering

All sequence diagrams use Mermaid syntax for easy rendering in:
- GitHub/GitLab (native support)
- VS Code (with Mermaid extension)
- Documentation sites (Markdown processors with Mermaid support)

To view diagrams:
1. **GitHub/GitLab**: View the .md files directly in the web interface
2. **VS Code**: Install "Markdown Preview Mermaid Support" extension
3. **Local rendering**: Use any Markdown viewer with Mermaid support

## Related Documentation

- [AppGatewayTelemetryIntegrationGuide.md](../AppGatewayTelemetryIntegrationGuide.md) - Developer integration guide
- [AppGatewayTelemetryMarkers.md](../AppGatewayTelemetryMarkers.md) - Complete marker reference
- [AppGatewayTelemetry_Architecture.md](../AppGatewayTelemetry_Architecture.md) - Architecture overview

## Questions?

For implementation questions, refer to:
- Integration guide for step-by-step implementation
- Marker reference for predefined constants
- Badger and OttServices source code for real-world examples
