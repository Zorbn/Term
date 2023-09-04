# print("\x1b[31mhello red world\x1b[00m hello normal world0")
# print("hello, world\x1b[Hnew thingy\x1b[H", end="")

import time

print("a\x1b[1500Ca")

i = 0
while True:
    print(i)
    i += 1
    time.sleep(0.1)
