// serial.ts — USB-serial bridge to the CoreS3 for WiFi provisioning.
//
// Line-delimited JSON both ways.
//   backend -> device : {type:'ping'} | {type:'scan_wifi'} | {type:'set_wifi',ssid,password,backendWs}
//   device  -> backend: {type:'pong',device:'cores3',fw,wifi:{connected,ssid,ip}}
//                       {type:'wifi_list',networks:[{ssid,rssi,secure}]}
//                       {type:'wifi_result',ok,ip}
//
// The device is auto-detected: candidate USB ports are opened and pinged; the one
// that answers `pong` is kept. Disconnects trigger re-detection.

import { SerialPort } from 'serialport';
import { ReadlineParser } from '@serialport/parser-readline';

const BAUD = 115200;
const PING_TIMEOUT_MS = 1500;
const POLL_MS = 2500;

export interface WifiNetwork {
  ssid: string;
  rssi: number;
  secure: boolean;
}

export interface DeviceStatus {
  connected: boolean;
  portPath: string | null;
  fw?: string;
  wifi?: { connected: boolean; ssid?: string; ip?: string } | null;
}

type EventOut =
  | ({ type: 'device_status' } & DeviceStatus)
  | { type: 'wifi_list'; networks: WifiNetwork[] }
  | { type: 'wifi_result'; ok: boolean; ip?: string };

// Known USB vendor IDs for ESP32 boards / their serial bridges, so we don't open
// unrelated serial devices: Espressif native-USB, WCH CH34x, SiLabs CP210x, FTDI.
const ESP_VENDORS = new Set(['303a', '1a86', '10c4', '0403']);

function isCandidate(port: { path: string; vendorId?: string }): boolean {
  const vid = (port.vendorId ?? '').toLowerCase();
  if (ESP_VENDORS.has(vid)) return true;
  // Fallback for boards that don't report a vendor id: match the usual macOS names.
  return !port.vendorId && /usbmodem|usbserial|wchusb|slab|cu\.usb/i.test(port.path);
}

export class DeviceSerial {
  private port: SerialPort | null = null;
  private status: DeviceStatus = { connected: false, portPath: null, wifi: null };
  private pollTimer: NodeJS.Timeout | null = null;

  constructor(
    private opts: { onEvent: (e: EventOut) => void; backendWsUrl: string },
  ) {}

  start(): void {
    if (this.pollTimer) return;
    const tick = () => {
      if (!this.port?.isOpen) this.findAndOpen().catch(() => {});
    };
    tick();
    this.pollTimer = setInterval(tick, POLL_MS);
  }

  stop(): void {
    if (this.pollTimer) clearInterval(this.pollTimer);
    this.pollTimer = null;
    this.port?.close(() => {});
  }

  getStatus(): DeviceStatus {
    return this.status;
  }

  scanWifi(): boolean {
    return this.write({ type: 'scan_wifi' });
  }

  setWifi(ssid: string, password: string): boolean {
    return this.write({ type: 'set_wifi', ssid, password, backendWs: this.opts.backendWsUrl });
  }

  // --- detection / connection ---------------------------------------------

  private async findAndOpen(): Promise<void> {
    let ports;
    try {
      ports = await SerialPort.list();
    } catch {
      return;
    }
    for (const p of ports) {
      if (!isCandidate(p)) continue;
      const ok = await this.tryPort(p.path);
      if (ok) return;
    }
  }

  private tryPort(path: string): Promise<boolean> {
    return new Promise((resolve) => {
      let settled = false;
      const done = (ok: boolean) => {
        if (settled) return;
        settled = true;
        resolve(ok);
      };

      const port = new SerialPort({ path, baudRate: BAUD }, (err) => {
        if (err) done(false);
      });
      const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

      const timeout = setTimeout(() => {
        // No pong: not our device (or busy). Close and move on.
        port.close(() => {});
        done(false);
      }, PING_TIMEOUT_MS);

      parser.on('data', (line: string) => {
        const msg = this.parseLine(line);
        if (!msg) return;
        if (msg.type === 'pong' && !this.port) {
          clearTimeout(timeout);
          this.adopt(port, path, msg);
          done(true);
        } else if (this.port === port) {
          this.handleMessage(msg);
        }
      });

      port.on('error', () => {
        clearTimeout(timeout);
        done(false);
      });
      port.on('close', () => {
        if (this.port === port) this.onDisconnect();
      });

      // Give the board a moment (opening may reset it), then ping.
      port.on('open', () => setTimeout(() => port.write(JSON.stringify({ type: 'ping' }) + '\n'), 400));
    });
  }

  private adopt(port: SerialPort, path: string, pong: any): void {
    this.port = port;
    this.status = {
      connected: true,
      portPath: path,
      fw: typeof pong.fw === 'string' ? pong.fw : undefined,
      wifi: pong.wifi ?? null,
    };
    this.emitStatus();
  }

  private onDisconnect(): void {
    this.port = null;
    this.status = { connected: false, portPath: null, wifi: null };
    this.emitStatus();
  }

  // --- messaging -----------------------------------------------------------

  private write(obj: unknown): boolean {
    if (!this.port?.isOpen) return false;
    this.port.write(JSON.stringify(obj) + '\n');
    return true;
  }

  private parseLine(line: string): any | null {
    const t = line.trim();
    if (!t || t[0] !== '{') return null; // ignore boot log noise
    try {
      return JSON.parse(t);
    } catch {
      return null;
    }
  }

  private handleMessage(msg: any): void {
    switch (msg.type) {
      case 'pong':
        this.status = { connected: true, portPath: this.status.portPath, fw: msg.fw, wifi: msg.wifi ?? null };
        this.emitStatus();
        break;
      case 'wifi_list':
        this.opts.onEvent({ type: 'wifi_list', networks: Array.isArray(msg.networks) ? msg.networks : [] });
        break;
      case 'wifi_result':
        if (msg.wifi || msg.ip) this.status.wifi = { connected: !!msg.ok, ssid: msg.ssid, ip: msg.ip };
        this.emitStatus();
        this.opts.onEvent({ type: 'wifi_result', ok: !!msg.ok, ip: msg.ip });
        break;
      default:
        break;
    }
  }

  private emitStatus(): void {
    this.opts.onEvent({ type: 'device_status', ...this.status });
  }
}
