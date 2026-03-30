import asyncio
import json
from enum import StrEnum

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn


class ServerState(StrEnum):
    IDLE = "IDLE"
    MAPPING = "MAPPING"
    CONSTRUCTING_ROUTE = "CONSTRUCTING_ROUTE"


app = FastAPI()
connected_clients: set[WebSocket] = set()
state_lock = asyncio.Lock()
current_state = ServerState.IDLE
state_machine_task: asyncio.Task[None] | None = None
is_paused = False
pause_event = asyncio.Event()
pause_event.set()


async def send_json(websocket: WebSocket, payload: dict[str, str]) -> None:
    await websocket.send_text(json.dumps(payload))


async def broadcast_state() -> None:
    if not connected_clients:
        return

    payload = {"type": "state_update", "state": current_state, "paused": is_paused}
    disconnected_clients: list[WebSocket] = []

    for websocket in list(connected_clients):
        try:
            await send_json(websocket, payload)
        except Exception:
            disconnected_clients.append(websocket)

    for websocket in disconnected_clients:
        connected_clients.discard(websocket)


async def set_state(next_state: ServerState) -> None:
    global current_state, is_paused
    current_state = next_state
    if next_state == ServerState.IDLE:
        is_paused = False
        pause_event.set()
    print(f"Server state changed to {current_state}", flush=True)
    await broadcast_state()


async def wait_with_pause(duration_seconds: int) -> None:
    remaining = duration_seconds

    while remaining > 0:
        await pause_event.wait()
        await asyncio.sleep(1)
        if pause_event.is_set():
            remaining -= 1


async def run_state_machine() -> None:
    global state_machine_task

    try:
        await set_state(ServerState.MAPPING)
        await wait_with_pause(5)
        await set_state(ServerState.CONSTRUCTING_ROUTE)
        await wait_with_pause(5)
        await set_state(ServerState.IDLE)
    finally:
        async with state_lock:
            state_machine_task = None


async def maybe_start_mapping() -> bool:
    global state_machine_task

    async with state_lock:
        if current_state != ServerState.IDLE or state_machine_task is not None:
            return False

        state_machine_task = asyncio.create_task(run_state_machine())
        return True


async def set_pause(paused: bool) -> tuple[bool, str]:
    global is_paused

    async with state_lock:
        if current_state == ServerState.IDLE or state_machine_task is None:
            return False, "Pause/resume is only available while mapping is active."

        if paused and is_paused:
            return False, "State machine is already paused."

        if not paused and not is_paused:
            return False, "State machine is not paused."

        is_paused = paused
        if paused:
            pause_event.clear()
            print("State machine paused", flush=True)
        else:
            pause_event.set()
            print("State machine resumed", flush=True)

    await broadcast_state()
    return True, "paused" if paused else "resumed"


@app.get("/")
async def healthcheck() -> dict[str, str]:
    return {"status": "ok", "state": current_state, "paused": is_paused}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket) -> None:
    await websocket.accept()
    connected_clients.add(websocket)

    client = f"{websocket.client.host}:{websocket.client.port}" if websocket.client else "unknown"
    print(f"WebSocket connection established from {client}", flush=True)
    await send_json(websocket, {"type": "state_update", "state": current_state, "paused": is_paused})

    try:
        while True:
            raw_message = await websocket.receive_text()
            print(f"Received from client {client}: {raw_message}", flush=True)

            try:
                message = json.loads(raw_message)
            except json.JSONDecodeError:
                await send_json(
                    websocket,
                    {"type": "error", "message": "Invalid JSON payload."},
                )
                continue

            if message.get("type") != "command":
                await send_json(
                    websocket,
                    {"type": "error", "message": "Unsupported message type."},
                )
                continue

            command = message.get("command")

            if command == "start_mapping":
                started = await maybe_start_mapping()
                if started:
                    await send_json(
                        websocket,
                        {"type": "command_ack", "command": "start_mapping", "status": "accepted"},
                    )
                else:
                    await send_json(
                        websocket,
                        {
                            "type": "command_ack",
                            "command": "start_mapping",
                            "status": "rejected",
                            "reason": f"Server is currently in state {current_state}.",
                        },
                    )
                continue

            if command == "pause":
                success, reason = await set_pause(True)
                await send_json(
                    websocket,
                    {
                        "type": "command_ack",
                        "command": "pause",
                        "status": "accepted" if success else "rejected",
                        "reason": None if success else reason,
                    },
                )
                continue

            if command == "resume":
                success, reason = await set_pause(False)
                await send_json(
                    websocket,
                    {
                        "type": "command_ack",
                        "command": "resume",
                        "status": "accepted" if success else "rejected",
                        "reason": None if success else reason,
                    },
                )
                continue

            if command not in {"start_mapping", "pause", "resume"}:
                await send_json(
                    websocket,
                    {"type": "error", "message": "Unsupported command."},
                )
                continue
    except WebSocketDisconnect:
        connected_clients.discard(websocket)
        print(f"WebSocket disconnected: {client}", flush=True)


if __name__ == "__main__":
    uvicorn.run("websocket_server:app", host="127.0.0.1", port=9002, reload=False)
