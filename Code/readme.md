# How to use

Prerequisites:
- Arduino IDE
- The assembled module

*Remember not to plug in the ESP32's USB while it has 5V input!*

Please note that I am *not* a software guy. I used to be, but recently I seem to have lost my coding touch. And so, this code was made with not a little help from an AI; even still, I've looked over it in full and, from what I can remember about coding, it does check out. If anybody out there knows better, or would like to help me in coding a better version, I'm very open to collaboration! Email me at `me "at" anicetus "dot" dev`.

First, download the SBFM_Speed_Test folder and upload it into your Arduino IDE projects folder. Then open the app, select the project, and upload it to the ESP32 in the usual manner. It should immediately begin displaying the speed value measured by the photoelectric sensor in the console. To change the pin number that the ESP32 is reading from, simply change the variable in the file.

Then download the SBFM_Full_Code folder and open it in Arduino IDE, and input the blank network details. Ensure you have all the listed dependencies downloaded, then upload this file and unplug the esp32.

Next, on the Raspberry Pi Klipper host, run a `sudo apt update` and ensure mosquitto is installed (`sudo apt install -y mosquitto mosquitto-clients`). Enable + start it with `sudo systemctl enable mosquitto` and `sudo systemctl start mosquitto`. Then, create/edit `/etc/mosquitto/conf.d/local.conf` to have:

```
listener 1883
allow_anonymous true
```

Then restart: `sudo systemctl restart mosquitto`. Next, use `pip install paho-mqtt requests`. Make sure you can access a JSON with print_stats.state when running `curl http://localhost:7125/printer/objects/query?print_stats`.

Upload the filament_monitor.py file to the home/pi directory, and run `python3 filament_monitor.py` to manually test the code! Start a 3D print, and make sure the log lines update.
But we can't have it requiring a manual start every time. Create a new file at `/etc/systemd/system/filament_monitor.service` with the following:

```
[Unit]
Description=Filament Speed Monitor
After=network.target mosquitto.service

[Service]
ExecStart=/usr/bin/python3 /home/pi/filament_monitor.py
WorkingDirectory=/home/pi
Restart=always
RestartSec=5
User=pi

[Install]
WantedBy=multi-user.target
```

And enable the service with:

```
sudo systemctl daemon-reload
sudo systemctl enable filament_monitor
sudo systemctl start filament_monitor

# Watch logs live
journalctl -u filament_monitor -f
```

If you have issues with inaccuracy or mistaken failures, tune the values in the Pi's and the ESP32's code, such as TOLERANCE_FRACTION and STALL_TIMEOUT.

Once it seems to be working correctly (after manually causing the filament to get not extrude properly to test the failure detection), uncomment this line in the Pi's code: `r = requests.post(f"{MOONRAKER_URL}/printer/print/pause", timeout=3)`.

That should be it! Enjoy :)
