// routes/device.ts — HTTP control for the USB-connected CoreS3 (WiFi provisioning).
// The admin frontend also gets live updates over /ws/admin; these endpoints are the
// request/trigger side.

import { Router } from 'express';
import type { DeviceSerial } from '../serial';

export function createDeviceRouter(serial: DeviceSerial): Router {
  const router = Router();

  // GET /api/device/status — is the CoreS3 plugged in? current wifi state?
  router.get('/status', (_req, res) => {
    res.json(serial.getStatus());
  });

  // POST /api/device/scan — ask the device to scan for wifi networks.
  // Results arrive asynchronously as a wifi_list message on /ws/admin.
  router.post('/scan', (_req, res) => {
    res.json({ ok: serial.scanWifi() });
  });

  // POST /api/device/wifi {ssid,password} — push credentials to the device.
  router.post('/wifi', (req, res) => {
    const { ssid, password } = (req.body ?? {}) as { ssid?: string; password?: string };
    if (!ssid) {
      res.status(400).json({ error: 'ssid required' });
      return;
    }
    res.json({ ok: serial.setWifi(String(ssid), String(password ?? '')) });
  });

  return router;
}
