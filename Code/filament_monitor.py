#!/usr/bin/env python3
"""
filament_monitor.py
────────────────────────────────────────────────────────────────
Subscribes to the ESP32's MQTT speed topic, polls Klipper's
Moonraker API for the expected filament feed rate, and raises
an alert when the measured speed deviates beyond the configured
tolerance — but ONLY while Klipper is actively printing and
extruding (not retracting, not idle, not in manual extrusion).

Dependencies:
    pip install paho-mqtt requests

Moonraker must be reachable on the local network (default: localhost).
"""

import time
import json
import logging
import threading
import requests
import paho.mqtt.client as mqtt

# ── User config ───────────────────────────────────────────────
MQTT_BROKER      = "localhost"          # mosquitto runs locally
MQTT_PORT        = 1883
MQTT_TOPIC       = "printer/filament/speed"
MQTT_USER        = ""                   # leave blank if no auth
MQTT_PASS        = ""

MOONRAKER_URL    = "http://localhost:7125"   # adjust if Moonraker is remote

# Tolerance: alert if |measured - expected| > TOLERANCE_FRACTION * expected
# e.g. 0.20 = allow ±20%
TOLERANCE_FRACTION = 0.20

# Minimum expected speed to bother checking (mm/s).
# Below this the extruder is basically idle / bridging; skip comparison.
MIN_CHECK_SPEED_MMS = 0.5

# How often to poll Moonraker for printer state (seconds)
POLL_INTERVAL = 0.25

# How many consecutive out-of-tolerance readings before alerting
ALERT_CONSECUTIVE = 3

# ── Logging ───────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("filament_monitor")

# ── Shared state (written by MQTT thread, read by main loop) ──
_lock            = threading.Lock()
_measured_speed  = 0.0   # mm/s from sensor
_sensor_stalled  = True


# ══════════════════════════════════════════════════════════════
#  Moonraker helpers
# ══════════════════════════════════════════════════════════════

def _get(path: str, timeout: float = 2.0):
    """GET from Moonraker, return parsed JSON or None on failure."""
    try:
        r = requests.get(f"{MOONRAKER_URL}{path}", timeout=timeout)
        r.raise_for_status()
        return r.json()
    except Exception as exc:
        log.debug("Moonraker GET %s failed: %s", path, exc)
        return None


def get_printer_state() -> dict:
    """
    Returns a dict with keys:
        printing      bool   — a print job is active (not paused/idle)
        extruding     bool   — extruder is actively moving filament forward
        expected_mms  float  — filament feed rate in mm/s (volumetric-corrected to linear)
    """
    result = {
        "printing":     False,
        "extruding":    False,
        "expected_mms": 0.0,
    }

    # ── 1. Is a print job running (not paused, not standby)? ──
    job = _get("/printer/objects/query?print_stats")
    if not job:
        return result

    stats = job.get("result", {}).get("status", {}).get("print_stats", {})
    state = stats.get("state", "standby")   # idle | printing | paused | complete | error | standby
    if state != "printing":
        return result
    result["printing"] = True

    # ── 2. Query extruder motion objects ──────────────────────
    motion = _get(
        "/printer/objects/query"
        "?extruder"
        "&motion_report"
        "&gcode_move"
    )
    if not motion:
        return result

    status = motion.get("result", {}).get("status", {})

    extruder     = status.get("extruder", {})
    motion_rep   = status.get("motion_report", {})
    gcode_move   = status.get("gcode_move", {})

    # ── 3. Determine if extruder is pushing filament forward ──
    #
    # Klipper exposes `extruder.velocity` in mm/s (signed: + = extrude, - = retract).
    # If the key is absent, fall back to motion_report.live_velocity (scalar, unsigned)
    # cross-referenced with gcode_move.extrude_factor.
    #
    extruder_velocity = extruder.get("velocity", None)

    if extruder_velocity is not None:
        # Direct signed velocity — most reliable
        if extruder_velocity <= 0:
            # Retracting or idle — do not check
            return result
        result["extruding"]    = True
        result["expected_mms"] = float(extruder_velocity)
    else:
        # Fallback: use motion_report live velocity for the extruder axis.
        # live_extruder_velocity is available in some Klipper builds.
        live_e = motion_rep.get("live_extruder_velocity", None)
        if live_e is not None:
            if float(live_e) <= 0:
                return result
            result["extruding"]    = True
            result["expected_mms"] = float(live_e)
        else:
            # Last resort: if extruder pressure_advance position is advancing,
            # we can't reliably determine direction — skip.
            return result

    return result


