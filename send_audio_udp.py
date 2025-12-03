import socket
import soundcard as sc
import numpy as np

# === CONFIG ===
ESP_HOST = "myclock.local"   # same mDNS name as your clock (or set to IP)
ESP_PORT = 4211              # AUDIO_UDP_PORT on the ESP
WIDTH = 40                   # columns of your LED matrix
SAMPLE_RATE = 48000
BLOCK = 512                  # frames per read (smaller = snappier updates)

# Resolve ESP once
esp_ip = socket.gethostbyname(ESP_HOST)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Pick default speaker and its loopback
spk = sc.default_speaker()
print("Default speaker: ", repr(spk.name))
mic = sc.get_microphone(spk.name, include_loopback=True)
print("Using loopback mic: ", repr(mic.name))
print(f"Streaming oscilloscope data to {esp_ip}:{ESP_PORT} (WIDTH={WIDTH})")

with mic.recorder(samplerate=SAMPLE_RATE, channels=1) as rec:
    frame_count = 0
    while True:
        # 1. grab audio chunk
        data = rec.record(numframes=BLOCK)    # shape: (BLOCK, 1)
        samples = data[:, 0]                  # 1D float array [-1..1]

        # 2. downsample BLOCK -> WIDTH
        idx = np.linspace(0, len(samples) - 1, WIDTH).astype(int)
        slice_ = samples[idx]

        # 3. normalize -1..1 -> 0..255 with higher gain
        slice_ = slice_ * 20
        slice_ = ((slice_ + 1.0) * 0.5 * 255.0).clip(0, 255).astype(np.uint8)

        # 4. send as 40-byte packet
        sock.sendto(slice_.tobytes(), (esp_ip, ESP_PORT))

        # optional debug
        frame_count += 1
        if frame_count % 60 == 0:
            level = float(np.mean(np.abs(slice_))) / 255.0
            print(f"[debug] approx level={level:.3f} min={slice_.min()} max={slice_.max()}")
