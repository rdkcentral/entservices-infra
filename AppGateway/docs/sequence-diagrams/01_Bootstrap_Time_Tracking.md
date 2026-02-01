# Scenario 1: Bootstrap Time Tracking

## Overview

This sequence diagram illustrates how App Gateway measures and reports the total time taken to initialize all plugins during system bootstrap.

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Thunder as Thunder Framework
    participant AppGw as AppGateway Plugin
    participant Telemetry as AppGatewayTelemetry
    participant T2 as T2 Telemetry Server
    
    Note over Thunder,T2: System Initialization Phase
    
    Thunder->>AppGw: Initialize()
    activate AppGw
    
    Note over AppGw: Start bootstrap timer
    AppGw->>AppGw: bootstrapStart = now()
    
    AppGw->>Telemetry: getInstance()
    Telemetry-->>AppGw: telemetry instance
    
    AppGw->>Telemetry: Initialize(service)
    activate Telemetry
    Note over Telemetry: Initialize timer<br/>Start periodic reporting
    Telemetry-->>AppGw: Core::ERROR_NONE
    deactivate Telemetry
    
    Note over AppGw: Initialize all plugins:<br/>- Badger<br/>- OttServices<br/>- FbAdvertising<br/>- etc.
    
    AppGw->>AppGw: Initialize child plugins
    Note over AppGw: bootstrapEnd = now()<br/>Calculate duration
    
    AppGw->>AppGw: Calculate duration_ms and plugins_loaded
    
    AppGw->>Telemetry: RecordBootstrapTime(duration_ms, plugins_loaded)
    activate Telemetry
    
    Note over Telemetry: Store bootstrap data<br/>Will be reported immediately<br/>or on next timer interval
    
    Telemetry->>Telemetry: Prepare T2 payload
    Note over Telemetry: Marker: "AppGwBootstrapTime_split"<br/>Payload: {"duration_ms": 2500, "plugins_loaded": 8}
    
    Telemetry->>T2: t2_event_s("AppGwBootstrapTime_split", payload)
    T2-->>Telemetry: Success
    
    Telemetry-->>AppGw: Core::ERROR_NONE
    deactivate Telemetry
    
    AppGw-->>Thunder: SUCCESS
    deactivate AppGw
```

## Key Components

| Component | Responsibility |
|-----------|---------------|
| **Thunder Framework** | Initiates plugin loading sequence |
| **AppGateway Plugin** | Measures bootstrap time across all child plugins |
| **AppGatewayTelemetry** | Aggregates and reports bootstrap metrics to T2 |
| **T2 Telemetry Server** | Receives and stores telemetry data |

## Data Flow

1. **Bootstrap Start**: AppGateway records timestamp when `Initialize()` is called
2. **Plugin Initialization**: All child plugins (Badger, OttServices, etc.) are loaded
3. **Bootstrap End**: After all plugins load, calculate total duration
4. **Telemetry Recording**: Report bootstrap time and plugin count to telemetry
5. **T2 Reporting**: Telemetry formats and sends data to T2 server

## T2 Marker

**Marker Name:** `AppGwBootstrapTime_split`

**Payload Format (JSON):**
```json
{
  "duration_ms": 2500,
  "plugins_loaded": 8
}
```

**Payload Format (COMPACT):**
```
2500,8
```

## Configuration

- **Reporting**: Immediate upon bootstrap completion
- **Frequency**: Once per system start
- **Format**: Configurable via `SetTelemetryFormat()` (JSON or COMPACT)

## Notes

- Bootstrap time is critical for measuring system startup performance
- Helps identify slow plugin initialization
- Reported only once during AppGateway initialization
- Independent of periodic telemetry reporting interval