# ══════════════════════════════════════════════════════════════
#  MQTT callbacks
# ══════════════════════════════════════════════════════════════

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info("Connected to MQTT broker.")
        client.subscribe(MQTT_TOPIC)
        log.info("Subscribed to %s", MQTT_TOPIC)
    else:
        log.error("MQTT connect failed (rc=%d)", rc)


def on_message(client, userdata, msg):
    global _measured_speed, _sensor_stalled
    try:
        data = json.loads(msg.payload)
        with _lock:
            _measured_speed = float(data.get("speed_mm_s", 0.0))
            _sensor_stalled = bool(data.get("stalled", True))
    except Exception as exc:
        log.warning("Bad MQTT payload: %s — %s", msg.payload, exc)


# ══════════════════════════════════════════════════════════════
#  Main monitoring loop
# ══════════════════════════════════════════════════════════════

def main():
    # ── MQTT setup ────────────────────────────────────────────
    client = mqtt.Client(client_id="filament_monitor_pi")
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
    client.loop_start()   # runs MQTT in background thread

    log.info("Filament speed monitor started.")
    log.info("Tolerance: ±%.0f%%   Min check speed: %.1f mm/s",
             TOLERANCE_FRACTION * 100, MIN_CHECK_SPEED_MMS)

    consecutive_faults = 0

    while True:
        time.sleep(POLL_INTERVAL)

        state = get_printer_state()

        if not state["printing"]:
            # Not printing — nothing to do
            consecutive_faults = 0
            continue

        if not state["extruding"]:
            # Printing but retracting or paused between moves — skip
            consecutive_faults = 0
            continue

        expected = state["expected_mms"]

        if expected < MIN_CHECK_SPEED_MMS:
            # Expected speed too low to be meaningful (very slow bridging, etc.)
            consecutive_faults = 0
            continue

        with _lock:
            measured  = _measured_speed
            stalled   = _sensor_stalled

        if stalled:
            # Sensor reports no movement while Klipper says extruder is running
            consecutive_faults += 1
            log.warning(
                "[FAULT %d/%d] Sensor stalled — expected %.3f mm/s, got 0.000 mm/s",
                consecutive_faults, ALERT_CONSECUTIVE, expected,
            )
        else:
            deviation = abs(measured - expected)
            tolerance = TOLERANCE_FRACTION * expected

            if deviation > tolerance:
                consecutive_faults += 1
                pct = (deviation / expected) * 100.0
                log.warning(
                    "[FAULT %d/%d] Speed mismatch — expected %.3f mm/s, "
                    "measured %.3f mm/s (%.1f%% deviation, limit ±%.0f%%)",
                    consecutive_faults, ALERT_CONSECUTIVE,
                    expected, measured, pct, TOLERANCE_FRACTION * 100,
                )
            else:
                # Reading is good — reset fault counter
                if consecutive_faults > 0:
                    log.info("Speed back in range — fault counter reset.")
                consecutive_faults = 0
                log.debug(
                    "OK  expected=%.3f  measured=%.3f  diff=%.3f mm/s",
                    expected, measured, deviation,
                )

        # ── Alert threshold reached ────────────────────────────
        if consecutive_faults >= ALERT_CONSECUTIVE:
            _raise_alert(expected, measured, stalled)
            consecutive_faults = 0   # reset so alert doesn't fire every cycle


def _raise_alert(expected: float, measured: float, stalled: bool):
    """
    Called when ALERT_CONSECUTIVE bad readings accumulate.
    Extend this function to send emails, push notifications,
    trigger GPIO, pause the print via Moonraker, etc.
    """
    if stalled:
        message = (
            f"FILAMENT ALERT: Sensor reports NO MOVEMENT while Klipper expects "
            f"{expected:.3f} mm/s. Possible jam, runout, or sensor failure."
        )
    else:
        pct = abs(measured - expected) / expected * 100
        message = (
            f"FILAMENT ALERT: Speed mismatch — "
            f"expected {expected:.3f} mm/s, measured {measured:.3f} mm/s "
            f"({pct:.1f}% deviation). Possible slip, jam, or calibration error."
        )

    log.error("━" * 60)
    log.error(message)
    log.error("━" * 60)

    # ── Optional: pause the print via Moonraker ────────────────
    # Uncomment the block below to automatically pause on alert.
    #
    # try:
    #     r = requests.post(
    #         f"{MOONRAKER_URL}/printer/print/pause",
    #         timeout=3,
    #     )
    #     if r.ok:
    #         log.error("Print PAUSED via Moonraker.")
    #     else:
    #         log.error("Moonraker pause failed: %s", r.text)
    # except Exception as exc:
    #     log.error("Could not reach Moonraker to pause: %s", exc)


if __name__ == "__main__":
    main()
