#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#define BUF_LEN 4096

char buf[BUF_LEN] __attribute__((aligned(__alignof__(struct inotify_event))));

int is_file_a_directory(char *file_name)
{
    struct stat sb;
    int res;
    int check;
    res = stat(file_name, &sb);
    if (res == -1)
    {
        printf("\nLa cartella richiesta non esiste!\nOra verrà creata!\n");
        check = mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (check == -1)
        {
            perror("mkdir failure");
            exit(EXIT_FAILURE);
        }
        return -1;
    }

    return (sb.st_mode & S_IFMT) == S_IFDIR;
}

void parent_process_signal_handler(int signum)
{
    pid_t child_pid;
    printf("[parent] parent_process_signal_handler\n");
    child_pid = wait(NULL);
}

static void show_inotify_event(struct inotify_event *i)
{
    printf("mask = ");
    if (i->mask & IN_MODIFY)
        printf("IN_MODIFY ");
}

int main(int argc, char *argv[])
{
    char *directory = "../src";
    int result = is_file_a_directory(directory);
    if (result != -1)
    {
        printf("\nLa cartella esiste già!\n");
        return 0;
    }
    int fd = open("../src/hello_world.c",
                  O_CREAT | O_WRONLY,
                  S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("open()");
        exit(EXIT_FAILURE);
    }
    char *text_to_write = "#include <stdio.h>\nint main() {\nprintf(\"Hello World!\");\n}";
    int ttw_length = strlen(text_to_write);
    if (write(fd, text_to_write, ttw_length) == -1)
    {
        perror("write()");
        exit(EXIT_FAILURE);
    }
    int fd2 = open("../src/output.txt",
                   O_CREAT | O_TRUNC | O_WRONLY,
                   S_IRUSR | S_IWUSR);
    if (fd2 == -1)
    {
        perror("open()");
        exit(EXIT_FAILURE);
    }
    //sighandling
    if (signal(SIGCHLD, parent_process_signal_handler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }
    //fase di monitoraggio
    int wd;
    int inotifyFd;
    int num_bytes_read;
    inotifyFd = inotify_init();
    if (inotifyFd == -1)
    {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }
    wd = inotify_add_watch(inotifyFd, "../src/hello_world.c", IN_MODIFY);
    if (wd == -1)
    {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }
    //rilevazione modifica
    for (;;)
    {
        num_bytes_read = read(inotifyFd, buf, BUF_LEN);
        if (num_bytes_read == 0)
        {
            printf("read() from inotify fd returned 0!");
            exit(EXIT_FAILURE);
        }
        if (num_bytes_read == -1)
        {
            if (errno == EINTR)
            {
                printf("read(): EINTR\n");
                continue;
            }
            else
            {
                perror("read()");
                exit(EXIT_FAILURE);
            }
        }
        printf("read %d bytes from inotify fd\n", num_bytes_read);

        // process all of the events in buffer returned by read()

        struct inotify_event *event;

        for (char *p = buf; p < buf + num_bytes_read;)
        {
            event = (struct inotify_event *)p;

            show_inotify_event(event);

            p += sizeof(struct inotify_event) + event->len;
            // event->len is length of (optional) file name
        }

        //esecuzione hello
        int gcc_value;
        switch (fork())
        {
        case 0:
            if (execlp("gcc", "gcc", "../src/hello_world.c", "-o", "hello", (char *)NULL) == -1)
            {
                perror("problema con execlp()");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);

            break;

        case -1:
            perror("problema con fork");
            exit(EXIT_FAILURE);

            break;

        default:
            gcc_value = wait(NULL);
            if (gcc_value == -1)
            {
                perror("wait problem");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);

            break;
        }
        pid_t child_pid;
        int length = 1;
        pid_t *pid_list = calloc(length, sizeof(pid_t));
        if (pid_list == NULL)
        {
            perror("calloc failure");
            exit(EXIT_FAILURE);
        }
        if (gcc_value == 0)
        {
            switch (fork())
            {
            case 0:
                child_pid = getpid();
                int fd3 = open("../src/output.txt",
                   O_WRONLY | O_APPEND,
                   S_IRUSR | S_IWUSR);
                if (fd3 == -1)
                {
                    perror("open()");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd3, STDOUT_FILENO) == -1)
                {
                    perror("problema con dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd3);
                char *new_arguments[] = {"hello", NULL};
                char *new_environment_variables[] = {NULL};
                if (execve("../hello", new_arguments, new_environment_variables) == -1)
                {
                    perror("execve failure");
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_SUCCESS);

                break;

            case -1:
                perror("problema con fork");
                exit(EXIT_FAILURE);

                break;

            default:
                pid_list[length - 1] = child_pid;
                length++;
                pid_list = realloc(pid_list, length * sizeof(pid_t));
                exit(0);

                break;
            }
        }
    }

    return 0;
}
