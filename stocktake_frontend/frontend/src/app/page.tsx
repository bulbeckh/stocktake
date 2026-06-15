"use client";

import { useEffect, useRef, useState } from "react";

const WS_HOST = "127.0.0.1";
const WS_PORT = 9002;
const WS_PATH = "/ws";
const RETRY_INTERVAL_MS = 10_000;

type ConnectionState = "idle" | "connecting" | "connected" | "retrying";
type ServerState = "IDLE" | "MAPPING" | "CONSTRUCTING_ROUTE";
type CommandName =
  | "start_mapping"
  | "start_stocktake"
  | "pause"
  | "resume"
  | "list_maps"
  | "select_map";
type MapInfo = {
  id: string;
  path: string;
  has_graph: boolean;
  has_map: boolean;
  selected: boolean;
};
type RfidTagObservation = {
  uid: string;
  rssi: number;
};
type SeenRfidTag = RfidTagObservation & {
  observations: number;
  flashCount: number;
};

type ServerMessage =
  | { type: "state_update"; state: ServerState; paused: boolean }
  | { type: "command_ack"; command: CommandName; status: "accepted" | "rejected"; reason?: string | null }
  | { type: "maps_list"; maps: MapInfo[] }
  | { type: "rfid_scan_observation"; waypoint_node_id: number; tags: RfidTagObservation[] }
  | { type: "error"; message: string };

