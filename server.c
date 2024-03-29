#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>


#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT      30000
#define BUFFER_SIZE        128
#define MAX_NUM_PLAYERS      3
#define NUM_HAND_CARDS       5
#define NUM_CARDS           52
#define MAX_NAME_LEN        30


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
    char name[MAX_NAME_LEN + 1];
    int money;
    int hand[NUM_HAND_CARDS];
    bool changed_card[NUM_HAND_CARDS];
    char address[INET_ADDRSTRLEN];
    unsigned short port;
    enum PlayerStatus status;
};


struct deck {
    int cards[NUM_CARDS];
    int next_draw_idx;
};

static char hand_str[19];
static char card_str[4];

bool is_same_player(struct player pl1, struct player pl2);
char *hand_to_str(int hand[]);
char *card_to_str(int card);
char card_suit(int card);
int card_number(int card);
void change_card(struct player *pl_p, struct deck *d_p, int card_idx);
void draw_cards(int hand[], struct deck *d_p);
void init_deck(struct deck *d);
void shuffle_deck(struct deck *d);
void print_deck(struct deck *d);
int exec_read(int sock_fd, char *buffer, unsigned long buffer_size);
int exec_write(int sock_fd, char *buffer, size_t len);
int compareInt_asc(const void* a, const void* b);
int compareInt_desc(const void* a, const void* b);

/*
役の強さのランク
1.ストレートフラッシュ
2.フォーカード
3.フルハウス
4.フラッシュ
5.ストレート
6.スリーカード
7.ツーペア
8.ワンペア
9.ハイカード
*/
// 役の強さのランクを返す関数
int judge_hand(int hand[]);

// 勝利したプレイヤーの配列を変更する
void judge_winners(struct player *pls, bool *winners_player);
// judge_poker

