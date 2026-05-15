# test_type_change.py — should trigger "Variable type changed after first assignment"

x = 5
x = "hello"   # error: int -> string

y = 3.14
y = 10        # should be fine: float and int promote to float? actually int != float so error

def foo(a):
    a = "world"   # error: param defaults to int, then reassigned to string
    return a
