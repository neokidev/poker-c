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

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT      30000
#define MAX_NUM_PLAYERS      3
#define NUM_HAND_CARDS       5
#define NUM_CARDS           52


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

struct player {
    char name[20];
    int money;
    int hand[NUM_HAND_CARDS];
    bool changed_card[NUM_HAND_CARDS];
    char address[INET_ADDRSTRLEN];
    unsigned short port;
    enum PlayerStatus status;
};

int exec_read(int sock_fd, char *buffer, unsigned long buffer_size);
void exec_write(int sock_fd, char *buffer, size_t len);
void init_deck(int deck[]);
void shuffle_deck(int deck[]);
void print_deck(int deck[]);

int main ()
{
    int    n, nbytes;
    bool   flag;
    int    rc, on = 1;
    int    len;
    int    listen_fd = -1, conn_fd = -1;
    int    end_server = false, compress_array = false;
    int    close_conn;
    char   buffer[200];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    char   *addr;
    socklen_t addrlen;
    struct pollfd fds[MAX_NUM_PLAYERS + 1];
    int    fd_idx;
    int    nfds = 1, cur_nfds = 0;
    int    i, j;

    struct player pls[MAX_NUM_PLAYERS];
    int    pl_idx;
    struct player *pl_in_turn_p, *pl_next_turn_p;
    int    deck[NUM_CARDS];
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
        for (fd_idx = 0; fd_idx < cur_nfds; fd_idx++)
        {
            if (fds[fd_idx].revents == 0)
                continue;

            /*
            if (fds[fd_idx].revents != POLLIN)
            {
                printf("  Error! revents = %d\n", fds[fd_idx].revents);
                end_server = true;
                break;
            }*/

            if (fds[fd_idx].revents & POLLIN)
            {
                if (fds[fd_idx].fd == listen_fd)
                {
                    printf("  Listening socket is readable\n");

                    pl_idx = nfds - 1;

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

                    if (nfds <= MAX_NUM_PLAYERS)
                    {
                        fds[nfds].fd = conn_fd;
                        fds[nfds].events = POLLIN;

                        pls[pl_idx].money = 10000;
                        for (i = 0; i < NUM_HAND_CARDS; i++)
                        {
                            pls[pl_idx].hand[i] = -1;
                            pls[pl_idx].changed_card[i] = false;
                        }
                        inet_ntop(AF_INET, &client_addr.sin_addr, pls[pl_idx].address, INET_ADDRSTRLEN);
                        pls[pl_idx].port = ntohs(client_addr.sin_port);
                        pls[pl_idx].status = REGIST_NAME;

                        printf("  New player: %s:%d\n", pls[pl_idx].address, pls[pl_idx].port);

                        strcpy(buffer, "0あなたの名前を入力してください\n> \0");
                        exec_write(fds[nfds].fd, buffer, strlen(buffer) + 1);
                        nfds++;
                    }
                    else
                    {
                        strcpy(buffer, "1参加人数の上限を超えているので，参加できませんでした\n\0");
                        exec_write(conn_fd, buffer, strlen(buffer) + 1);
                        close(conn_fd);
                    }
                }
                else
                {
                    printf("  Descriptor %d is readable\n", fds[fd_idx].fd);

                    pl_idx = fd_idx - 1;

                    // close_conn = false;
                    switch (pls[pl_idx].status)
                    {
                        case REGIST_NAME:
                            nbytes = exec_read(fds[fd_idx].fd, pls[pl_idx].name, sizeof(pls[pl_idx].name));
                            printf("  %d bytes received\n", nbytes);
                            snprintf(buffer, sizeof(buffer), "ポーカーの世界へようこそ！%s さん！\n\0", pls[pl_idx].name);
                            exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                            pls[pl_idx].status = WAIT_PLAYER;
                            break;
                        case WAIT_PLAYER:
                            nbytes = exec_read(fds[fd_idx].fd, pls[pl_idx].name, sizeof(pls[pl_idx].name));
                            printf("  %d bytes received\n", nbytes);
                            flag = false;
                            for (int j = 0; j < MAX_NUM_PLAYERS; j++)
                            {
                                if (pls[j].status == REGIST_NAME)
                                {
                                    flag = true;
                                    break;
                                }
                            }

                            if (nfds <= MAX_NUM_PLAYERS || flag)
                            {
                                strcpy(buffer, "0プレイヤーが集まるのを待っています...\n\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                            }
                            else
                            {
                                strcpy(buffer, "1プレイヤーが揃いました！\n\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                pls[pl_idx].status = GAME_PREPARE;
                            }
                            break;
                    }

                    /*
                    if (close_conn)
                    {
                        close(fds[fd_idx].fd);
                        fds[fd_idx].fd = -1;
                        compress_array = true;
                    }
                    */
                }
            }
        }
        /*
        if (compress_array)
        {
            compress_array = false;
            for (i = 0; i < nfds; i++)
            {
                if (fds[fd_idx].fd == -1)
                {
                    for(j = i; j < nfds; j++)
                    {
                        fds[j].fd = fds[j+1].fd;
                    }
                    nfds--;
                }
            }
        }
        */
    }

    for (i = 0; i < nfds; i++)
    {
        if(fds[fd_idx].fd >= 0)
        close(fds[fd_idx].fd);
    }
    return 0;
}


void init_deck(int deck[]) {
    int i, size = NUM_CARDS;
    for (i = 0; i < size; i++) {
        deck[i] = i;
    }
}


void shuffle_deck(int deck[]) {
    int i, j, t;
    for (i = 0; i < NUM_CARDS; i++) {
        j = rand() % NUM_CARDS;
        t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
    }
}


void print_deck(int deck[]) {
    int i;
    for (i = 0; i < NUM_CARDS; i++) {
        printf("%d ", deck[i]);
    }
    printf("\n");
}


int exec_read(int fd, char *buffer, unsigned long buffer_size)
{
    int nbytes = read(fd, buffer, buffer_size);
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

    buffer[nbytes] = '\0';

    return nbytes;
}

void exec_write(int fd, char *buffer, size_t len)
{
    if (write(fd, buffer, len) < 0)
    {
        perror("  write() failed");
        exit(1);
    }
}
