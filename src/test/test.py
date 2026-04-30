# test.py — exercises every token type the lexer supports

x = 10
y = 3.14
name = 'hello'
flag = True
nothing = None

if x == 10:
    print(name)
elif x != 5:
    x += 1
else:
    x -= 1

while x > 0:
    x = x - 1
    if x == 5:
        break
    if x == 7:
        continue
    pass

for i in [1, 2, 3]:
    y = y * 2.0
    if y >= 100.0:
        break

def add(a, b):
    return a + b

def power(base, exp):
    result = base ** exp
    return result

z = add(x, 10) // 3
r = z % 2
big = z * 10
div = big / 4

check = (x <= 0) or (y >= 3.14)
both  = (x > 0) and (y < 100.0)
inv   = not flag

items = [1, 2, 3]
coords = {'x': 1, 'y': 2}
nested = (x + z) * (y - 1.0)
