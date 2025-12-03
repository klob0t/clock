import soundcard as sc

print("=== Speakers ===")
for spk in sc.all_speakers():
    print(f"- {spk.name!r}")

print("\n=== Microphones (include_loopback=True) ===")
for mic in sc.all_microphones(include_loopback=True):
    print(f"- {mic.name!r}")