# Smart Home With Voice Recognition — Offline & Online Modes

This project runs on an ESP32 and supports both internet-connected (STA) and offline (AP/LittleFS) usage.

Key features
- Serves the web UI from LittleFS (`index.html`, `Landing_Page.html`).
- Uses `WiFiManager` for easy Wi‑Fi setup (captive portal `ESP32-Setup`).
- Runs in STA + AP mode so clients can connect locally (`ESP32-AP`) even without upstream internet.
- Provides REST endpoints `/control` and `/status` and voice-module integration.

Quick start

1. Install required libraries in the Arduino IDE: `WiFi`, `WiFiManager`, `LittleFS` for ESP32, and the `ESP32 LittleFS Data Upload` plugin (or use PlatformIO).
2. Put your `index.html` and `Landing_Page.html` into the data folder (Arduino: `sketch/data`, PlatformIO: `data/`).
3. Upload the filesystem image:

Arduino IDE (with plugin installed):
```bash
# Tools → ESP32 Sketch Data Upload
```

PlatformIO:
```bash
pio run -t uploadfs
```

4. Upload the sketch `esp32_control.ino` from the Arduino IDE or PlatformIO.
5. Open Serial Monitor at `115200` — the device will start WiFiManager captive portal `ESP32-Setup` if no saved credentials.
6. After configuration, the device prints its STA IP and also starts the AP `ESP32-AP` (IP usually `192.168.4.1`).

Usage modes

- Online (STA): connect the ESP32 to your router via the captive portal. The ESP32 can reach the internet; it will still serve the web UI from LittleFS.
- Offline (AP): if no router is available, connect a client (phone/PC) to `ESP32-AP` and browse to `http://192.168.4.1` (or the IP printed on Serial) to access the UI.

Notes

- The firmware periodically checks upstream internet availability by requesting `http://clients3.google.com/generate_204` and logs whether the internet is reachable.
- To update web files, re-run the LittleFS upload step then reboot the ESP32 or re-open the page.

If you want, I can:
- Add OTA update support
- Add fallback to fetch remote resources when internet is available
- Create a small local server script for desktop testing

Firebase (Realtime Database) sync
- This project can optionally sync actions to a Firebase Realtime Database. To enable:
	1. Create a Firebase project at https://console.firebase.google.com/
 2. In the project, go to Realtime Database and create a database. Note the database URL (example: `https://PROJECT_ID.firebaseio.com`).
 3. For quick testing set the Realtime DB rules to allow read/write (not for production):

```
{
	"rules": {
		".read": true,
		".write": true
	}
}
```

4. In `esp32_control.ino` set `FIREBASE_DB` to your database URL. If you need authentication, set `FIREBASE_AUTH` with your token.
5. The firmware will store local user actions in a queue on LittleFS and automatically flush them to `/<your-db>/actions/` when internet is available.

I can add secure auth (Firebase Auth + token minting) when you're ready for production.
