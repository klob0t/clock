import socket
import numpy as np
import sounddevice as sd

HOST = "myclock.local"  # set to ESP IP if mDNS fails
PORT = 4211             # AUDIO_UDP_PORT on the ESP
RATE = 16000            # capture sample rate
CHUNK = 256             # samples per UDP packet
DEVICE = 11           # set to input device index/name if default fails
LATENCY = None          # e.g., 0.05 to hint latency if needed

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


def callback(indata, frames, time, status):
  if status:
    print(status)
  mono = indata[:, 0]
  samples = np.clip((mono + 1.0) * 127.5, 0, 255).astype(np.uint8)
  sock.sendto(samples.tobytes(), (HOST, PORT))
  # Debug: print a brief summary every chunk
  print(f"sent {len(samples)} bytes to {HOST}:{PORT} min={samples.min()} max={samples.max()}")


with sd.InputStream(channels=1,
                    samplerate=RATE,
                    blocksize=CHUNK,
                    callback=callback,
                    device=DEVICE,
                    latency=LATENCY):
  print(f"Streaming audio to {HOST}:{PORT}. Ctrl+C to stop.")
  while True:
    sd.sleep(1000)
