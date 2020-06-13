#ifndef GAME_H

#define GAME_H

#include <stdatomic.h>
#include <glib.h>

typedef struct Player {
    char * name;
    char * password;
    GString * role;
    atomic_int canRW;
    GString * buf_to_send;
} Player;

typedef struct LGame {
    int number;
    atomic_int player_count;
    int max_player;
    Player * players;
    char ** roles;
} LGame;

typedef struct GamePackage {
    LGame * game;
    int fd;
} GamePackage;

LGame * init_game(unsigned int max_player);
int add_player(LGame * game, GString * name, GString * password);
Player * get_player(LGame * game, gchar * name);
int validate_player(LGame * game, gchar * name, gchar * password);
void send_to(Player * p, GString * cmd);
GString * get_next_to_send(Player * p);

#endif
