@echo off
cd /d "D:\04. MISC\clock"
call .venv\Scripts\activate.bat
start "" /min python send_keys_udp.py
start "" /min python send_audio.udp.py