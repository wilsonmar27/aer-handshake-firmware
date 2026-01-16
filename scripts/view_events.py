#!/usr/bin/env python3
import argparse
import struct
import sys
import time
import pygame
import serial
from serial.tools import list_ports


MAGIC = b"AERS"
HDR_LEN = 8  # magic(4) + ver(1) + type(1) + len(2)

# hal_stream_type_t
HAL_STREAM_LOG_TEXT  = 1
HAL_STREAM_EVENT_BIN = 2
HAL_STREAM_RAW_BIN   = 3
HAL_STREAM_MARKER    = 4

# usb_stream_event_rec_type_t
USB_EVT_REC_V1_NOTS  = 1
USB_EVT_REC_V1_TICKS = 2

USB_EVT_FLAG_ON = 0x01


def auto_find_port() -> str | None:
    ports = list(list_ports.comports())
    if not ports:
        return None

    for p in ports:
        text = f"{p.description} {p.manufacturer} {p.product}".lower()
        if "pico" in text or "raspberry" in text or "rp2350" in text:
            return p.device

    for p in ports:
        if "acm" in p.device.lower() or "usb" in (p.description or "").lower():
            return p.device

    return ports[0].device


class FramedStreamReader:
    def __init__(self, ser: serial.Serial):
        self.ser = ser
        self.buf = bytearray()

    def _read_some(self):
        data = self.ser.read(self.ser.in_waiting or 1)
        if data:
            self.buf.extend(data)

    def read_packet(self):
        """Return (ver, ptype, payload) for next packet; blocks until one parsed."""
        while True:
            self._read_some()

            idx = self.buf.find(MAGIC)
            if idx < 0:
                if len(self.buf) > 3:
                    self.buf = self.buf[-3:]
                continue

            if idx > 0:
                del self.buf[:idx]

            if len(self.buf) < HDR_LEN:
                continue

            ver = self.buf[4]
            ptype = self.buf[5]
            plen = struct.unpack_from("<H", self.buf, 6)[0]
            total = HDR_LEN + plen
            if len(self.buf) < total:
                continue

            payload = bytes(self.buf[HDR_LEN:total])
            del self.buf[:total]
            return ver, ptype, payload


def extract_on_events(payload: bytes):
    """
    Yield (row, col) for each ON event in payload.
    Payload may contain one or more records.
    """
    i = 0
    while i < len(payload):
        rec_type = payload[i]

        if rec_type == USB_EVT_REC_V1_NOTS:
            if i + 4 > len(payload):
                return
            _, flags, row, col = struct.unpack_from("<BBBB", payload, i)
            i += 4
            if flags & USB_EVT_FLAG_ON:
                yield row, col

        elif rec_type == USB_EVT_REC_V1_TICKS:
            if i + 8 > len(payload):
                return
            _, flags, row, col, _ticks = struct.unpack_from("<BBBBI", payload, i)
            i += 8
            if flags & USB_EVT_FLAG_ON:
                yield row, col

        else:
            # Unknown record type; stop to avoid desync
            return


def main():
    ap = argparse.ArgumentParser(description="Visualize 32x32 ON events from Pico USB stream.")
    ap.add_argument("--port", default=None, help="Serial port (e.g., /dev/ttyACM0, COM5). Auto-detect if omitted.")
    ap.add_argument("--baud", type=int, default=115200, help="Baud (ignored for USB CDC).")
    ap.add_argument("--scale", type=int, default=16, help="Pixel scale factor for display.")
    ap.add_argument("--fps", type=int, default=60, help="Display refresh rate.")
    ap.add_argument("--decay-ms", type=int, default=200, help="Fade-out time after last event (0 = no decay).")
    ap.add_argument("--persist", action="store_true", help="Alias for --decay-ms 0 (pixels stay on).")
    args = ap.parse_args()

    if args.persist:
        args.decay_ms = 0

    port = args.port or auto_find_port()
    if not port:
        print("No serial ports found.")
        sys.exit(1)

    print(f"Opening {port} ...")
    ser = serial.Serial(port, args.baud, timeout=0.0)
    reader = FramedStreamReader(ser)

    # 32x32 grid stores "last seen time" in seconds
    last_seen = [[-1.0 for _ in range(32)] for _ in range(32)]

    pygame.init()
    w, h = 32 * args.scale, 32 * args.scale
    screen = pygame.display.set_mode((w, h))
    pygame.display.set_caption("AER 32x32 ON Events")
    clock = pygame.time.Clock()

    running = True
    try:
        while running:
            # Handle window events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False

            # Non-blocking read loop: pull as many packets as are available quickly
            # We "pump" the serial buffer for a short time each frame.
            t_start = time.time()
            while True:
                # Stop pumping after a small time slice to keep UI responsive
                if time.time() - t_start > 0.005:
                    break

                # If no bytes waiting, stop pumping
                if ser.in_waiting <= 0:
                    break

                ver, ptype, payload = reader.read_packet()
                if ptype != HAL_STREAM_EVENT_BIN:
                    continue

                now = time.time()
                for row, col in extract_on_events(payload):
                    if 0 <= row < 32 and 0 <= col < 32:
                        last_seen[row][col] = now

            # Render
            now = time.time()
            screen.fill((0, 0, 0))

            for r in range(32):
                for c in range(32):
                    t = last_seen[r][c]
                    if t < 0:
                        continue

                    if args.decay_ms == 0:
                        intensity = 255
                    else:
                        age_ms = (now - t) * 1000.0
                        if age_ms >= args.decay_ms:
                            continue
                        # Linear fade (simple)
                        intensity = int(255 * (1.0 - age_ms / args.decay_ms))

                    # grayscale intensity
                    color = (intensity, intensity, intensity)
                    x = c * args.scale
                    y = r * args.scale
                    pygame.draw.rect(screen, color, (x, y, args.scale, args.scale))

            pygame.display.flip()
            clock.tick(args.fps)

    finally:
        ser.close()
        pygame.quit()


if __name__ == "__main__":
    main()