int main ()
{
    int    n, nbytes;
    bool   flag;
    int    listen_fd = -1, conn_fd = -1;
    int    end_server = false;
    char   buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    struct pollfd fds[MAX_NUM_PLAYERS + 1];
    int    fd_idx;
    int    nfds = 1, cur_nfds = 0;
    int    i, j;

    struct player pls[MAX_NUM_PLAYERS];
    int    pl_idx;
    struct player *pl_in_turn_p, *pl_next_turn_p, *pl_dealer_p;
    struct deck deck1;
    bool   winners[MAX_NUM_PLAYERS];


    /* rand()の初期値が実行の度に変わるようにする */
    srand((unsigned)time(NULL));

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() failed");
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

    printf("サーバが起動しました\n");
    while (end_server == false)
    {
        if ((n = poll(fds, nfds, -1)) < 0)
        {
            perror("  poll() failed");
            break;
        }

        cur_nfds = nfds;
        for (fd_idx = 0; fd_idx < cur_nfds; fd_idx++)
        {
            if (fds[fd_idx].revents == 0)
                continue;

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
                        snprintf(buffer, sizeof(buffer), "1参加人数の上限を超えたため，参加できませんでした\n");
                        nbytes = exec_write(conn_fd, buffer, strlen(buffer) + 1);
                        printf("nbytes: %d\n", nbytes);
                        close(conn_fd);
                    }
                }
                else
                {
                    printf("  Descriptor %d is readable\n", fds[fd_idx].fd);

                    pl_idx = fd_idx - 1;

                    nbytes = exec_read(fds[fd_idx].fd, buffer, sizeof(buffer));

                    switch (pls[pl_idx].status)
                    {
                        case REGIST_NAME:
                            strncpy(pls[pl_idx].name, buffer, strlen(buffer) + 1);
                            snprintf(buffer, sizeof(buffer), "ポーカーの世界へようこそ！%s さん！\n", pls[pl_idx].name);
                            exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                            pls[pl_idx].status = WAIT_PLAYER;
                            break;
                        case WAIT_PLAYER:
                            flag = false;
                            for (j = 0; j < MAX_NUM_PLAYERS; j++)
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
                        case GAME_PREPARE:
                            flag = false;
                            for (i = 0; i < MAX_NUM_PLAYERS; i++)
                            {
                                if (pls[i].status == REGIST_NAME || pls[i].status == WAIT_PLAYER ||
                                    pls[i].status == GAME_RESULT)
                                {
                                    flag = true;
                                    break;
                                }
                            }

                            if (flag)
                            {
                                strcpy(buffer, "0\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                            }
                            else
                            {
                                /* 以下の処理を１回だけ行うための条件 */
                                if (fd_idx == 1)
                                {
                                    pl_in_turn_p = NULL;
                                    pl_next_turn_p = &pls[pl_idx];
                                    pl_dealer_p = &pls[MAX_NUM_PLAYERS - 1];

                                    init_deck(&deck1);
                                    shuffle_deck(&deck1);

                                    for (i = 0; i < MAX_NUM_PLAYERS; i++)
                                    {
                                        draw_cards(pls[i].hand, &deck1);

                                        for (j = 0; j < NUM_HAND_CARDS; j++)
                                        {
                                            pls[i].changed_card[j] = false;
                                        }
                                        // printf("\n");

                                        winners[i] = false;
                                    }
                                }

                                strcpy(buffer, "1ゲームを開始します！\n\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                pls[pl_idx].status = GAME_LOOK_FIRST_HAND;
                            }
                            break;
                        case GAME_LOOK_FIRST_HAND:
                            flag = false;
                            for (i = 0; i < MAX_NUM_PLAYERS; i++)
                            {
                                if (pls[i].status == GAME_PREPARE)
                                {
                                    flag = true;
                                    break;
                                }
                            }

                            if (!flag)
                            {
                                snprintf(buffer, sizeof(buffer), "山札からカードを5枚引きます\n%s\n", hand_to_str(pls[pl_idx].hand));
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                pls[pl_idx].status = GAME_BEGINNING_OF_TURN;
                            }
                            break;
                        case GAME_BEGINNING_OF_TURN:
                            flag = false;
                            for (i = 0; i < MAX_NUM_PLAYERS; i++)
                            {
                                if (pls[i].status == GAME_LOOK_FIRST_HAND || pls[i].status == GAME_END_OF_TURN)
                                {
                                    flag = true;
                                    break;
                                }
                            }

                            if (flag)
                            {
                                strcpy(buffer, "0\n\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                            }
                            else
                            {
                                if (pl_in_turn_p != pl_next_turn_p)
                                    pl_in_turn_p = pl_next_turn_p;

                                if (&pls[pl_idx] == pl_in_turn_p)
                                {
                                    strcpy(buffer, "1\n\0");
                                    exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                    pls[pl_idx].status = GAME_MY_TURN;
                                }
                                else
                                {
                                    strcpy(buffer, "2\n\0");
                                    exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                    pls[pl_idx].status = GAME_OTHER_PLAYER_TURN;
                                }
                            }
                            break;
                        case GAME_MY_TURN:
                            strcpy(buffer, "あなたの番です\n\0");
                            exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                            pls[pl_idx].status = GAME_START_CHANGE_CARD;
                            break;
                        case GAME_START_CHANGE_CARD:
                            strcpy(buffer, "0交換するカードを選んでください\n0. 交換しない\n");
                            j = 0;
                            for (i = 0; i < NUM_HAND_CARDS; i++)
                            {
                                if (!pls[pl_idx].changed_card[i])
                                {
                                    j++;
                                    snprintf(buffer, sizeof(buffer), "%s%d. %s\n",
                                             buffer, j, card_to_str(pls[pl_idx].hand[i]));
                                }
                            }
                            snprintf(buffer, sizeof(buffer), "%s> ", buffer);
                            buffer[0] = '0' + j;

                            nbytes += exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                            pls[pl_idx].status = GAME_SELECT_CHANGE_CARD;
                            break;
                        case GAME_SELECT_CHANGE_CARD:
                            if (buffer[0] == '0')
                            {
                                /* カードの交換を終了する */
                                snprintf(buffer, sizeof(buffer), "1手札の交換を終了します\n%s\n", hand_to_str(pls[pl_idx].hand));
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                pls[pl_idx].status = GAME_END_OF_TURN;
                            }
                            else
                            {
                                /* カードを交換する */
                                j = 1;
                                for (i = 0; i < NUM_HAND_CARDS; i++)
                                {
                                    if (pls[pl_idx].changed_card[i])
                                        continue;

                                    if (buffer[0] == '0' + j)
                                    {
                                        change_card(&pls[pl_idx], &deck1, i);
                                        snprintf(buffer, sizeof(buffer), "0%d番目のカードを交換します\n", i + 1);
                                        exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                                        break;
                                    }
                                    j++;
                                }
                                pls[pl_idx].status = GAME_START_CHANGE_CARD;
                            }
                            break;
                        case GAME_OTHER_PLAYER_TURN:
                            if (pl_in_turn_p->status == GAME_BEGINNING_OF_TURN || pl_in_turn_p->status == GAME_MY_TURN ||
                                pl_in_turn_p->status == GAME_START_CHANGE_CARD || pl_in_turn_p->status == GAME_SELECT_CHANGE_CARD)
                            {
                                snprintf(buffer, sizeof(buffer), "0%s さんの番です\n", pl_in_turn_p->name);
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                            }
                            else if (pl_in_turn_p->status == GAME_END_OF_TURN)
                            {
                                strcpy(buffer, "1\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                pls[pl_idx].status = GAME_END_OF_TURN;
                            }
                            else
                            {
                                perror("  implementation error (ohter player)");
                                exit(1);
                            }
                            break;
                        case GAME_END_OF_TURN:
                            flag = false;
                            for (i = 0; i < MAX_NUM_PLAYERS; i++)
                            {
                                if (pls[i].status == GAME_MY_TURN || pls[i].status == GAME_OTHER_PLAYER_TURN ||
                                    pls[i].status == GAME_START_CHANGE_CARD || pls[i].status == GAME_SELECT_CHANGE_CARD)
                                {
                                    flag = true;
                                    break;
                                }
                            }

                            if (flag)
                            {
                                strcpy(buffer, "0\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                            }
                            else
                            {
                                /* 現在の手番のプレイヤーがディーラーの場合 */
                                if (pl_in_turn_p == pl_dealer_p)
                                {
                                    strcpy(buffer, "2\0");
                                    exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                    pls[pl_idx].status = GAME_RESULT;
                                }
                                /* 現在の手番のプレイヤーがディーラーではない場合 */
                                else
                                {
                                    if (&pls[pl_idx] == pl_in_turn_p)
                                    {
                                        pl_next_turn_p = &pls[(pl_idx + 1) % MAX_NUM_PLAYERS];
                                    }
                                    strcpy(buffer, "1\0");
                                    exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                    pls[pl_idx].status = GAME_BEGINNING_OF_TURN;
                                }
                            }
                            break;
                        case GAME_RESULT:
                            flag = false;
                            for (i = 0; i < MAX_NUM_PLAYERS; i++)
                            {
                                if (pls[i].status == GAME_END_OF_TURN)
                                {
                                    flag = true;
                                    break;
                                }
                            }

                            if (flag)
                            {
                                strcpy(buffer, "0\0");
                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);
                            }
                            else
                            {
                                bool winners_player[MAX_NUM_PLAYERS];
                                judge_winners(pls, winners_player);

                                for (i = 0; i < MAX_NUM_PLAYERS; i++)
                                {
                                    if (winners_player[i] == true)
                                    {
                                        snprintf(buffer, sizeof(buffer), "1%s%s\t[%s] Win!! \n", buffer+1, pls[i].name, hand_to_str(pls[i].hand));
                                    }
                                    else
                                    {
                                        snprintf(buffer, sizeof(buffer), "1%s%s\t[%s] Lose... \n", buffer+1, pls[i].name, hand_to_str(pls[i].hand));
                                    }
                                }

                                exec_write(fds[fd_idx].fd, buffer, strlen(buffer) + 1);

                                pls[pl_idx].status = GAME_RESULT + 1;
                            }
                            break;
                    }

                    printf("    player status: ");
                    for (i = 0; i < MAX_NUM_PLAYERS; i++)
                    {
                        printf("%d ", pls[i].status);
                    }
                    printf("\n");
                }
            }
        }
        usleep(100000);
    }
}



bool is_same_player(struct player pl1, struct player pl2)
{
    if (strcmp(pl1.address, pl2.address) == 0 && pl1.port == pl2.port)
        return true;
    return false;
}


char *hand_to_str(int hand[])
{
    int i, str_idx = 0;
    int number;
    char suit;

    for (i = 0; i < NUM_HAND_CARDS; i++)
    {
        suit = card_suit(hand[i]);
        number = card_number(hand[i]);

        hand_str[str_idx] = suit;
        str_idx++;

        switch (number) {
            case 1:
                hand_str[str_idx] = 'A';
                str_idx++;
                break;
            case 10:
                hand_str[str_idx] = '1';
                str_idx++;
                hand_str[str_idx] = '0';
                str_idx++;
                break;
            case 11:
                hand_str[str_idx] = 'J';
                str_idx++;
                break;
            case 12:
                hand_str[str_idx] = 'Q';
                str_idx++;
                break;
            case 13:
                hand_str[str_idx] = 'K';
                str_idx++;
                break;
            default:
                hand_str[str_idx] = '0' + number;
                str_idx++;
        }

        if (i < NUM_HAND_CARDS - 1)
        {
            hand_str[str_idx] = ' ';
            str_idx++;
        }
        else
        {
            hand_str[str_idx] = '\0';
        }
    }

    return hand_str;
}

char *card_to_str(int card)
{
    int str_idx = 1;
    int n;

    card_str[0] = card_suit(card);

    n = card_number(card);

    switch (n) {
        case 1:
            card_str[str_idx] = 'A';
            str_idx++;
            break;
        case 10:
            card_str[str_idx] = '1';
            str_idx++;
            card_str[str_idx] = '0';
            str_idx++;
            break;
        case 11:
            card_str[str_idx] = 'J';
            str_idx++;
            break;
        case 12:
            card_str[str_idx] = 'Q';
            str_idx++;
            break;
        case 13:
            card_str[str_idx] = 'K';
            str_idx++;
            break;
        default:
            card_str[str_idx] = '0' + n;
            str_idx++;
    }

    card_str[str_idx] = '\0';

    return card_str;
}

char card_suit(int card)
{
    char suit;
    switch (card / 13)
    {
        case 0:
            suit = 's';
            break;
        case 1:
            suit = 'h';
            break;
        case 2:
            suit = 'd';
            break;
        case 3:
            suit = 'c';
            break;
    }
    return suit;
}


int card_number(int card)
{
    return card % 13 + 1;
}


void change_card(struct player *pl_p, struct deck *d_p, int card_idx)
{
    pl_p->hand[card_idx] = d_p->cards[d_p->next_draw_idx];
    pl_p->changed_card[card_idx] = true;
    d_p->next_draw_idx++;
}


void draw_cards(int hand[], struct deck *d_p)
{
    int i;
    for (i = 0; i < NUM_HAND_CARDS; i++)
    {
        hand[i] = d_p->cards[d_p->next_draw_idx];
        d_p->next_draw_idx++;
    }
}


void init_deck(struct deck *d_p)
{
    int i;
    for (i = 0; i < NUM_CARDS; i++)
    {
        d_p->cards[i] = i;
    }
    d_p->next_draw_idx = 0;
}


void shuffle_deck(struct deck *d_p)
{
    int i, j, t;
    for (i = 0; i < NUM_CARDS; i++)
    {
        j = rand() % NUM_CARDS;
        t = d_p->cards[i];
        d_p->cards[i] = d_p->cards[j];
        d_p->cards[j] = t;
    }
}


void print_deck(struct deck *d_p)
{
    int i;
    for (i = 0; i < NUM_CARDS; i++) {
        printf("%d ", d_p->cards[i]);
    }
    printf("\n");
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


int compareInt_asc(const void* a, const void* b)
{
    return *(int*)a - *(int*)b;
}

int compareInt_desc(const void* a, const void* b)
{
    return *(int*)b - *(int*)a;
}

int judge_hand(int hand[])
{
    int hand_num[NUM_HAND_CARDS] = {0};
    int hand_marks[NUM_HAND_CARDS] = {0};

    for (int i = 0; i < NUM_HAND_CARDS; i++)
    {
        hand_num[i] = card_number(hand[i]);
        hand_marks[i] = card_suit(hand[i]);
    }

    qsort(hand_num, NUM_HAND_CARDS, sizeof(int), compareInt_asc);

    // フラッシュかどうかのフラグを変更する
    int top_mark = hand_marks[0];
    int flash_flag = 1;
    for (int i = 1; i < NUM_HAND_CARDS; i++)
    {
        if (top_mark != hand_marks[i])
        {
            flash_flag = 0;
            break;
        }
    }

    // ストレートかどうかのフラグを変更する
    int top_num = hand_num[0];
    int straight_flag = 1;
    for (int i = 1; i < NUM_HAND_CARDS; i++)
    {
        if (top_num == 1 && hand_num[i] > 9)
        {
            top_num = 9;
        }
        top_num++;
        if (hand_num[i] != top_num)
        {
            straight_flag = 0;
            break;
        }
    }

    // ストレートフラッシュ判定(ランク:1)
    if (flash_flag == 1 && straight_flag == 1)
    {
        return 1;
    }

    // 重複する数字の判定
    int hand_dup[NUM_HAND_CARDS] = {0};
    int dup_index = 0;
    for (int i = 0; i < NUM_HAND_CARDS; i++)
    {
        int dup_count = 1;
        for (int j = (i+1); j < NUM_HAND_CARDS; j++)
        {
            if (hand_num[i] != hand_num[j])
            {
                break;
            }
            dup_count++;
        }
        hand_dup[dup_index] = dup_count;
        dup_index++;
        i += dup_count-1;
    }

    qsort(hand_dup, dup_index, sizeof(int), compareInt_desc);

    // フォーカード判定(ランク:2)
    if (hand_dup[0] == 4)
    {
        return 2;
    }

    // フルハウス判定(ランク:3)
    if (hand_dup[0] == 3 && hand_dup[1] == 2)
    {
        return 3;
    }

    // フラッシュ判定(ランク:4)
    if (flash_flag == 1)
    {
        return 4;
    }

    // ストレート判定(ランク:5)
    if (straight_flag == 1)
    {
        return 5;
    }

    // スリーカード判定(ランク:6)
    if (hand_dup[0] == 3)
    {
        return 6;
    }

    // ツーペア判定(ランク:7) and ワンペア判定(ランク:8)
    if (hand_dup[0] == 2)
    {
        if (hand_dup[1] == 2)
        {
            return 7;
        }
        else
        {
            return 8;
        }
    }

    return 9;
}

void judge_winners(struct player *pls, bool *winners_player)
{
    int player_rank[MAX_NUM_PLAYERS] = {0};
    for (int i = 0; i < MAX_NUM_PLAYERS; i++)
    {
        player_rank[i] = judge_hand(pls[i].hand);
    }

    int min_rank = player_rank[0];
    for (int i = 1; i < MAX_NUM_PLAYERS; i++)
    {
        if (min_rank > player_rank[i])
        {
            min_rank = player_rank[i];
        }
    }

    for (int i = 0; i < MAX_NUM_PLAYERS; i++)
    {
        if (min_rank == player_rank[i])
        {
            winners_player[i] = true;
        }
        else
        {
            winners_player[i] = false;
        }
    }
}