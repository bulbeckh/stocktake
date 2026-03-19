"use client";

import { useEffect, useRef, useState } from "react";

const WS_HOST = "127.0.0.1";
const WS_PORT = 9002; // <- UPDATE THIS PORT
const MESSAGE_TEXT = "hello from client"; // <- UPDATE MESSAGE IF NEEDED

export default function Home() {
  const wsRef = useRef<WebSocket | null>(null);
  const sendIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const [status, setStatus] = useState("Disconnected");

  const clearSendInterval = () => {
    if (sendIntervalRef.current) {
      clearInterval(sendIntervalRef.current);
      sendIntervalRef.current = null;
    }
  };

  const connect = () => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      setStatus("Already connected");
      return;
    }

    const socket = new WebSocket(`ws://${WS_HOST}:${WS_PORT}`);
    wsRef.current = socket;

    setStatus("Connecting...");

    socket.onopen = () => {
      setStatus("Connected");

      clearSendInterval();
      sendIntervalRef.current = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) {
          socket.send(MESSAGE_TEXT);
        }
      }, 1000);
    };

    socket.onmessage = (event) => {
      console.log("Received from websocket:", event.data);
    };

    socket.onclose = () => {
      clearSendInterval();
      setStatus("Disconnected");
    };

    socket.onerror = () => {
      setStatus("Connection error");
    };
  };

  useEffect(() => {
    return () => {
      clearSendInterval();
      wsRef.current?.close();
    };
  }, []);

  return (
    <main>
      <button onClick={connect}>Connect WebSocket</button>
      <p>{status}</p>
    </main>
  );
}
