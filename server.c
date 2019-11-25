#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_ADDR  "127.0.0.1"
#define SERVER_PORT  30000
#define MAX_NUM_PLAYERS  3
// #define NFDS            10

struct player {
    char name[20];
    int money;
    int hand[5];
    bool changed_card[5];
    char address[INET_ADDRSTRLEN];
    unsigned short port;
};


enum PlayerStatus {
    REGIST_NAME,
    WAIT_PLAYER,
    GAME_PREPARE,
    GAME_LOOK_FIRST_HAND,
    GAME_BEGINNING_OF_TURN,
    GAME_MY_TURN,
    GAME_START_CHANGE_CARD,
    GAME_SELECT_CHANGE_CARD,
    GAME_OTHER_PLAYER_TURN,
    GAME_END_OF_TURN,
    GAME_RESULT
};

void init_deck(int deck[]);
void shuffle_deck(int deck[]);
void print_deck(int deck[]);

int main ()
{
    int    n, nbytes;
    int    rc, on = 1;
    int    len;
    int    listen_fd = -1, conn_fd = -1;
    int    end_server = false, compress_array = false;
    int    close_conn;
    char   buffer[80];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    char *addr;
    socklen_t addrlen;
    struct pollfd fds[MAX_NUM_PLAYERS + 1];
    int    nfds = 1, cur_nfds = 0, i, j;

    struct player pl[MAX_NUM_PLAYERS];
    struct player *pl_in_turn_p, *pl_next_turn_p;
    int    deck[52];
    int    next_draw_idx;


    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() failed");
        exit(-1);
    }

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
    {
        perror("setsockopt() failed");
        close(listen_fd);
        exit(-1);
    }

    if (ioctl(listen_fd, FIONBIO, (char *)&on) < 0)
    {
        perror("ioctl() failed");
        close(listen_fd);
        exit(-1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind() failed");
        close(listen_fd);
        exit(-1);
    }

    if (listen(listen_fd, 32) < 0)
    {
        perror("listen() failed");
        close(listen_fd);
        exit(-1);
    }

    memset(fds, 0, sizeof(fds));

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;

    while (end_server == false)
    {
        printf("Waiting on poll()...\n");

        if ((n = poll(fds, nfds, -1)) < 0)
        {
            perror("  poll() failed");
            break;
        }

        if (n == 0)
        {
            printf("  poll() timed out.  End program.\n");
            break;
        }

        cur_nfds = nfds;
        for (i = 0; i < cur_nfds; i++)
        {
            if(fds[i].revents == 0)
                continue;

            if(fds[i].revents != POLLIN)
            {
                printf("  Error! revents = %d\n", fds[i].revents);
                end_server = true;
                break;
            }

            if (fds[i].fd == listen_fd)
            {
                printf("  Listening socket is readable\n");

                addrlen = sizeof(client_addr);
                conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);
                if (conn_fd < 0)
                {
                    if (errno != EWOULDBLOCK)
                    {
                        perror("  accept() failed");
                        end_server = true;
                    }
                    break;
                }

                printf("  New incoming connection - %d\n", conn_fd);

                fds[nfds].fd = conn_fd;
                fds[nfds].events = POLLIN;

                strcpy(buffer, "あなたの名前を入力してください\n> ");
                if (write(fds[nfds].fd, buffer, strlen(buffer)) < 0)
                {
                    perror("  write() failed");
                    close_conn = true;
                    break;
                }

                strcpy(pl[nfds-1].name, "Unknown");
                pl[nfds-1].money = 10000;
                for (i = 0; i < 5; i++)
                {
                    pl[nfds-1].hand[i] = -1;
                    pl[nfds-1].changed_card[i] = false;
                }

                inet_ntop(AF_INET, &client_addr.sin_addr, pl[nfds-1].address, INET_ADDRSTRLEN);

                pl[nfds-1].port = ntohs(client_addr.sin_port);
                printf("name:%s, address:%s, port:%d\n", pl[nfds-1].name, pl[nfds-1].address, pl[nfds-1].port);

                nfds++;
            }
            else
            {
                printf("  Descriptor %d is readable\n", fds[i].fd);

                close_conn = false;

                for (;;)
                {
                    nbytes = read(fds[i].fd, buffer, sizeof(buffer) - 1);
                    if (nbytes < 0)
                    {
                        if (errno != EWOULDBLOCK)
                        {
                            perror("  read() failed");
                            close_conn = true;
                        }
                        break;
                    }
                    if (nbytes == 0)
                    {
                        printf("  Connection closed\n");
                        close_conn = true;
                        break;
                    }
                    buffer[nbytes] = '\0';

                    printf("  %d bytes received\n", nbytes);
                    printf("あなたの名前は %s ですね！\n", buffer);

                    /*
                    rc = send(fds[i].fd, buffer, nbytes, 0);
                    if (rc < 0)
                    {
                        perror("  send() failed");
                        close_conn = true;
                        break;
                    }
                    */
                }

                if (close_conn)
                {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    compress_array = true;
                }


            }
        }
        if (compress_array)
        {
            compress_array = false;
            for (i = 0; i < nfds; i++)
            {
                if (fds[i].fd == -1)
                {
                    for(j = i; j < nfds; j++)
                    {
                        fds[j].fd = fds[j+1].fd;
                    }
                    nfds--;
                }
            }
        }
    }

    for (i = 0; i < nfds; i++)
    {
        if(fds[i].fd >= 0)
        close(fds[i].fd);
    }
    return 0;
}


void init_deck(int deck[]) {
    int i, size = 52;
    for (i = 0; i < size; i++) {
        deck[i] = i;
    }
}


void shuffle_deck(int deck[]) {
    int i, j, t;
    int size = 52;
    for (i = 0; i < size; i++) {
        j = rand() % size;
        t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
    }
}


void print_deck(int deck[]) {
    int i, size = 52;
    for (i = 0; i < size; i++) {
        printf("%d ", deck[i]);
    }
    printf("\n");
}
