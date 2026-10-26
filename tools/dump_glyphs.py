import sys
sys.path.insert(0, r"C:\Users\doge6\AppData\Local\Temp\claude\C--Users-doge6-Videos-GammaSource-main\a21c1dde-df0d-438a-98db-ab8502d5afe4\scratchpad")
from atlas import read_vtf_bgr888
w, h, img = read_vtf_bgr888(sys.argv[1])
x0, x1 = 118, 256
for y in range(29, 41):
    print(f"{y:2d} " + ''.join(
        ('#' if v > 170 else '+' if v > 90 else '.' if v > 30 else ' ')
        for v in (sum(img[y][x]) // 3 for x in range(x0, x1))))
print('cols', ''.join(str(x % 10) for x in range(x0, x1)))
# raw values for E in HEALTH
for y in range(30, 40):
    print(y, [img[y][x] for x in range(167, 174)])
