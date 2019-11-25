#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>


#define SERVER_ADDR  "127.0.0.1"
#define SERVER_PORT  30000


int main()
{
    int    sock_fd, nbytes;
    char   buffer[80];
    struct sockaddr_in servaddr;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

    nbytes = read(sock_fd, buffer, sizeof(buffer));
    if (nbytes < 0)
    {
        if (errno != EWOULDBLOCK)
        {
            perror("  read() failed");
            exit(1);
        }
    }

    if (nbytes == 0)
    {
        printf("  Connection closed\n");
    }

    printf("%s", buffer);

    strcpy(buffer, "てすと");
    printf("buffer: %s", buffer);
    if (write(sock_fd, buffer, strlen(buffer)) < 0)
    {
        perror("  write() failed");
        exit(1);
    }
}