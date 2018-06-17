import numpy as np

import sys
print(sys.version)

a = 0.2
b = 8
func = lambda x: (a * x + b) * x

xx = np.arange(256)
yy = func(xx)

yy = yy * 255 / yy[-1]

print(', '.join('0x{:02x}'.format(int(round(y))) for y in yy))
