#include <stdio.h>
#include <math.h>

int factorial(int n)
{
    int result = 1;
    int i = 1;
    while ((i <= n))
    {
        result *= i;
        i += 1;
    }
    return result;
}

int is_even(int n)
{
    if ((n == 0))
    {
        return 1;
    }
    int r = (n - 2);
    if ((r < 0))
    {
        return 0;
    }
    return is_even(r);
}

int sum_range(int limit)
{
    int total = 0;
    for (int i = 0; i < limit; i++)
    {
        total += i;
    }
    return total;
}

int main(void)
{
    int x = 5;
    double y = 3.14;
    const char *name = "Alice";
    int flag = 1;
    if ((x > 0))
    {
        printf("%d\n", x);
    }
    else if ((x == 0))
    {
        printf("%d\n", 0);
    }
    else
    {
        printf("%d\n", -(1));
    }
    int count = 0;
    while ((count < x))
    {
        if ((count == 2))
        {
            count += 1;
            continue;
        }
        printf("%d\n", count);
        count += 1;
    }
    for (int i = 0; i < 10; i++)
    {
        if ((i == 7))
        {
            break;
        }
    }
    int f = factorial(x);
    printf("%d\n", f);
    int s = sum_range(x);
    printf("%d\n", s);
    int a = 10;
    int b = 4;
    int c = (a + b);
    int d = (a - b);
    int e = !(flag);
    printf("%d\n", c);
    printf("%d\n", d);
    printf("%d\n", e);
    return 0;
}
