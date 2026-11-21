# Esc or Ctrl+C stops this script, even while it sleeps.
import time

for count in range(1, 11):
    print("Count:", count, "uptime:", time.ticks_ms(), "ms")
    time.sleep(1)
print("Finished")
