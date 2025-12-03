import socket
import soundcard as sc
import numpy as np

# === CONFIG ===
ESP_HOST = "myclock.local"   # set to ESP IP if mDNS fails
ESP_PORT = 4211              # AUDIO_UDP_PORT on the ESP
WIDTH = 40                   # columns of your LED matrix
SAMPLE_RATE = 48000
BLOCK = 1024                 # frames per FFT
FMIN = 40.0                  # min freq to consider (Hz)
FMAX = None                  # max freq (None = Nyquist)
FLOOR_DB = -90.0             # lower clamp for dB
CEIL_DB = -30.0              # upper clamp for dB
SMOOTH = 0.6                 # 0..1 smoothing for stability (higher = steadier)

esp_ip = socket.gethostbyname(ESP_HOST)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

spk = sc.default_speaker()
print("Default speaker: ", repr(spk.name))
mic = sc.get_microphone(spk.name, include_loopback=True)
print("Using loopback mic: ", repr(mic.name))
print(f"Streaming spectrum data to {esp_ip}:{ESP_PORT} (WIDTH={WIDTH})")

smoothed = np.zeros(WIDTH, dtype=np.float32)

with mic.recorder(samplerate=SAMPLE_RATE, channels=1) as rec:
    frame_count = 0
    while True:
        data = rec.record(numframes=BLOCK)    # shape: (BLOCK, 1)
        samples = data[:, 0]

        # Hann window
        window = np.hanning(len(samples))
        windowed = samples * window

        # FFT magnitude
        spec = np.fft.rfft(windowed)
        mag = np.abs(spec)

# Band to WIDTH bins (log spacing between FMIN..FMAX)
        nyquist = SAMPLE_RATE / 2.0
        fmax = FMAX if FMAX is not None else nyquist
        fmax = min(fmax, nyquist)
        freqs = np.geomspace(FMIN, fmax, WIDTH + 1)
        bins = (freqs / nyquist * (len(mag) - 1)).astype(int)
        bins = np.clip(bins, 0, len(mag) - 1)
        # ensure strictly increasing bins
        for i in range(1, len(bins)):
            if bins[i] <= bins[i - 1]:
                bins[i] = bins[i - 1] + 1
        bins[-1] = min(bins[-1], len(mag) - 1)

        cols = np.zeros(WIDTH, dtype=np.float32)
        for i in range(WIDTH):
            start = bins[i]
            end = bins[i + 1] if i + 1 < len(bins) else len(mag)
            band = mag[start:end]
            if band.size > 0:
                # RMS to reduce bias toward wide bands
                cols[i] = np.sqrt(np.mean(band * band))

        # dB normalize
        cols = 20.0 * np.log10(cols + 1e-6)
        cols = (cols - FLOOR_DB) / (CEIL_DB - FLOOR_DB)
        cols = np.clip(cols, 0.0, 1.0)

        # Smooth
        smoothed = SMOOTH * smoothed + (1.0 - SMOOTH) * cols

        # Map to 0..255
        out = (smoothed * 255.0).astype(np.uint8)

        sock.sendto(out.tobytes(), (esp_ip, ESP_PORT))

        frame_count += 1
        if frame_count % 60 == 0:
            print(f"[debug] min={out.min()} max={out.max()}")
