import socket
import time
from pynput import keyboard

HOST = "myclock.local"  # set to IP if mDNS fails
PORT = 4210

COMBOS = {
    (keyboard.Key.ctrl, keyboard.Key.alt, keyboard.Key.f10): b"\x20",
    (keyboard.Key.ctrl_l, keyboard.Key.alt_l, keyboard.Key.f10): b"\x20",
    (keyboard.Key.ctrl_r, keyboard.Key.alt_gr, keyboard.Key.f10): b"\x20",
}

pressed = set()

def normalize(key):
    if isinstance(key, keyboard.KeyCode):
        return key.char
    return key

def send_payload(payload):
    while True:
        try:
            addr = socket.getaddrinfo(HOST, PORT, socket.AF_INET, socket.SOCK_DGRAM)[0][-1]
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(payload, addr)
            sock.close()
            break
        except OSError as exc:
            print(f"Send retry ({exc}); waiting for {HOST} to resolve/respond...")
            time.sleep(2)

def check_combos():
    for combo, payload in COMBOS.items():
        if all(k in pressed for k in combo):
            send_payload(payload)
            print(f"sent {payload.hex()} for combo {combo} -> {HOST}:{PORT}")

def on_press(key):
    k = normalize(key)
    if k:
        pressed.add(k)
        check_combos()

def on_release(key):
    k = normalize(key)
    if k and k in pressed:
        pressed.remove(k)

with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
    print(f"Listening... combos {list(COMBOS.keys())} -> {HOST}:{PORT} (0x20). Ctrl+C to exit.")
    listener.join()
