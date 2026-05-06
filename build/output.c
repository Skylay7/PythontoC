#include <stdio.h>
#include <math.h>

int add(int a, int b)
{
    return (a + b);
}

int scale(int value, int factor)
{
    return (value * factor);
}

const char *greet(const char *name)
{
    char msg[256];
    snprintf(msg, 256, "%s%s", "Hello, ", name);
    return msg;
}

void print_line(const char *label, int count)
{
    char result[256];
    snprintf(result, 256, "%s%s", label, " count");
    printf("%s\n", result);
    printf("%d\n", count);
}

int absolute(int n)
{
    if ((n < 0))
    {
        return (n * -(1));
    }
    else
    {
        return n;
    }
}

double double_float(double x)
{
    return (x * 2.0);
}

int commented_body(int n)
{
    int result = (n + 1);
    return result;
}

int main(void)
{
    char greeting[256];
    snprintf(greeting, 256, "%s%s", "Hello, ", "world");
    printf("%s\n", greeting);
    const char *name = "Alice";
    char __tmp_0[256];
    snprintf(__tmp_0, 256, "%s%s", "Welcome, ", name);
    printf("%s\n", __tmp_0);
    int counter = 0;
    counter += 5;
    counter -= 1;
    counter *= 2;
    counter /= 2;
    printf("%d\n", counter);
    int flag = 1;
    int other = 0;
    if ((flag && !(other)))
    {
        printf("%d\n", 1);
    }
    int score = 85;
    if ((score >= 90))
    {
        printf("%d\n", 4);
    }
    else if ((score >= 80))
    {
        printf("%d\n", 3);
    }
    else if ((score >= 70))
    {
        printf("%d\n", 2);
    }
    else
    {
        printf("%d\n", 1);
    }
    int i = 0;
    while ((i < 10))
    {
        if ((i == 3))
        {
            i += 1;
            continue;
        }
        if ((i == 7))
        {
            break;
        }
        i += 1;
    }
    int total = 0;
    for (int i = 0; i < 5; i++)
    {
        total += i;
    }
    printf("%d\n", total);
    add(3, 4);
    greet("Bob");
    int result = add(10, 20);
    printf("%d\n", result);
    int outer = add(add(1, 2), 3);
    printf("%d\n", outer);
    int x = 5;
    int neg = (x * -(1));
    printf("%d\n", neg);
    int b = !(flag);
    printf("%d\n", b);
    double pi = 3.14159;
    double radius = 2.0;
    double area = ((pi * radius) * radius);
    printf("%f\n", area);
    int val = commented_body(9);
    printf("%d\n", val);
    print_line("items", 42);
    return 0;
}
