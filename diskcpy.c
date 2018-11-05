#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

void printErr(const char* str) {
    fprintf(stderr, "%s: (errno %d) %s\n", str, errno, strerror(errno));
}

char* readLine(const char* prompt) {
    if (prompt != NULL) {
        printf("%s", prompt);
    }

    size_t size = 16;
    char *s = malloc(size);
    size_t pos = 0;
    for (;;) {
        int ch = getchar();
        if (ch == EOF) {
            return NULL;
        } else if (ch == '\n') {
            s[pos] = '\0';
            return s;
        } else {
            if (pos + 1 == size) {
                size_t newSize = size * 2;
                char *newS = malloc(newSize);
                strncpy(newS, s, size);
                free(s);
                size = newSize;
                s = newS;
            }
            s[pos++] = (char)ch;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("THIS IS A TOY coping all non-zeroed blocks of a block device (Src) to another block device (Dst).\n"
               "\n"
               "Usage: diskcpy SRC DST\n"
               "\n"
               "Please make sure:\n"
               " 1) Dst is not smaller than Src.\n"
               " 2) Dst is all-zeroed.\n");
        return 1;
    }

    const int DefaultBufSize = 64 * 1024;
    int bufSize = DefaultBufSize;

    MENU:

    printf("Src: %s\n"
           "Dst: %s\n"
           "(Buffer/Block Size: %d)\n"
           "\n"
           "Please make sure:\n"
           " 1) Dst is not smaller than Src.\n"
           " 2) Dst is all-zeroed.\n"
           "\n",
           argv[1], argv[2], bufSize);

    char* strConfirm = readLine("Input 'YES' to start coping, or 'bs <SIZE>' to change buffer size: ");
    if (!strConfirm) {
        puts("Program exiting.");
        return 2;
    } else if(!strcmp(strConfirm, "YES")) {
        free(strConfirm);
    } else if(sscanf(strConfirm, "bs %d", &bufSize) == 1) {
        free(strConfirm);
        if (bufSize <= 0) {
            printf("invalid size.\n");
            bufSize = DefaultBufSize;
        }
        printf("\n");
        goto MENU;
    } else {
        free(strConfirm);
        puts("Program exiting.");
        return 2;
    }

    printf("\nStarting coping...\n");

    int src = open(argv[1], O_RDONLY);
    if (src == -1) {
        printErr("Error opening src file");
        return 1;
    }
    int dst = open(argv[2], O_WRONLY);
    if (dst == -1) {
        printErr("Error opening dst file");
        return 1;
    }

    printf("Files opened.\n");

    int64_t progRead = 0, progWrite = 0, totalWrite = 0, lastReport = 0;
    char *buf = malloc((size_t)bufSize);

    for (;;) {
        ssize_t r = read(src, buf, (size_t)bufSize);
        if (r == -1) {
            printErr("Error reading src file");
            return 1;
        } else if (r == 0) {
            printf("Read EOF.\ntotalR=%ld, totalW=%ld\n", progRead, totalWrite);
            break;
        }
        int needWrite = 0;
        for (int i = 0; i < r; i++) {
            if (buf[i] != 0) {
                needWrite = 1;
                break;
            }
        }
        if (needWrite) {
            if (progWrite != progRead) {
                if (progRead != lseek(dst, progRead, SEEK_SET)) {
                    printErr("Error seeking dst file");
                    return 1;
                }
                progWrite = progRead;
            }
            if (write(dst, buf, (size_t)r) != r) {
                printErr("Error writing to dst file");
                return 1;
            }
            progWrite += r;
            totalWrite += r;
        }
        progRead += r;

        if (progRead - lastReport >= 10 * 1024 * 1024) {
            printf("progR=%ld, progW=%ld, totalW=%ld\n", progRead, progWrite, totalWrite);
            lastReport = progRead;
        }
    }

    return 0;
}
