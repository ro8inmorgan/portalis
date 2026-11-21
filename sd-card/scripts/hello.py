# Run from the Python scripts menu on the Cardputer.
import math
import gc

print("Hello from MicroPython!")
print("Squares:", [x * x for x in range(10)])
print("Square root of 81:", math.sqrt(81))
gc.collect()
print("Python heap free:", gc.mem_free(), "bytes")
