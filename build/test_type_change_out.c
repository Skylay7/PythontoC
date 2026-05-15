#include <stdio.h>
#include <math.h>

const char *foo(const char *a)
{
    const char *a = "world";
    return a;
}

int main(void)
{
    int x = 5;
    x = "hello";
    double y = 3.14;
    y = 10;
    return 0;
}
