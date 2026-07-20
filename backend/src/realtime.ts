// realtime.ts — WebSocket channels.
//   /ws/device : the CoreS3. Receives session snapshots + notifications; sends
//                hello / confirm_done.
//   /ws/admin  : the admin frontend. Receives device USB status + wifi scan results.
//
// Two noServer WebSocketServers routed by URL path off the shared HTTP server.

import { WebSocketServer, WebSocket, type RawData } from 'ws';
import type { Server } from 'node:http';
import type { Socket } from 'node:net';
import type { SessionStore } from './types';
import { toDeviceSession, toDeviceUsage } from './serialize';

type MsgHandler = (msg: any, ws: WebSocket) => void;

function safeParse(data: RawData): any | null {
  try {
    return JSON.parse(data.toString());
  } catch {
    return null;
  }
}

export interface DeviceNotification {
  kind: 'task_done' | 'permission_request';
  sessionId: string;
  title?: string;
  tool?: string;
  command?: string;
  description?: string;
}

export class Realtime {
  private wssDevice = new WebSocketServer({ noServer: true });
  private wssAdmin = new WebSocketServer({ noServer: true });
  private deviceHandlers: MsgHandler[] = [];
  private adminHandlers: MsgHandler[] = [];

  constructor(private store: SessionStore) {}

  attach(server: Server): void {
    server.on('upgrade', (req, socket, head) => {
      if (req.url === '/ws/device') {
        this.wssDevice.handleUpgrade(req, socket as Socket, head, (ws) => this.onDeviceConnect(ws));
      } else if (req.url === '/ws/admin') {
        this.wssAdmin.handleUpgrade(req, socket as Socket, head, (ws) => this.onAdminConnect(ws));
      } else {
        socket.destroy();
      }
    });
  }

  onDeviceMessage(h: MsgHandler): void {
    this.deviceHandlers.push(h);
  }
  onAdminMessage(h: MsgHandler): void {
    this.adminHandlers.push(h);
  }

  // --- device channel ---

  private onDeviceConnect(ws: WebSocket): void {
    ws.on('message', (data) => {
      const msg = safeParse(data);
      if (msg) this.deviceHandlers.forEach((h) => h(msg, ws));
    });
    this.send(ws, this.snapshotPayload());
  }

  private snapshotPayload() {
    return {
      type: 'snapshot',
      sessions: this.store.listSessions('codex').map(toDeviceSession),
      usage: { codex: toDeviceUsage('codex', this.store.latestUsage('codex')) },
    };
  }

  /** Push the full session snapshot to every connected device. */
  broadcastSnapshot(): void {
    this.broadcast(this.wssDevice, this.snapshotPayload());
  }

  notifyDevice(n: DeviceNotification): void {
    this.broadcast(this.wssDevice, { type: 'notification', ...n });
  }

  // --- admin channel ---

  private onAdminConnect(ws: WebSocket): void {
    ws.on('message', (data) => {
      const msg = safeParse(data);
      if (msg) this.adminHandlers.forEach((h) => h(msg, ws));
    });
  }

  broadcastAdmin(obj: unknown): void {
    this.broadcast(this.wssAdmin, obj);
  }

  // --- helpers ---

  private send(ws: WebSocket, obj: unknown): void {
    if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
  }

  private broadcast(wss: WebSocketServer, obj: unknown): void {
    const data = JSON.stringify(obj);
    for (const ws of wss.clients) {
      if (ws.readyState === WebSocket.OPEN) ws.send(data);
    }
  }
}
