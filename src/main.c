#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("========================================\n");
    printf("              LOCKLAB\n");
    printf("========================================\n");

    printf("\n[CO-1] Linux System Call Demonstration\n\n");
    printf("Process ID       : %ld\n", (long)getpid());
    printf("Parent Process ID: %ld\n", (long)getppid());

    const char message[] = "Message written using the write() system call.\n";
    size_t remaining = sizeof(message) - 1;
    const char *ptr = message;

    while (remaining > 0) {
        ssize_t written = write(STDOUT_FILENO, ptr, remaining);
        if (written < 0) {
            perror("write");
            return 1;
        }
        ptr += written;
        remaining -= (size_t)written;
    }

    printf("\nProgram will sleep for 2 seconds...\n");
    sleep(2);
    printf("Sleep completed.\n");

    return 0;
}
