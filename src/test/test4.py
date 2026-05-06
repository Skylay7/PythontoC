# test4.py — edge case coverage

# 1. Return type: int (explicit)
def add(a, b):
    return a + b

# 2. Return type: float (int+float promotion)
def scale(value, factor):
    return value * factor

# 3. Return type: string (string concat in return)
def greet(name):
    msg = "Hello, " + name
    return msg

# 4. Void function (no return statement)
def print_line(label, count):
    result = label + " count"
    print(result)
    print(count)

# 5. Multiple return paths — same type, should be STYPE_INT
def absolute(n):
    if n < 0:
        return n * -1
    else:
        return n

# 6. Param refinement: multiplier becomes float from float literal
def double_float(x):
    return x * 2.0

# 7. String concat assigned to variable, then printed
greeting = "Hello, " + "world"
print(greeting)

# 8. String concat passed directly to print (temp buffer case)
name = "Alice"
print("Welcome, " + name)

# 9. Compound assignments
counter = 0
counter += 5
counter -= 1
counter *= 2
counter /= 2
print(counter)

# 10. Boolean operations in condition
flag = True
other = False
if flag and not other:
    print(1)

# 11. Nested if/elif/else
score = 85
if score >= 90:
    print(4)
elif score >= 80:
    print(3)
elif score >= 70:
    print(2)
else:
    print(1)

# 12. While loop with break and continue
i = 0
while i < 10:
    if i == 3:
        i += 1
        continue
    if i == 7:
        break
    i += 1

# 13. For loop
total = 0
for i in range(5):
    total += i
print(total)

# 14. Bare function call statement (no assignment)
add(3, 4)
greet("Bob")

# 15. Function call result assigned to variable
result = add(10, 20)
print(result)

# 16. Nested function calls as arguments
outer = add(add(1, 2), 3)
print(outer)

# 17. Unary minus and not
x = 5
neg = x * -1
print(neg)
b = not flag
print(b)

# 18. Float arithmetic
pi = 3.14159
radius = 2.0
area = pi * radius * radius
print(area)

# 19. Comment-only lines inside function bodies (parser regression test)
def commented_body(n):
    # this is a comment
    result = n + 1
    # another comment
    return result

val = commented_body(9)
print(val)

# 20. Void function called as statement (no assignment)
print_line("items", 42)
