#!/usr/bin/env python3
import subprocess, sys, os, urllib.request, time, serial.tools.list_ports

URL = "https://github.com/adafruit/tinyuf2/releases/download/0.15.0/tinyuf2-xiao_esp32s3-0.15.0.uf2"
UF2 = "tinyuf2.uf2"

def main():
    if not os.path.exists(UF2):
        print("Downloading UF2…")
        urllib.request.urlretrieve(URL, UF2)
    print("Hold BOOT, tap RESET, release BOOT…")
    time.sleep(3)
    for p in serial.tools.list_ports.comports():
        if "XIAO-ESP32S3" in p.description:
            print(f"Flashing {UF2} to {p.device}")
            subprocess.run(["esptool.py", "--chip", "esp32s3", "--port", p.device,
                            "write_flash", "0x0", UF2], check=True)
            print("Done. Reset the board.")
            return
    print("ESP32-S3 not found in download mode.")

if __name__ == "__main__":
    main()