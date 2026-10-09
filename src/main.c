#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("========================================\n");
    printf("              LOCKLAB\n");
    printf("========================================\n");

    printf("\n[CO-1] Linux System Call Demonstration\n\n");

    printf("Process ID       : %d\n", getpid());
    printf("Parent Process ID: %d\n", getppid());

    const char message[] = "Message written using the write() system call.\n";
    write(STDOUT_FILENO, message, sizeof(message) - 1);

    printf("\nProgram will sleep for 10 seconds...\n");
    sleep(10);

    printf("Sleep completed.\n");

    return 0;
}
