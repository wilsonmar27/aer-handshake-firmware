#!/usr/bin/env python3
import argparse
import struct
import sys
import time
import serial
from serial.tools import list_ports

MAGIC = b"AERS"
HDR_LEN = 8  # magic(4) + ver(1) + type(1) + len(2)

# hal_stream_type_t (from hal_stdio.h)
HAL_STREAM_LOG_TEXT  = 1
HAL_STREAM_EVENT_BIN = 2
HAL_STREAM_RAW_BIN   = 3
HAL_STREAM_MARKER    = 4

# usb_stream_event_rec_type_t (from usb_stream.h)
USB_EVT_REC_V1_NOTS  = 1  # rec_type,u8 flags,u8 row,u8 col,u8
USB_EVT_REC_V1_TICKS = 2  # above + u32 ticks

USB_EVT_FLAG_ON = 0x01


def auto_find_port() -> str | None:
    """Try to auto-pick a likely Pico CDC port."""
    ports = list(list_ports.comports())
    if not ports:
        return None

    # Heuristic: look for "Pico" or "Raspberry Pi" in description/manufacturer
    for p in ports:
        text = f"{p.description} {p.manufacturer} {p.product}".lower()
        if "pico" in text or "raspberry" in text or "rp2350" in text:
            return p.device

    # Fallback: first ACM/USB serial port
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

    def _find_magic(self) -> int:
        return self.buf.find(MAGIC)

    def read_packet(self):
        """
        Returns (ver:int, ptype:int, payload:bytes) or None if not enough data yet.
        Resyncs on MAGIC.
        """
        while True:
            self._read_some()

            idx = self._find_magic()
            if idx < 0:
                # keep last few bytes in case MAGIC spans reads
                if len(self.buf) > 3:
                    self.buf = self.buf[-3:]
                continue

            # discard everything before MAGIC
            if idx > 0:
                del self.buf[:idx]

            if len(self.buf) < HDR_LEN:
                continue

            # Parse header: AERS, ver, type, len_le
            # Header layout: magic[4], ver:u8, type:u8, len:u16le
            ver = self.buf[4]
            ptype = self.buf[5]
            plen = struct.unpack_from("<H", self.buf, 6)[0]

            total_len = HDR_LEN + plen
            if len(self.buf) < total_len:
                continue

            payload = bytes(self.buf[HDR_LEN:total_len])
            del self.buf[:total_len]
            return ver, ptype, payload


def decode_and_print_events(payload: bytes, show_ticks: bool):
    """
    Payload is the inner bytes of HAL_STREAM_EVENT_BIN.
    Your usb_stream sends 1 record per packet, but we allow multiple.
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
                print(f"ON  row={row:02d} col={col:02d}")
        elif rec_type == USB_EVT_REC_V1_TICKS:
            if i + 8 > len(payload):
                return
            _, flags, row, col, ticks = struct.unpack_from("<BBBBI", payload, i)
            i += 8
            if flags & USB_EVT_FLAG_ON:
                if show_ticks:
                    print(f"ON  row={row:02d} col={col:02d}  ticks={ticks}")
                else:
                    print(f"ON  row={row:02d} col={col:02d}")
        else:
            # Unknown record type: bail out so we don't desync the stream
            print(f"[warn] Unknown event record type {rec_type}; payload_len={len(payload)}")
            return


def main():
    ap = argparse.ArgumentParser(description="Print ON events from Pico USB framed stream.")
    ap.add_argument("--port", default=None, help="Serial port (e.g., /dev/ttyACM0, COM5). If omitted, tries auto-detect.")
    ap.add_argument("--baud", type=int, default=115200, help="Baud (ignored for USB CDC, but required by pyserial).")
    ap.add_argument("--show-non-events", action="store_true", help="Print non-event packets (markers/logs) too.")
    ap.add_argument("--show-ticks", action="store_true", help="Print cycle ticks timestamps when present.")
    args = ap.parse_args()

    port = args.port or auto_find_port()
    if not port:
        print("No serial ports found.")
        sys.exit(1)

    print(f"Opening {port} ...")
    with serial.Serial(port, args.baud, timeout=0.1) as ser:
        reader = FramedStreamReader(ser)
        print("Listening (Ctrl+C to stop)...")

        try:
            while True:
                pkt = reader.read_packet()
                if not pkt:
                    continue
                ver, ptype, payload = pkt

                if ptype == HAL_STREAM_EVENT_BIN:
                    decode_and_print_events(payload, show_ticks=args.show_ticks)
                elif args.show_non_events:
                    # Helpful for debug if you enable markers/logs
                    if ptype in (HAL_STREAM_LOG_TEXT, HAL_STREAM_MARKER):
                        try:
                            txt = payload.decode("utf-8", errors="replace")
                        except Exception:
                            txt = repr(payload)
                        print(f"[type={ptype} ver={ver}] {txt}")
                    else:
                        print(f"[type={ptype} ver={ver}] {payload.hex()}")
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
