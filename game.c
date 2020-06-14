#include "game.h"
#include <stdlib.h>
#include <err.h>
#include <unistd.h>

LGame * init_game(unsigned int max_player) {
    LGame * game = malloc(sizeof(LGame));
    if (game == NULL)
        errx(EXIT_FAILURE,"game malloc()");
    game->players = malloc(sizeof(Player)*max_player);
    if (game->players == NULL)
        errx(EXIT_FAILURE,"game players malloc()");
    game->max_player = max_player;
    game->number = 0;
    game->player_count = 0;
    return game;
}

int add_player(LGame * game, GString * name, GString * password) {
    int pcount = atomic_load(&(game->player_count));
    if (pcount >= game->max_player)
        return 0;
    Player * p = &(game->players[pcount]);
    p->name = malloc(sizeof(char) * (name->len + 1));
    if (p->name  == NULL)
        errx(EXIT_FAILURE,"player name malloc()");
    p->name[name->len] = 0;
    strcpy(p->name, name->str);
    p->password = malloc(sizeof(char) * (1 + password->len));
    if (p->password == NULL)
        errx(EXIT_FAILURE,"player password malloc()");
    p->password[password->len] = 0;
    strcpy(p->password, password->str);
    atomic_fetch_add(&(game->player_count), 1);
    p->buf_to_send = g_string_new("");
    p->canRW = 0;
    return 1;
}

Player * get_player(LGame * game, gchar * name) {
    int pcount = atomic_load(&(game->player_count));
    for (int i = 0; i < pcount; i++) {
        if (!strcmp(game->players[i].name, name)) {
            return &(game->players[i]);
        }
    }
    return NULL;
}

// 0 is ok, 1 is not existant and 2 is wrong password
int validate_player(LGame * game, gchar * name, gchar * password) {
    Player * p = get_player(game, name);
    if (p == NULL)
        return 1;
    if (!strcmp(p->password, password))
        return 0;
    return 2;
}

void send_to(Player * p, GString * cmd) {
    while (atomic_load(&(p->canRW)) == 1)
    usleep(500000);
    atomic_fetch_add(&(p->canRW), 1);
    g_string_append_len(p->buf_to_send, cmd->str, cmd->len);
    atomic_fetch_sub(&(p->canRW), 1);
}

GString * get_next_to_send(Player * p) {
    if (p->buf_to_send == NULL)
        return g_string_new("ping_server_pong\n");
    while (atomic_load(&(p->canRW)) == 1)
        usleep(500000);
    atomic_fetch_add(&(p->canRW), 1);
    GString * to_send = g_string_new(p->buf_to_send->str);
    g_string_free(p->buf_to_send, TRUE);
    p->buf_to_send = g_string_new("ping_server_pong\n");
    atomic_fetch_sub(&(p->canRW), 1);
    return to_send;
}


