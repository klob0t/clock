import sounddevice as sd                                                                          
with sd.InputStream(device="Stereo Mix (Realtek HD Audio Stereo input)", samplerate=44100, channels=1, blocksize=512, latency=0.05):                                                         
    print("ok")                                                                 
                                                                                                    
with sd.InputStream(device=idx, channels=1, samplerate=44100, blocksize=256):
    print("InputStream open OK")      