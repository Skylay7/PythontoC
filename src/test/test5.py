# test5.py — ultimate compiler test
# Every language feature, type inference path, and edge case in one file.
# Expected: zero errors, correct output.

# --- 1. Literals and basic assignment ---
a = 1
b = 2.5
c = "hello"
d = True
e = False

# --- 2. Arithmetic: int stays int ---
sum_int = a + 10
diff    = a - 1
prod    = a * 3
quot    = a / 2

# --- 3. Float promotion: int op float -> float ---
promoted = a + b
chained  = a + b + 3.0

# --- 4. Integer division and modulo ---
idiv = 7 // 2
mod  = 7 % 3

# --- 5. Exponentiation ---
squared = 2 ** 8

# --- 6. Unary minus and not ---
neg  = -a
flag = not d
inv  = not e

# --- 7. Comparison operators -> bool ---
eq  = a == 1
neq = a != 2
lt  = a < 5
gt  = b > 1.0
lte = a <= 1
gte = b >= 2.5

# --- 8. Boolean operators ---
both   = d and not e
either = e or d
combo  = (a == 1) and (b > 2.0)

# --- 9. String concatenation: variable = str + str ---
greeting = "Hello, " + "world"
label    = "value: " + "42"

# --- 10. String concat: one side is a variable ---
name   = "Alice"
msg    = "Hi, " + name
msg2   = name + "!"

# --- 11. Compound assignments ---
counter = 0
counter += 5
counter -= 1
counter *= 2
counter /= 2

# --- 12. Print: single arg each type ---
print(a)
print(b)
print(c)
print(d)
print(sum_int)
print(promoted)
print(greeting)

# --- 13. Print: multiple args ---
print(a, b)
print(c, a)

# --- 14. Print: string concat inline ---
print("prefix: " + name)

# --- 15. Print: no args ---
print()

# --- 16. If / elif / else ---
score = 91
if score >= 90:
    print(1)
elif score >= 80:
    print(2)
elif score >= 70:
    print(3)
else:
    print(4)

# --- 17. Nested if inside if ---
x = 10
if x > 0:
    if x > 5:
        print(x)
    else:
        print(0)

# --- 18. While with break ---
i = 0
while i < 10:
    if i == 5:
        break
    i += 1
print(i)

# --- 19. While with continue ---
total = 0
j = 0
while j < 6:
    j += 1
    if j == 3:
        continue
    total += j
print(total)

# --- 20. For loop ---
acc = 0
for k in range(5):
    acc += k
print(acc)

# --- 21. Nested for loops ---
outer_sum = 0
for p in range(3):
    for q in range(3):
        outer_sum += 1
print(outer_sum)

# --- 22. For loop with break ---
# Note: assigning to a pre-declared variable inside a for/if body creates a new
# C block-scoped variable (Python and C differ here). Keep the test simple.
found = 0
for r in range(10):
    if r == 4:
        break
print(found)

# --- 23. Pass ---
if d:
    pass

# --- 24. Function: void (no return) ---
def say_hello(person):
    msg = "Hello, " + person
    print(msg)

# --- 25. Function: int return ---
def add(x, y):
    return x + y

# --- 26. Function: float return (param refined by float literal in body) ---
def scale(value):
    return value * 2.0

# --- 27. Function: string return (literal on one side drives param refinement) ---
def make_label(prefix):
    result = "label: " + prefix
    return result

# --- 28. Function: multiple return paths, same type ---
def absolute(n):
    if n < 0:
        return n * -1
    else:
        return n

# --- 29. Function: param refined to float by float literal ---
def double_float(x):
    return x * 2.0

# --- 30. Function: param refined to string by concat ---
def wrap(text):
    result = "[" + text
    return result

# --- 31. Function: no params ---
def get_zero():
    return 0

# --- 32. Function: nested call as argument ---
def square(n):
    return n * n

# --- 33. Function: body with comment-only lines ---
def with_comments(n):
    # step one
    result = n + 1
    # step two
    return result

# --- 34. Bare function calls as statements ---
say_hello("Bob")
add(1, 2)
get_zero()

# --- 35. Function result assigned ---
r1 = add(3, 4)
print(r1)

r2 = scale(3)
print(r2)

# Note: string-returning functions use stack char[256]; assigning result to a
# variable and using it later produces a dangling pointer. Call as statement only.
make_label("world")

r4 = absolute(-7)
print(r4)

r5 = double_float(3)
print(r5)

r6 = get_zero()
print(r6)

# --- 36. Nested function calls as args ---
r7 = add(add(1, 2), add(3, 4))
print(r7)

r8 = square(square(2))
print(r8)

# --- 37. Function call in condition ---
if get_zero() == 0:
    print(1)

# --- 38. Variable reassignment (already declared) ---
counter = 99
print(counter)

# --- 39. Bool used in arithmetic context ---
bit = d
if bit:
    print(1)

# --- 40. Float in condition ---
ratio = 1.5
if ratio > 1.0:
    print(1)

# --- 41. Chained float arithmetic ---
pi     = 3.14159
radius = 5.0
area   = pi * radius * radius
print(area)

# --- 42. String variable (separate from the concat one above) ---
plain_str = "Good morning"
print(plain_str)

# --- 43. Integer used as loop bound from variable ---
limit = 4
sum2  = 0
for s in range(limit):
    sum2 += s
print(sum2)

# --- 44. with_comments result ---
wc = with_comments(9)
print(wc)

# --- 45. wrap result ---
wrap("hello")
