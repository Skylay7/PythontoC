#include <stdio.h>
#include <math.h>

void say_hello(const char * person)
{
    char msg[256];
    snprintf(msg, 256, "%s%s", "Hello, ", person);
    printf("%s\n", msg);
}

int add(int x, int y)
{
    return (x + y);
}

double scale(double value)
{
    return (value * 2.0);
}

const char * make_label(const char * prefix)
{
    char result[256];
    snprintf(result, 256, "%s%s", "label: ", prefix);
    return result;
}

int absolute(int n)
{
    if ((n < 0)) {
        return (n * -(1));
    } else {
        return n;
    }
}

double double_float(double x)
{
    return (x * 2.0);
}

const char * wrap(const char * text)
{
    char result[256];
    snprintf(result, 256, "%s%s", "[", text);
    return result;
}

int get_zero()
{
    return 0;
}

int square(int n)
{
    return (n * n);
}

int with_comments(int n)
{
    int result = (n + 1);
    return result;
}

int main(void)
{
    int a = 1;
    double b = 2.5;
    const char * c = "hello";
    int d = 1;
    int e = 0;
    int sum_int = (a + 10);
    int diff = (a - 1);
    int prod = (a * 3);
    int quot = (a / 2);
    double promoted = (a + b);
    double chained = ((a + b) + 3.0);
    int idiv = (7 / 2);
    int mod = (7 % 3);
    int squared = pow(2, 8);
    int neg = -(a);
    int flag = !(d);
    int inv = !(e);
    int eq = (a == 1);
    int neq = (a != 2);
    int lt = (a < 5);
    int gt = (b > 1.0);
    int lte = (a <= 1);
    int gte = (b >= 2.5);
    int both = (d && !(e));
    int either = (e || d);
    int combo = ((a == 1) && (b > 2.0));
    char greeting[256];
    snprintf(greeting, 256, "%s%s", "Hello, ", "world");
    char label[256];
    snprintf(label, 256, "%s%s", "value: ", "42");
    const char * name = "Alice";
    char msg[256];
    snprintf(msg, 256, "%s%s", "Hi, ", name);
    char msg2[256];
    snprintf(msg2, 256, "%s%s", name, "!");
    int counter = 0;
    counter += 5;
    counter -= 1;
    counter *= 2;
    counter /= 2;
    printf("%d\n", a);
    printf("%f\n", b);
    printf("%s\n", c);
    printf("%d\n", d);
    printf("%d\n", sum_int);
    printf("%f\n", promoted);
    printf("%s\n", greeting);
    printf("%d %f\n", a, b);
    printf("%s %d\n", c, a);
    char __tmp_0[256];
    snprintf(__tmp_0, 256, "%s%s", "prefix: ", name);
    printf("%s\n", __tmp_0);
    printf("\n");
    int score = 91;
    if ((score >= 90)) {
        printf("%d\n", 1);
    } else if ((score >= 80)) {
        printf("%d\n", 2);
    } else if ((score >= 70)) {
        printf("%d\n", 3);
    } else {
        printf("%d\n", 4);
    }
    int x = 10;
    if ((x > 0)) {
        if ((x > 5)) {
            printf("%d\n", x);
        } else {
            printf("%d\n", 0);
        }
    }
    int i = 0;
    while ((i < 10)) {
        if ((i == 5)) {
            break;
        }
        i += 1;
    }
    printf("%d\n", i);
    int total = 0;
    int j = 0;
    while ((j < 6)) {
        j += 1;
        if ((j == 3)) {
            continue;
        }
        total += j;
    }
    printf("%d\n", total);
    int acc = 0;
    for (int k = 0; k < 5; k++) {
        acc += k;
    }
    printf("%d\n", acc);
    int outer_sum = 0;
    for (int p = 0; p < 3; p++) {
        for (int q = 0; q < 3; q++) {
            outer_sum += 1;
        }
    }
    printf("%d\n", outer_sum);
    int found = 0;
    for (int r = 0; r < 10; r++) {
        if ((r == 4)) {
            break;
        }
    }
    printf("%d\n", found);
    if (d) {
    }
    say_hello("Bob");
    add(1, 2);
    get_zero();
    int r1 = add(3, 4);
    printf("%d\n", r1);
    double r2 = scale(3);
    printf("%f\n", r2);
    make_label("world");
    int r4 = absolute(-(7));
    printf("%d\n", r4);
    double r5 = double_float(3);
    printf("%f\n", r5);
    int r6 = get_zero();
    printf("%d\n", r6);
    int r7 = add(add(1, 2), add(3, 4));
    printf("%d\n", r7);
    int r8 = square(square(2));
    printf("%d\n", r8);
    if ((get_zero() == 0)) {
        printf("%d\n", 1);
    }
    counter = 99;
    printf("%d\n", counter);
    int bit = d;
    if (bit) {
        printf("%d\n", 1);
    }
    double ratio = 1.5;
    if ((ratio > 1.0)) {
        printf("%d\n", 1);
    }
    double pi = 3.14159;
    double radius = 5.0;
    double area = ((pi * radius) * radius);
    printf("%f\n", area);
    const char * plain_str = "Good morning";
    printf("%s\n", plain_str);
    int limit = 4;
    int sum2 = 0;
    for (int s = 0; s < limit; s++) {
        sum2 += s;
    }
    printf("%d\n", sum2);
    int wc = with_comments(9);
    printf("%d\n", wc);
    wrap("hello");
    return 0;
}
