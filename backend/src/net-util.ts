// net-util.ts — find the machine's LAN IPv4 so the device (on WiFi) can reach the
// backend. `localhost` only works for processes on this machine, not the CoreS3.

import { networkInterfaces } from 'node:os';

export function lanIp(): string {
  const ifaces = networkInterfaces();
  for (const addrs of Object.values(ifaces)) {
    for (const a of addrs ?? []) {
      if (a.family === 'IPv4' && !a.internal) return a.address;
    }
  }
  return 'localhost';
}
