#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t signal_received = 0;

static void handle_signal(int signo)
{
    signal_received = signo;
}

int main(void)
{
    printf("====================================\n");
    printf(" LOCKLAB - OS CONCEPT DEMONSTRATIONS\n");
    printf("====================================\n");

    /* CO-2: Process creation and synchronization */
    printf("\n[CO-2] fork() and waitpid()\n");
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        printf("Child process: PID=%ld, parent PID=%ld\n",
               (long)getpid(), (long)getppid());
        _exit(7);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status))
        printf("Parent: child exited with status %d\n",
               WEXITSTATUS(status));

    /* CO-3: Anonymous pipe */
    printf("\n[CO-3] Anonymous pipe IPC\n");
    int fds[2];

    if (pipe(fds) < 0) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        close(fds[0]);
        close(fds[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        close(fds[0]);
        const char msg[] = "Hello through an anonymous pipe";
        ssize_t n = write(fds[1], msg, sizeof(msg));
        close(fds[1]);
        _exit(n == (ssize_t)sizeof(msg) ? 0 : 1);
    }

    close(fds[1]);
    char buffer[128];
    ssize_t n = read(fds[0], buffer, sizeof(buffer) - 1);
    close(fds[0]);

    if (n < 0) {
        perror("pipe read");
        return EXIT_FAILURE;
    }
    buffer[n] = '\0';
    printf("Parent received: %s\n", buffer);

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    /* CO-3: POSIX signal handling */
    printf("\n[CO-3] Signal handler\n");
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    if (raise(SIGUSR1) != 0) {
        perror("raise");
        return EXIT_FAILURE;
    }

    printf("SIGUSR1 received: %s\n",
           signal_received == SIGUSR1 ? "YES" : "NO");

    /* CO-4: Dynamic memory allocation */
    printf("\n[CO-4] Dynamic memory allocation\n");
    int *numbers = malloc(5 * sizeof(*numbers));

    if (numbers == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 5; i++)
        numbers[i] = (i + 1) * 10;

    printf("Allocated values:");
    for (int i = 0; i < 5; i++)
        printf(" %d", numbers[i]);
    printf("\n");

    free(numbers);
    printf("Allocated memory released with free().\n");

    /* CO-5: File descriptor and file I/O */
    printf("\n[CO-5] File I/O system calls\n");
    const char *path = "/tmp/locklab_demo.txt";
    const char message[] = "LockLab file I/O demonstration\n";

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open for writing");
        return EXIT_FAILURE;
    }

    size_t remaining = sizeof(message) - 1;
    const char *ptr = message;

    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            perror("write file");
            close(fd);
            return EXIT_FAILURE;
        }
        ptr += written;
        remaining -= (size_t)written;
    }

    if (close(fd) < 0) {
        perror("close after writing");
        return EXIT_FAILURE;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open for reading");
        return EXIT_FAILURE;
    }

    char filebuf[128];
    ssize_t bytes = read(fd, filebuf, sizeof(filebuf) - 1);
    if (bytes < 0) {
        perror("read file");
        close(fd);
        return EXIT_FAILURE;
    }

    filebuf[bytes] = '\0';
    printf("Read from %s: %s", path, filebuf);

    if (close(fd) < 0) {
        perror("close after reading");
        return EXIT_FAILURE;
    }

    printf("\nDemonstrations completed.\n");
    return EXIT_SUCCESS;
}
