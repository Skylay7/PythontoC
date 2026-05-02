def factorial(n):
    result = 1
    i = 1
    while i <= n:
        result *= i
        i += 1
    return result

def is_even(n):
    if n == 0:
        return True
    r = n - 2
    if r < 0:
        return False
    return is_even(r)

def sum_range(limit):
    total = 0
    for i in range(limit):
        total += i
    return total

x = 5
y = 3.14
name = "Alice"
flag = True

if x > 0:
    print(x)
elif x == 0:
    print(0)
else:
    print(-1)

count = 0
while count < x:
    if count == 2:
        count += 1
        continue
    print(count)
    count += 1

for i in range(10):
    if i == 7:
        break
    pass

f = factorial(x)
print(f)

s = sum_range(x)
print(s)

a = 10
b = 4
c = a + b
d = a - b
e = not flag
print(c)
print(d)
print(e)
