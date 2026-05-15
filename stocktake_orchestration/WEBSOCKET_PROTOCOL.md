# WebSocket Interface

This document describes the current mock server interface implemented by
[`websocket_server.py`](./websocket_server.py). It is intended to be a clean
reference for a future reimplementation in C++ with Boost.Asio.

## Endpoint

- Host: `127.0.0.1`
- Port: `9002`
- WebSocket path: `/ws`
- Full URL: `ws://127.0.0.1:9002/ws`

There is also a simple HTTP healthcheck:

- `GET /`

Example response:

```json
{
  "status": "ok",
  "state": "IDLE",
  "paused": false
}
```

## Transport Rules

- All WebSocket application messages are JSON text frames.
- The server accepts multiple concurrent clients.
- On every new WebSocket connection, the server immediately sends the current
  state to that client.
- Whenever the server state changes, the server broadcasts the new state to all
  connected clients.
- Whenever the paused flag changes, the server also broadcasts a state update to
  all connected clients.

## Server State Model

The server has two pieces of state:

- `state`: one of `IDLE`, `MAPPING`, `CONSTRUCTING_ROUTE`
- `paused`: boolean

### Meaning

- `IDLE`
  - No active workflow is running.
  - `paused` must be `false`.
- `MAPPING`
  - Active workflow phase 1.
- `CONSTRUCTING_ROUTE`
  - Active workflow phase 2.

### State Machine

Normal flow:

1. Initial state is `IDLE`, `paused=false`
2. Client sends `start_mapping`
3. Server transitions to `MAPPING`, `paused=false`
4. The node triggers its `IDLE -> MAPPING` transition hook for ROS2-side work
5. When mapping completes, the node explicitly transitions to `CONSTRUCTING_ROUTE`, `paused=false`
6. The node triggers its `MAPPING -> CONSTRUCTING_ROUTE` transition hook for ROS2-side work
7. When route construction completes, the node explicitly transitions to `IDLE`, `paused=false`

Pause behavior:

- `pause` is only valid while the workflow is active, meaning state is not `IDLE`.
- While paused, external completion callbacks should not advance the active phase.
- `resume` re-enables phase progression after a pause.
- While paused, external completion callbacks should not advance the state machine.
- Pausing does not change `state`; it only changes `paused`.

## Client-to-Server Messages

All client requests use this envelope:

```json
{
  "type": "command",
  "command": "<command-name>"
}
```

### Supported commands

#### `start_mapping`

Valid only when:

- `state == "IDLE"`

Example:

```json
{
  "type": "command",
  "command": "start_mapping"
}
```

#### `pause`

Valid only when:

- `state != "IDLE"`
- `paused == false`

Example:

```json
{
  "type": "command",
  "command": "pause"
}
```

#### `resume`

Valid only when:

- `state != "IDLE"`
- `paused == true`

Example:

```json
{
  "type": "command",
  "command": "resume"
}
```

## Server-to-Client Messages

### State update

Sent:

- immediately after a client connects
- whenever `state` changes
- whenever `paused` changes

Schema:

```json
{
  "type": "state_update",
  "state": "IDLE",
  "paused": false
}
```

Example while paused in mapping:

```json
{
  "type": "state_update",
  "state": "MAPPING",
  "paused": true
}
```

### Command acknowledgement

Sent after each recognized command.

Schema:

```json
{
  "type": "command_ack",
  "command": "start_mapping",
  "status": "accepted"
}
```

Rejected command example:

```json
{
  "type": "command_ack",
  "command": "pause",
  "status": "rejected",
  "reason": "Pause/resume is only available while mapping is active."
}
```

Notes:

- `command` is one of `start_mapping`, `pause`, `resume`
- `status` is one of `accepted`, `rejected`
- `reason` is present on rejection

### Error

Sent when the payload is malformed or unsupported.

Schema:

```json
{
  "type": "error",
  "message": "Invalid JSON payload."
}
```

Other current error messages include:

- `Unsupported message type.`
- `Unsupported command.`

## Behavioral Notes For A C++ Reimplementation

- Broadcast the latest state to every connected client, not just the client that
  triggered the command.
- Send an initial `state_update` immediately after accepting a connection.
- Only one workflow should run at a time.
- The C++ implementation no longer needs internal phase timers; later transitions
  can be driven by ROS2 callbacks, service responses, or other orchestration logic.
- `start_mapping` should be rejected if the server is not currently `IDLE`.
- `pause` and `resume` should gate whether external completion events are allowed to advance the workflow.
- Reset `paused` to `false` when transitioning back to `IDLE`.
- Logging to stdout is currently expected for:
  - new WebSocket connection
  - incoming client messages
  - state changes
  - pause/resume events
  - disconnects
