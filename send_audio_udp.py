import socket
import time
import warnings
import soundcard as sc
import numpy as np
from soundcard import SoundcardRuntimeWarning

# Best-effort oscilloscope sender with pitch-follow alignment (rising zero-cross) and 2-cycle window.
ESP_HOST = "myclock.local"   # set to ESP IP if mDNS is flaky
ESP_PORT = 4211              # AUDIO_UDP_PORT on the ESP
WIDTH = 40                   # columns of your LED matrix
SAMPLE_RATE = 48000
BLOCK = 512                  # frames per read
GAIN = 30                    # overall gain before mapping to 0..255
CYCLES = 2                   # show ~2 periods when possible

warnings.filterwarnings("ignore", category=SoundcardRuntimeWarning)

_addr = None
_sock = None

def send_frame(payload: bytes):
    """
    Reuses a single socket/address; only re-resolves on failure to reduce per-frame overhead.
    """
    global _addr, _sock
    while True:
        try:
            if _addr is None:
                _addr = socket.getaddrinfo(ESP_HOST, ESP_PORT, socket.AF_INET, socket.SOCK_DGRAM)[0][-1]
            if _sock is None:
                _sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            _sock.sendto(payload, _addr)
            return
        except OSError as exc:
            print(f"Send retry ({exc}); waiting for {ESP_HOST}...")
            if _sock:
                try:
                    _sock.close()
                except OSError:
                    pass
            _sock = None
            _addr = None
            time.sleep(1)

spk = sc.default_speaker()
print("Default speaker: ", repr(spk.name))
mic = sc.get_microphone(spk.name, include_loopback=True)
print("Using loopback mic: ", repr(mic.name))
print(f"Streaming oscilloscope data to {ESP_HOST}:{ESP_PORT} (WIDTH={WIDTH})")

with mic.recorder(samplerate=SAMPLE_RATE, channels=1) as rec:
    frame_count = 0
    while True:
        data = rec.record(numframes=BLOCK)    # shape: (BLOCK, 1)
        samples = data[:, 0]

        # Remove DC
        samples = samples - np.mean(samples)

        # Trigger: find rising zero-crossing on a lightly smoothed detector
        detector = np.convolve(samples, np.ones(8)/8.0, mode="same")
        start_search = len(detector) // 4
        end_search = len(detector) * 3 // 4
        first = None
        for i in range(start_search, end_search - 1):
            if detector[i] <= 0.0 and detector[i + 1] > 0.0:
                first = i + 1
                break
        if first is None:
            first = start_search

        # Estimate period from next crossing
        second = None
        for i in range(first + 1, end_search - 1):
            if detector[i] <= 0.0 and detector[i + 1] > 0.0:
                second = i + 1
                break
        period = (second - first) if second else BLOCK // 4
        period = max(period, 32)           # clamp to a sane minimum
        window_len = min(period * CYCLES, len(samples))

        # Align and crop window
        aligned = np.roll(samples, -first)[:window_len]

        # Resample window to WIDTH
        idx = np.linspace(0, len(aligned) - 1, WIDTH).astype(int)
        slice_ = aligned[idx]

        # Gain and map to 0..255
        slice_ = slice_ * GAIN
        slice_ = ((slice_ + 1.0) * 0.5 * 255.0).clip(0, 255).astype(np.uint8)

        send_frame(slice_.tobytes())
