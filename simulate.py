"""Desk simulator for the greenhouse control rules.

Mirrors the hysteresis logic in smart_greenhouse.ino so thresholds can be tuned
without hardware. Run: python simulator/simulate.py
"""
SOIL_ON, SOIL_OFF = 35.0, 55.0
TEMP_ON, TEMP_OFF = 30.0, 27.0
PUMP_MAX_S, PUMP_REST_S = 60, 300


def run(hours=6, step=10):
    soil, temp = 60.0, 24.0
    pump = fan = False
    started = stopped = None
    log = []
    for s in range(0, hours * 3600, step):
        # crude physics: sun heats, soil dries; fan cools; pump wets
        temp += 0.004 * step * (1 if (s // 3600) % 24 < 8 else -0.5)
        temp -= 0.012 * step if fan else 0
        soil -= 0.004 * step
        soil += 0.05 * step if pump else 0
        soil = max(0, min(100, soil)); temp = max(15, temp)
        if not fan and temp >= TEMP_ON: fan = True
        elif fan and temp <= TEMP_OFF: fan = False
        if not pump and soil <= SOIL_ON and (stopped is None or s - stopped >= PUMP_REST_S):
            pump, started = True, s
        elif pump and (soil >= SOIL_OFF or s - started >= PUMP_MAX_S):
            pump, stopped = False, s
        if s % 600 == 0:
            log.append((s // 60, round(temp, 1), round(soil), pump, fan))
    return log


if __name__ == "__main__":
    print("min  temp  soil  pump  fan")
    for row in run():
        print("%4d %5.1f %5d %5s %4s" % row)
