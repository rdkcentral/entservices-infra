# GstreamerService Plugin

## Overview
The GstreamerService plugin provides a Thunder/WPEFramework interface for managing GStreamer multimedia pipelines. It allows applications to start, stop, and monitor GStreamer pipelines through a standardized JSON-RPC API.

## Features
- **Start Pipeline**: Launch a GStreamer pipeline with custom configuration
- **Stop Pipeline**: Stop the currently running pipeline
- **Get Pipeline Status**: Query the current state of the pipeline
- **Event Notifications**: Receive real-time notifications about pipeline state changes and errors
- **Out-of-Process Architecture**: Runs as a separate process for improved stability and isolation

## Plugin Configuration

### Build Options
The plugin can be enabled in the build by setting the following CMake option:
```cmake
-DPLUGIN_GSTREAMERSERVICE=ON
```

### Runtime Configuration
The plugin configuration is defined in `GstreamerService.conf.in`:

```python
autostart = "false"                           # Plugin starts on demand
precondition = ["Platform"]                   # Requires Platform subsystem
callsign = "org.rdk.GstreamerService"        # Plugin identifier
startuporder = ""                             # Startup sequence (if defined)

configuration = {
    "root": {
        "mode": "Local",                      # Out-of-process mode
        "locator": "libGstreamerServiceImplementation.so"
    }
}
```

## JSON-RPC API

### Methods

#### StartPipeline
Starts a GStreamer pipeline with the specified configuration.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "org.rdk.GstreamerService.1.startPipeline",
    "params": {
        "pipelineConfig": "videotestsrc ! autovideosink"
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": null
}
```

#### StopPipeline
Stops the currently running pipeline.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "org.rdk.GstreamerService.1.stopPipeline"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 2,
    "result": null
}
```

#### GetPipelineStatus
Retrieves the current status of the pipeline.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "org.rdk.GstreamerService.1.getPipelineStatus"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 3,
    "result": {
        "status": "PLAYING"
    }
}
```

Possible status values:
- `NULL`: Pipeline is not initialized or has been stopped
- `READY`: Pipeline is ready but not playing
- `PAUSED`: Pipeline is paused
- `PLAYING`: Pipeline is actively playing
- `VOID_PENDING`: State change is pending

### Events

#### onPipelineStateChanged
Triggered when the pipeline state changes.

**Event:**
```json
{
    "jsonrpc": "2.0",
    "method": "org.rdk.GstreamerService.1.onPipelineStateChanged",
    "params": {
        "state": "PLAYING"
    }
}
```

#### onError
Triggered when an error occurs in the pipeline.

**Event:**
```json
{
    "jsonrpc": "2.0",
    "method": "org.rdk.GstreamerService.1.onError",
    "params": {
        "errorMessage": "Error from source: Failed to open video device"
    }
}
```

## Usage Examples

### Example 1: Play a test video
```bash
# Start the pipeline
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "org.rdk.GstreamerService.1.startPipeline",
    "params": {
      "pipelineConfig": "videotestsrc ! autovideosink"
    }
  }'

# Get status
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "org.rdk.GstreamerService.1.getPipelineStatus"
  }'

# Stop the pipeline
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "org.rdk.GstreamerService.1.stopPipeline"
  }'
```

### Example 2: Play an audio file
```bash
curl -X POST http://localhost:9998/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "org.rdk.GstreamerService.1.startPipeline",
    "params": {
      "pipelineConfig": "filesrc location=/path/to/audio.mp3 ! decodebin ! audioconvert ! autoaudiosink"
    }
  }'
```

## Dependencies
- GStreamer 1.0 development libraries
- Thunder/WPEFramework
- entservices-apis (IGstreamerService interface)

## Building

### Prerequisites
```bash
# Install GStreamer development packages
sudo apt-get install libgstreamer1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good
```

### Build Configuration
```bash
cmake -B build -S . \
  -DPLUGIN_GSTREAMERSERVICE=ON \
  -DPLUGIN_GSTREAMERSERVICE_MODE=Local \
  -DPLUGIN_GSTREAMERSERVICE_AUTOSTART=false

cmake --build build
```

## Testing

After building and deploying the plugin:

1. Ensure the Thunder framework is running
2. Activate the plugin:
   ```bash
   curl -X POST http://localhost:9998/jsonrpc \
     -H "Content-Type: application/json" \
     -d '{
       "jsonrpc": "2.0",
       "id": 1,
       "method": "Controller.1.activate",
       "params": {
         "callsign": "org.rdk.GstreamerService"
       }
     }'
   ```
3. Use the API examples above to test functionality

## Troubleshooting

### Common Issues

**Pipeline fails to start:**
- Verify GStreamer is installed correctly: `gst-launch-1.0 --version`
- Check pipeline syntax: `gst-launch-1.0 <pipeline-config>`
- Review system logs for detailed error messages

**Plugin not loading:**
- Ensure GStreamer development libraries are installed
- Verify the plugin is enabled in the build: `PLUGIN_GSTREAMERSERVICE=ON`
- Check Thunder logs: `/opt/logs/Thunder.log`

**Out-of-process crashes:**
- Check system resources (memory, CPU)
- Review crash dumps if available
- Verify GStreamer plugin compatibility

## License
Copyright 2024 RDK Management

Licensed under the Apache License, Version 2.0
