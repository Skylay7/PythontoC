#include <stdio.h>
#include <math.h>

int test_inference(int base_score, double multiplier, const char * username, int unused_param)
{
    int final_score = (base_score + 50);
    double bonus = (multiplier * 1.25);
    const char * greeting = (username + " logged in.");
    printf("%d\n", final_score);
    printf("%f\n", bonus);
    printf("%s\n", greeting);
}

int main(void)
{
    test_inference(100, 2.0, "admin", 0);
    return 0;
}
