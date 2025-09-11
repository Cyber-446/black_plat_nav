#!/usr/bin/env python3
"""
test.py
Шлёт UDP‑команды каждые SEND_PERIOD секунд, чтобы платформа ровно проехала
100 см вперёд (лин. скорость в см/с).

Используйте:  python3 test.py
"""

import socket
import struct
import time

# ────── протокол ───────────────────────────────────────────────────────
PKT_SIZE         = 128
BOARD_ADDR       = 0x23
UPPER_ADDR       = 0xAA
GEO_MODE_MASK    = 0x01

LIN_VEL_OFFSET   = 10
ANG_VEL_OFFSET   = 14
GEOM_POS_OFFSET  = 21
CRC_OFFSET       = 125

# ────── сеть ───────────────────────────────────────────────────────────
REMOTE_IP   = "192.168.3.5"
REMOTE_PORT = 4001
LOCAL_PORT  = 0               # 0 = любой свободный

# ────── движение ───────────────────────────────────────────────────────
DIST_CM      = 100.0          # сколько сантиметров проехать
LIN_SPEED_CM = 20.0           # см/с >0 вперёд
ANG_SPEED    = 0.0            # рад/с
GEO_MODE     = 0              # 0 = PWM‑скорость, 1 = позиция

SEND_PERIOD  = 0.1            # сек, как в GUI
DURATION     = DIST_CM / LIN_SPEED_CM  # общее время движения

# ────── CRC16‑CCITT ────────────────────────────────────────────────────
def crc16_ccitt(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc

# ────── формирование пакета ────────────────────────────────────────────
def build_packet(lin_cm_s: float, ang: float) -> bytes:
    p = bytearray(PKT_SIZE)
    p[0] = BOARD_ADDR
    p[1] = GEO_MODE & GEO_MODE_MASK
    p[2] = 0x01                     # шасси PWM
    p[3] = 0x00                     # ID
    struct.pack_into("<f", p, LIN_VEL_OFFSET,  lin_cm_s)
    struct.pack_into("<f", p, ANG_VEL_OFFSET,  ang)
    struct.pack_into("<f", p, GEOM_POS_OFFSET, 0.0)
    struct.pack_into("<H", p, CRC_OFFSET, crc16_ccitt(p[:CRC_OFFSET]))
    p[-1] = UPPER_ADDR
    return bytes(p)

# ────── отправка ───────────────────────────────────────────────────────
def send(sock: socket.socket, pkt: bytes):
    sock.sendto(pkt, (REMOTE_IP, REMOTE_PORT))

# ────── main ───────────────────────────────────────────────────────────
def main():
    pkt_run  = build_packet(LIN_SPEED_CM, ANG_SPEED)
    pkt_stop = build_packet(0.0, 0.0)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("", LOCAL_PORT))

        # ♦ страхующий стоп
        send(s, pkt_stop)
        time.sleep(0.1)

        # ♦ движение с циклической отправкой
        end = time.time() + DURATION
        while time.time() < end:
            send(s, pkt_run)
            time.sleep(SEND_PERIOD)

        # ♦ финальный стоп
        send(s, pkt_stop)
        time.sleep(0.1)

if __name__ == "__main__":
    main()
