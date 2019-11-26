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


#define SERVER_ADDR  "127.0.0.1"
#define SERVER_PORT  30000

int exec_read(int sock_fd, char *buffer, unsigned long buffer_size);
void exec_write(int sock_fd, char *buffer, size_t strlen);

int main()
{
    int    sock_fd, nbytes;
    char   flag, print_flag;
    char   buffer[80], stdin_buffer[80];
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

        fgets(stdin_buffer, sizeof(stdin_buffer), stdin);
        sscanf(stdin_buffer, "%s", buffer);
        exec_write(sock_fd, buffer, strlen(buffer) + 1);

        nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
    else if (flag == '1')
    {
        printf("%s", &buffer[1]);
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
            if (!print_flag) {
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
            break;
        }
        /* 手番ではないプレイヤーの処理 */
        else if (flag == '2')
        {
            strcpy(buffer, "0\0");
            exec_write(sock_fd, buffer, strlen(buffer) + 1);

            nbytes = exec_read(sock_fd, buffer, sizeof(buffer));
            printf("%s", buffer);
            break;
        }
        else
        {
            perror("  implementation error (game biginning of turn)");
            printf("recieved message: %s", buffer);
            exit(1);
        }
    }

    /* ちょっとした時間稼ぎ */
    for (int j = 0; j < 100000; j++) {
        for (int k = 0; k < 10000; k++) {
            int l = 0;
        }
    }
    printf("Done.\n");
}

int exec_read(int sock_fd, char *buffer, unsigned long buffer_size)
{
    int nbytes = read(sock_fd, buffer, buffer_size);
    if (nbytes < 0)
    {
        if (errno != EWOULDBLOCK)
        {
            perror("  read() failed");
            close(sock_fd);
            exit(1);
        }
    }

    if (nbytes == 0)
    {
        printf("  Connection closed\n");
        close(sock_fd);
    }

    buffer[nbytes] = '\0';

    return nbytes;
}

void exec_write(int sock_fd, char *buffer, size_t len)
{
    if (write(sock_fd, buffer, len) < 0)
    {
        perror("  write() failed");
        exit(1);
    }
}
