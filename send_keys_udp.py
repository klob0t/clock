import socket
from pynput import keyboard

HOST = "myclock.local"  # set to IP if mDNS fails
PORT = 4210
# Allow multiple chord variants to avoid missing right/left modifiers.
COMBOS = {
    (keyboard.Key.ctrl, keyboard.Key.alt, keyboard.Key.f10): b"\x20",
    (keyboard.Key.ctrl_l, keyboard.Key.alt_l, keyboard.Key.f10): b"\x20",
    (keyboard.Key.ctrl_r, keyboard.Key.alt_gr, keyboard.Key.f10): b"\x20",
}

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
pressed = set()

def normalize(key):
    if isinstance(key, keyboard.KeyCode):
        return key.char
    return key

def check_combos():
    for combo, payload in COMBOS.items():
        if all(k in pressed for k in combo):
            sock.sendto(payload, (HOST, PORT))
            print(f"sent {payload.hex()} for combo {combo}")

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
