# GStreamerPlayer Thunder Plugin

## Overview

GStreamerPlayer is a Thunder (WPEFramework) plugin that exposes a GStreamer-based media playback engine through JSON-RPC APIs. It enables remote control of media playback (start, stop, pause/resume, seek, volume control) and supports event-driven notifications for state changes and errors.

This plugin follows the standard Thunder architecture:

* Interface definition (`IGStreamerPlayer.h`)
* Proxy plugin (in-process, JSON-RPC bridge)
* Implementation (out-of-process, GStreamer engine)

---

## Architecture

### Layers

1. **Interface Layer**

   * Defines the contract exposed to clients
   * Located in: `entservices-apis/apis/GStreamerPlayer/IGStreamerPlayer.h`
   * Used to generate:

     * JSON-RPC bindings
     * COM-RPC proxy/stubs

2. **Plugin Layer (Proxy)**

   * Lives inside Thunder process
   * Handles JSON-RPC requests
   * Forwards calls to implementation via COM-RPC

3. **Implementation Layer**

   * Runs out-of-process (Local mode)
   * Owns GStreamer pipeline
   * Executes playback logic

---

## Features

* Start media playback from URI
* Stop playback
* Toggle play/pause
* Seek forward/backward
* Volume control (0.0 – 1.0)
* Playback state query
* Event notifications:

  * State changes
  * Errors

---

## Build Instructions

### Prerequisites

* RDK / Thunder build environment
* GStreamer 1.0
* `entservices-apis` integrated
* ThunderTools available

### Build Steps

```bash
bitbake -c cleanall entservices-apis
bitbake <your-image>
```

---

## Configuration

### Plugin Config File

Generated automatically from:

`GStreamerPlayer.conf.in`

Installed at:

```
/etc/WPEFramework/plugins/GStreamerPlayer.config
```

### Example Config

```json
{
  "autostart": false,
  "callsign": "org.rdk.GStreamerPlayer",
  "classname": "GStreamerPlayer",
  "locator": "libGStreamerPlayer.so",
  "configuration": {
    "root": {
      "mode": "Local",
      "locator": "libGStreamerPlayerImplementation.so"
    }
  }
}
```

---

## Activation

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":1,
  "method":"Controller.1.activate",
  "params":{"callsign":"org.rdk.GStreamerPlayer"}
}'
```

---

## JSON-RPC API Reference

### 1. Start Playback

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":1,
  "method":"org.rdk.GStreamerPlayer.1.start",
  "params":{
    "uri":"https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm"
  }
}'
```

---

### 2. Stop Playback

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":2,
  "method":"org.rdk.GStreamerPlayer.1.stop"
}'
```

---

### 3. Play / Pause Toggle

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":3,
  "method":"org.rdk.GStreamerPlayer.1.playPause"
}'
```

---

### 4. Seek

Forward 10 seconds:

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":4,
  "method":"org.rdk.GStreamerPlayer.1.seek",
  "params":{"offset":10}
}'
```

Backward 10 seconds:

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":5,
  "method":"org.rdk.GStreamerPlayer.1.seek",
  "params":{"offset":-10}
}'
```

---

### 5. Set Volume

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":6,
  "method":"org.rdk.GStreamerPlayer.1.setVolume",
  "params":{"volume":0.5}
}'
```

---

### 6. Get State

```bash
curl -X POST http://localhost:9998/jsonrpc \
-d '{
  "jsonrpc":"2.0",
  "id":7,
  "method":"org.rdk.GStreamerPlayer.1.getState"
}'
```

---

## Event Notifications

This plugin supports event-driven updates using Thunder events.

### Supported Events

* `onStateChanged`
* `onError`

---

### Event Subscription (WebSocket)

```bash
wscat -c ws://localhost:9998/jsonrpc
```

Register:

```json
{"jsonrpc":"2.0","id":1,"method":"org.rdk.GStreamerPlayer.1.register"}
```

Example Event:

```json
{
  "method": "org.rdk.GStreamerPlayer.1.onStateChanged",
  "params": {
    "state": "PLAYING"
  }
}
```

---

## Implementation Details

* Uses `playbin` GStreamer element
* Pipeline dynamically created per `Start()` call
* Supports:

  * State transitions (PLAYING, PAUSED, STOPPED)
  * Seeking via `gst_element_seek_simple`
  * Volume control via `g_object_set`

---

## File Structure

```
GStreamerPlayer/
├── GStreamerPlayer.cpp
├── GStreamerPlayer.h
├── GStreamerPlayerImplementation.cpp
├── GStreamerPlayerImplementation.h
├── Module.cpp
├── Module.h
├── CMakeLists.txt
├── GStreamerPlayer.conf.in
```

Interface repo:

```
entservices-apis/apis/GStreamerPlayer/
└── IGStreamerPlayer.h
```

---

## Yocto Integration

### entservices-apis.bb

```bitbake
SRC_URI += "file://IGStreamerPlayer.h"
```

---

### entservices-infra.bb

```bitbake
PACKAGECONFIG += "gstreamerplayer"

PACKAGECONFIG[gstreamerplayer] = "-DPLUGIN_GSTREAMERPLAYER=ON,-DPLUGIN_GSTREAMERPLAYER=OFF,entservices-apis,entservices-apis"

DEPENDS += "gstreamer1.0"
RDEPENDS:${PN} += "gstreamer1.0"
```

---

## Troubleshooting

### Plugin not visible

* Check `.config` file exists
* Verify `write_config()` in CMake

### JSON-RPC not working

* Ensure plugin is activated
* Verify method names (case-sensitive)

### Build failure (JsonGenerator)

* Ensure pointer params have `@in/@out`
* Check interface naming conventions

### No playback

* Verify GStreamer plugins installed
* Check sink availability

---

## Design Notes

* Out-of-process model improves stability
* JSON-RPC provides external control interface
* COM-RPC ensures efficient inter-process communication
* Event-driven architecture enables reactive UI integration

---

## Future Enhancements

* Multi-instance playback
* Playlist support
* Adaptive streaming (HLS/DASH)
* DRM integration
* Playback metrics (buffering, bitrate)

---

## License

Licensed under the Apache License, Version 2.0.
