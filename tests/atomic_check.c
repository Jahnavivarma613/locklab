#include <stdatomic.h>
#include <stdio.h>

int main(void)
{
    _Atomic int integer = 0;
    _Atomic long long large_integer = 0;

    printf("Atomic int lock-free: %s\n",
           atomic_is_lock_free(&integer) ? "YES" : "NO");

    printf("Atomic long long lock-free: %s\n",
           atomic_is_lock_free(&large_integer) ? "YES" : "NO");

    return 0;
}
