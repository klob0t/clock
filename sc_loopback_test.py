import soundcard as sc
import numpy as np
import time

# 1. Pick the default speaker (same as Windows default output)
spk = sc.default_speaker()
print("Default speaker:", repr(spk.name))

# 2. Get its loopback as a "microphone"
mic = sc.get_microphone(spk.name, include_loopback=True)
print("Using loopback mic:", repr(mic.name))

SAMPLE_RATE = 48000
BLOCK = 1024  # frames per read

with mic.recorder(samplerate=SAMPLE_RATE, channels=2) as rec:
    print("Recording SYSTEM audio via loopback... Ctrl+C to stop.")
    while True:
        data = rec.record(numframes=BLOCK)   # shape: (BLOCK, 2), float32 [-1, 1]
        mono = data.mean(axis=1)             # mix L+R to mono
        level = float(np.mean(np.abs(mono))) # average absolute amplitude
        print(f"Level: {level:.5f}")
        time.sleep(0.02)
