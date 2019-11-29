#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>


#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT      30000
#define BUFFER_SIZE        128
#define MAX_NAME_LEN        30

int exec_read(int sock_fd, char *buffer, unsigned long buffer_size);
int exec_write(int sock_fd, char *buffer, size_t len);

int main()
{
    int    sock_fd, nbytes, len;
    char   flag, print_flag, gamed_over_flag;
    char   buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_ADDR, &servaddr.sin_addr) < 0)
    {
        perror("inet_pton() failed");
        exit(1);
    }

    if (connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect() failed");
        exit(1);
    }

    /* 名前登録処理 */
    nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
    flag = buffer[0];

    if (flag == '0')
    {
        printf("%s", &buffer[1]);

        /* 名前を標準入力から受け取る */
        buffer[MAX_NAME_LEN + 3] = '\0';
        for (;;)
        {
            /* 問題が発生した場合 */
            if (fgets(buffer, MAX_NAME_LEN + 2, stdin) == NULL) {
                printf("問題が発生したので，もう一度入力してください\n> ");
            }
            /* 何も入力しなかった場合 */
            else if (buffer[0] == '\n') {
                printf("名前には1文字以上必要です，もう一度入力してください\n> ");
            }
            /* MAX_NAME_LENよりも大きな文字列を入力した場合 */
            else if (buffer[strlen(buffer) - 1] != '\n') {
                printf("名前が長すぎます．もう一度入力してください\n> ");
                /* stdinのバッファに溜まっている文字列をすべて吐き出す */
                for (;;)
                {
                    fgets(buffer, BUFFER_SIZE, stdin);
                    len = strlen(buffer);
                    if (buffer[len - 1] == '\n')
                        break;
                }
            }
            else
            {
                len = strlen(buffer);
                buffer[len - 1] = '\0';
                break;
            }
        }

        exec_write(sock_fd, buffer, strlen(buffer) + 1);

        nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
    else if (flag == '1')
    {
        printf("%s", &buffer[1]);
        exit(0);
    }

    /* プレイヤー待機処理 */
    print_flag = false;
    for (;;)
    {
        strcpy(buffer, "0\0");
        exec_write(sock_fd, buffer, strlen(buffer) + 1);

        nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
        flag = buffer[0];

        if (flag == '0')
        {
            if (!print_flag)
            {
                printf("%s", &buffer[1]);
                print_flag = true;
            }
        }
        else if (flag == '1')
        {
            printf("%s", &buffer[1]);
            break;
        }
        else
        {
            perror("  implementation error (wait player)");
            exit(1);
        }
    }

    /* ゲームの準備中の処理 */
    for (;;)
    {
        strcpy(buffer, "0\0");
        exec_write(sock_fd, buffer, strlen(buffer) + 1);

        nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
        flag = buffer[0];

        if (flag == '0')
        {
            continue;
        }
        else if (flag == '1')
        {
            printf("%s", &buffer[1]);
            break;
        }
        else
        {
            perror("  implementation error (game prepare)");
            exit(1);
        }
    }

    /* 手札確認処理 */
    strcpy(buffer, "0\0");
    exec_write(sock_fd, buffer, strlen(buffer) + 1);
    nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
    printf("%s", buffer);

    /* ターンの処理 */
    for (;;)
    {
        strcpy(buffer, "0\0");
        exec_write(sock_fd, buffer, strlen(buffer) + 1);

        nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
        flag = buffer[0];

        if (flag == '0')
        {
            continue;
        }
        /* 手番のプレイヤーの処理 */
        else if (flag == '1')
        {
            strcpy(buffer, "0\0");
            exec_write(sock_fd, buffer, strlen(buffer) + 1);

            nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
            printf("%s", buffer);

            /* カードの交換処理 */
            for (;;)
            {
                strcpy(buffer, "0\0");
                exec_write(sock_fd, buffer, strlen(buffer) + 1);

                nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
                flag = buffer[0];
                printf("%s", &buffer[1]);

                /* 名前を標準入力から受け取る */
                for (;;)
                {
                    /* 問題が発生した場合 */
                    if (fgets(buffer, 3, stdin) == NULL) {
                        printf("問題が発生したので，もう一度入力してください\n> ");
                    }
                    /* 何も入力しなかった場合 */
                    else if (buffer[0] == '\n') {
                        printf("何も入力していません，もう一度入力してください\n> ");
                    }
                    /* MAX_NAME_LENよりも大きな文字列を入力した場合 */
                    else if (buffer[strlen(buffer) - 1] != '\n') {
                        printf("入力が正しくありません．もう一度入力してください\n> ");
                        /* stdinのバッファに溜まっている文字列をすべて吐き出す */
                        for (;;)
                        {
                            fgets(buffer, BUFFER_SIZE, stdin);
                            len = strlen(buffer);
                            if (buffer[len - 1] == '\n')
                                break;
                        }
                    }
                    /* 選択肢に無い入力をした場合 */
                    else if (!(buffer[0] >= '0' && buffer[0] <= flag))
                    {
                        printf("入力が正しくありません．もう一度入力してください\n> ");
                    }
                    else
                    {
                        len = strlen(buffer);
                        buffer[len - 1] = '\0';
                        break;
                    }
                }

                exec_write(sock_fd, buffer, strlen(buffer) + 1);

                nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
                flag = buffer[0];

                if (flag == '0')
                {
                    printf("%s", &buffer[1]);
                }
                else if (flag == '1')
                {
                    printf("%s", &buffer[1]);
                    break;
                }
                else
                {
                    perror("  implementation error (game swap card)");
                }
            }
        }
        /* 手番ではないプレイヤーの処理 */
        else if (flag == '2')
        {
            print_flag = false;
            for (;;)
            {
                strcpy(buffer, "0\0");
                exec_write(sock_fd, buffer, strlen(buffer) + 1);

                nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
                flag = buffer[0];

                if (flag == '0')
                {
                    if (!print_flag)
                    {
                        printf("%s", &buffer[1]);
                        print_flag = true;
                    }
                }
                else if (flag == '1')
                {
                    break;
                }
                else
                {
                    perror("  implementation error (ohter player)");
                    exit(1);
                }
            }
        }
        else
        {
            perror("  implementation error (game biginning of turn)");
            exit(1);
        }

        /* ターン終了時の処理 */
        for (;;)
        {
            strcpy(buffer, "0\0");
            exec_write(sock_fd, buffer, strlen(buffer) + 1);

            nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
            flag = buffer[0];

            if (flag == '0')
            {
                continue;
            }
            else if (flag == '1')
            {
                gamed_over_flag = false;
                break;
            }
            else if (flag == '2')
            {
                gamed_over_flag = true;
                break;
            }
            else
            {
                perror("  implementation error (game end of turn)");
                exit(1);
            }
        }

        if (gamed_over_flag)
        {
            for (;;)
            {
                strcpy(buffer, "0\0");
                exec_write(sock_fd, buffer, strlen(buffer) + 1);

                nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
                flag = buffer[0];

                if (flag == '0')
                {
                    continue;
                }
                else if (flag == '1')
                {
                    printf("%s", &buffer[1]);
                    break;
                }
            }
            break;
        }
    }
}


int exec_read(int fd, char *buffer, unsigned long buffer_size)
{
    int nbytes = 0;

    nbytes = read(fd, buffer, buffer_size);
    if (nbytes < 0)
    {
        if (errno != EWOULDBLOCK)
        {
            perror("  read() failed");
            close(fd);
            exit(1);
        }
    }

    if (nbytes == 0)
    {
        printf("  Connection closed\n");
        close(fd);
    }

    return nbytes;
}


int exec_write(int fd, char *buffer, size_t len)
{
    int nbytes = 0;

    nbytes = write(fd, buffer, len);
    if (nbytes < 0)
    {
        perror("  write() failed");
        exit(1);
    }

    return nbytes;
}