export default function Home() {
  const wsRef = useRef<WebSocket | null>(null);
  const retryTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const connectionStateRef = useRef<ConnectionState>("idle");
  const shouldReconnectRef = useRef(true);
  const [connectionState, setConnectionState] = useState<ConnectionState>("idle");
  const [connectionStatus, setConnectionStatus] = useState("Waiting to connect");
  const [serverState, setServerState] = useState<ServerState>("IDLE");
  const [isPaused, setIsPaused] = useState(false);
  const [commandStatus, setCommandStatus] = useState("Waiting for server");
  const [availableMaps, setAvailableMaps] = useState<MapInfo[]>([]);
  const [selectedMapId, setSelectedMapId] = useState("no map");
  const [seenTags, setSeenTags] = useState<SeenRfidTag[]>([]);
  const websocketUrl = `ws://${WS_HOST}:${WS_PORT}${WS_PATH}`;
  const isSocketOpen = wsRef.current?.readyState === WebSocket.OPEN;

  const updateConnectionState = (nextState: ConnectionState) => {
    connectionStateRef.current = nextState;
    setConnectionState(nextState);
  };

  const clearRetryTimeout = () => {
    if (retryTimeoutRef.current) {
      clearTimeout(retryTimeoutRef.current);
      retryTimeoutRef.current = null;
    }
  };

  const scheduleReconnect = () => {
    clearRetryTimeout();
    retryTimeoutRef.current = setTimeout(() => {
      connect();
    }, RETRY_INTERVAL_MS);
  };

  const connect = () => {
    const existingSocket = wsRef.current;
    if (
      existingSocket &&
      (existingSocket.readyState === WebSocket.OPEN ||
        existingSocket.readyState === WebSocket.CONNECTING)
    ) {
      return;
    }

    clearRetryTimeout();
    updateConnectionState("connecting");
    setConnectionStatus(`Attempting to connect to ${websocketUrl}`);

    const socket = new WebSocket(websocketUrl);
    wsRef.current = socket;

    socket.onopen = () => {
      if (wsRef.current !== socket) {
        return;
      }

      console.log(`WebSocket connected to ${websocketUrl}`);
      updateConnectionState("connected");
      setConnectionStatus(`Connected to ${websocketUrl}`);
      setCommandStatus("Connected to server");
      sendCommand("list_maps", socket);
    };

    socket.onmessage = (event) => {
      if (wsRef.current !== socket) {
        return;
      }

      console.log("Received from websocket:", event.data);

      try {
        const message = JSON.parse(event.data) as ServerMessage;

        if (message.type === "state_update") {
          setServerState(message.state);
          setIsPaused(message.paused);
          setCommandStatus(
            message.paused
              ? `Server state updated to ${message.state} (paused)`
              : `Server state updated to ${message.state}`,
          );
          return;
        }

        if (message.type === "command_ack") {
          setCommandStatus(
            message.status === "accepted"
              ? `${formatCommandLabel(message.command)} accepted by server`
              : message.reason ?? `${formatCommandLabel(message.command)} rejected by server`,
          );
          return;
        }

        if (message.type === "maps_list") {
          const selectedMap = message.maps.find((map) => map.selected);

          setAvailableMaps(message.maps);
          setSelectedMapId(selectedMap?.id ?? "no map");
          setCommandStatus(`${message.maps.length} map${message.maps.length === 1 ? "" : "s"} available`);
          return;
        }

        if (message.type === "rfid_scan_observation") {
          setSeenTags((currentTags) => {
            let hasChanges = false;
            const nextTags = [...currentTags];

            for (const observedTag of message.tags) {
              const existingTagIndex = nextTags.findIndex((tag) => tag.uid === observedTag.uid);

              if (existingTagIndex === -1) {
                nextTags.push({
                  ...observedTag,
                  observations: 1,
                  flashCount: 0,
                });
                hasChanges = true;
                continue;
              }

              const existingTag = nextTags[existingTagIndex];
              nextTags[existingTagIndex] = {
                ...existingTag,
                rssi: observedTag.rssi,
                observations: existingTag.observations + 1,
                flashCount: existingTag.flashCount + 1,
              };
              hasChanges = true;
            }

            return hasChanges ? nextTags : currentTags;
          });
          return;
        }

        if (message.type === "error") {
          setCommandStatus(message.message);
        }
      } catch (error) {
        console.error("Failed to parse websocket message:", error);
      }
    };

    socket.onclose = () => {
      if (wsRef.current !== socket) {
        return;
      }

      if (!shouldReconnectRef.current) {
        wsRef.current = null;
        return;
      }

      const wasConnected = connectionStateRef.current === "connected";
      wsRef.current = null;
      updateConnectionState("retrying");
      setConnectionStatus(
        wasConnected
          ? `Connection lost. Retrying in ${RETRY_INTERVAL_MS / 1000} seconds...`
          : `Connection failed. Retrying in ${RETRY_INTERVAL_MS / 1000} seconds...`,
      );
      setCommandStatus("Disconnected from server");
      console.log(
        `WebSocket closed. Retrying connection to ${websocketUrl} in ${
          RETRY_INTERVAL_MS / 1000
        } seconds.`,
      );
      scheduleReconnect();
    };

    socket.onerror = () => {
      if (wsRef.current !== socket) {
        return;
      }

      console.error(`WebSocket connection failed for ${websocketUrl}`);
    };
  };

  useEffect(() => {
    shouldReconnectRef.current = true;
    connect();

    return () => {
      shouldReconnectRef.current = false;
      clearRetryTimeout();
      wsRef.current?.close();
      wsRef.current = null;
    };
  }, []);

  const sendCommand = (
    command: CommandName,
    socket = wsRef.current,
    extraPayload: Record<string, string> = {},
  ) => {
    if (socket?.readyState !== WebSocket.OPEN) {
      setCommandStatus("Cannot send command while websocket is disconnected");
      return;
    }

    const payload = {
      type: "command",
      command,
      ...extraPayload,
    };

    socket.send(JSON.stringify(payload));
    setCommandStatus(`${formatCommandLabel(command)} command sent`);
  };

  const startMapping = () => {
    sendCommand("start_mapping");
  };

  const startStocktake = () => {
    sendCommand("start_stocktake");
  };

  const togglePauseMapping = () => {
    sendCommand(isPaused ? "resume" : "pause");
  };

  const selectMap = (mapId: string) => {
    setSelectedMapId(mapId || "no map");

    if (!mapId) {
      return;
    }

    sendCommand("select_map", wsRef.current, { map_id: mapId });
  };

  const indicatorClassName =
    connectionState === "connected"
      ? "statusIndicator statusConnected"
      : connectionState === "connecting"
        ? "statusIndicator statusConnecting"
        : "statusIndicator statusRetrying";

  const stateBadgeClassName =
    serverState === "IDLE"
      ? "stateBadge stateIdle"
      : serverState === "MAPPING"
        ? "stateBadge stateMapping"
        : "stateBadge stateConstructing";

  const isStartMappingDisabled =
    !isSocketOpen || serverState !== "IDLE";
  const canTogglePause = isSocketOpen && serverState !== "IDLE";

  return (
    <main className="pageShell">
      <section className="dashboardCard">
        <div className="statusCard">
          <div className={indicatorClassName} aria-hidden="true" />
          <div>
            <p className="statusLabel">
              {connectionState === "connected" ? "WebSocket connected" : "WebSocket status"}
            </p>
            <p className="statusText">{connectionStatus}</p>
          </div>
        </div>

        <section className="stateSection">
          <div>
            <p className="statusLabel">Server state</p>
            <div className={stateBadgeClassName}>{serverState}</div>
          </div>
          <p className="statusText">
            {serverState === "IDLE" ? "No active timer" : isPaused ? "Timer paused" : "Timer running"}
          </p>
          <p className="statusText">{commandStatus}</p>
        </section>

        <section className="stateSection">
          <div>
            <p className="statusLabel">Selected map</p>
            <div className="mapLabel">{selectedMapId}</div>
          </div>
          <label className="mapSelectLabel" htmlFor="map-select">
            Map ID
          </label>
          <select
            id="map-select"
            className="mapSelect"
            value={selectedMapId === "no map" ? "" : selectedMapId}
            onChange={(event) => selectMap(event.target.value)}
            disabled={!isSocketOpen || availableMaps.length === 0}
          >
            <option value="">No map</option>
            {availableMaps.map((map) => (
              <option key={map.id} value={map.id}>
                {map.id}
              </option>
            ))}
          </select>
        </section>

        <div className="actionRow">
          <button
            type="button"
            className="actionButton"
            onClick={startMapping}
            disabled={isStartMappingDisabled}
          >
            Start Mapping
          </button>
          <button
            type="button"
            className="actionButton"
            onClick={startStocktake}
            disabled={isStartMappingDisabled}
          >
            Start Stocktake
          </button>
          <button
            type="button"
            className="actionButton actionButtonSecondary"
            onClick={togglePauseMapping}
            disabled={!canTogglePause}
          >
            {isPaused ? "Resume" : "Pause"}
          </button>
        </div>

        <section className="stateSection">
          <div>
            <p className="statusLabel">RFID observations</p>
            <p className="statusText">{seenTags.length} unique tag{seenTags.length === 1 ? "" : "s"}</p>
          </div>
          <div className="tagTable" role="table" aria-label="RFID tag observations">
            <div className="tagTableHeader" role="row">
              <div role="columnheader">UID</div>
              <div role="columnheader">RSSI</div>
              <div role="columnheader">Observations</div>
            </div>
            <div className="tagTableBody" role="rowgroup">
              {seenTags.length === 0 ? (
                <div className="tagTableEmpty" role="row">
                  <div role="cell">No tags observed</div>
                </div>
              ) : (
                seenTags.map((tag) => (
                  <div
                    className={`tagTableRow${tag.flashCount > 0 ? " tagTableRowFlash" : ""}`}
                    role="row"
                    key={`${tag.uid}-${tag.flashCount}`}
                  >
                    <div className="tagUid" role="cell">{tag.uid}</div>
                    <div role="cell">{tag.rssi}</div>
                    <div role="cell">{tag.observations}</div>
                  </div>
                ))
              )}
            </div>
          </div>
        </section>
      </section>
    </main>
  );
}

function formatCommandLabel(command: CommandName): string {
  if (command === "start_mapping") {
    return "Start mapping";
  }

  if (command === "start_stocktake") {
    return "Start stocktake";
  }

  if (command === "list_maps") {
    return "List maps";
  }

  if (command === "select_map") {
    return "Select map";
  }

  return command.charAt(0).toUpperCase() + command.slice(1);
}
