"use client";

import { useEffect, useRef, useState } from "react";

const WS_HOST = "127.0.0.1";
const WS_PORT = 9002;
const WS_PATH = "/ws";
const RETRY_INTERVAL_MS = 10_000;

type ConnectionState = "idle" | "connecting" | "connected" | "retrying";
type ServerState = "IDLE" | "MAPPING" | "CONSTRUCTING_ROUTE";
type CommandName = "start_mapping" | "pause" | "resume";

type ServerMessage =
  | { type: "state_update"; state: ServerState; paused: boolean }
  | { type: "command_ack"; command: CommandName; status: "accepted" | "rejected"; reason?: string | null }
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

  const sendCommand = (command: CommandName) => {
    if (wsRef.current?.readyState !== WebSocket.OPEN) {
      setCommandStatus("Cannot send command while websocket is disconnected");
      return;
    }

    const payload = {
      type: "command",
      command,
    };

    wsRef.current.send(JSON.stringify(payload));
    setCommandStatus(`${formatCommandLabel(command)} command sent`);
  };

  const startMapping = () => {
    sendCommand("start_mapping");
  };

  const togglePauseMapping = () => {
    sendCommand(isPaused ? "resume" : "pause");
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
            className="actionButton actionButtonSecondary"
            onClick={togglePauseMapping}
            disabled={!canTogglePause}
          >
            {isPaused ? "Resume" : "Pause"}
          </button>
        </div>
      </section>
    </main>
  );
}

function formatCommandLabel(command: CommandName): string {
  if (command === "start_mapping") {
    return "Start mapping";
  }

  return command.charAt(0).toUpperCase() + command.slice(1);
}
