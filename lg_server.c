#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <glib.h>
#include <pthread.h>
#include <stdatomic.h>

#define BUFFER_SIZE 512
#define LOG 0

/*typedef struct BufQueue {
    gchar * buf;
    gsize length;
    struct BufQueue * next;
} BufQueue;
*/
typedef struct Player {
    char * name;
    char * password;
    char * role;
    atomic_int canRW;
    GString * buf_to_send;
} Player;

typedef struct LGame {
    int number;
    atomic_int player_count;
    int max_player;
    Player * players;
} LGame;

typedef struct GamePackage {
    LGame * game;
    int fd;
} GamePackage;

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
    printf("pc: %u\n", game->player_count);
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
        return g_string_new("");
    while (atomic_load(&(p->canRW)) == 1)
        usleep(500000);
    atomic_fetch_add(&(p->canRW), 1);
    GString * to_send = g_string_new(p->buf_to_send->str);
    g_string_free(p->buf_to_send, TRUE);
    p->buf_to_send = g_string_new("ping_server_pong\n");
    atomic_fetch_sub(&(p->canRW), 1);
    return to_send;
}
/*
   void send_to(struct Player * player, GString * cmd) {
   if (*(player.buf_to_send) == NULL) {
   player.buf_to_send = malloc(sizeof(struct BufQueue) + sizeof(gchar)*cmd->len);
 *(player.buf_to_send).buf = cmd->str;
 *(player.buf_to_send).length = cmd->len;
 *(player.buf_to_send).next = NULL;
 return;
 }
 struct BufQueue * prevBuf = player.buf_to_send;
 for (prevBuf = *(player.buf_to_send).next; *(prevBuf).next != NULL; prevBuf = *(prevBuf).next);
 *(prevBuf).next = malloc(sizeof(struct BufQueue) + sizeof(gchar)*cmd->len);
 *(prevBuf).next.buf = cmd->str;
 *(prevBuf).next.length = cmd->len;
 *(prevBuf).next.next = NULL;
 }

 GString * get_next_to_send(struct Player * player) {
 if (*(player.buf_to_send)==NULL)
 return g_string_new("");
 struct BufQueue * bufq = *(player.buf_to_send);
 GString * to_send = g_string_new(*(bufq).buf);
 player.buf_to_send = *(bufq).next;
 free(bufq.buf);
 free(bufq);
 return to_send;
 }*/

//Sending and getting
void write_fd(int fd_out, char* buf, ssize_t to_write, int more) {
    ssize_t offset = 0;
    while (to_write > 0) {
        ssize_t w;
        if (more)
            w = send(fd_out, buf + offset, to_write, MSG_MORE);
        else
            w = write(fd_out, buf + offset, to_write);
        if (w == -1)
            err(EXIT_FAILURE, "write()");
        offset += w;

        to_write -= w;
    }
}

GString * read_from_file(int fd_in) {
    ssize_t r = -1;
    GString * str = g_string_new(NULL);
    while (r != 0) {
        char buf[BUFFER_SIZE];
        buf[BUFFER_SIZE - 1] = 0;
        r = read(fd_in, buf, BUFFER_SIZE-1);
        if (r == -1)
            err(EXIT_FAILURE, "read()");
        g_string_append_len(str, buf, r);
    }
    return str;
}

GString * read_from_socket(int fd_in) {
    ssize_t r = 0;
    GString * str = g_string_new(NULL);
    while (!g_str_has_suffix(str->str, "\r\n\r\n")) {
        char buf[BUFFER_SIZE];
        buf[BUFFER_SIZE - 1] = 0;
        r = read(fd_in, buf, BUFFER_SIZE - 1);
        if (r == 0) {
            g_string_free(str, TRUE);
            return NULL;
        }
        if (r == -1)
            err(EXIT_FAILURE, "read()");
        g_string_append_len(str, buf, r);
    }

    return str;
}

GString * get_resource(int fd) {
    GString * str = read_from_socket(fd);
    if (str == NULL)
        return NULL;
    gsize start;
    for (start = 0; start < str->len && str->str[start] != '/'; start++);
    start++;
    gsize end;
    for (end = start; end < str->len && str->str[end] != ' '; end++);
    gchar * resource = g_strndup(str->str + start, end - start);
    if (LOG)
        printf("%i: %s\n", fd, resource);
    return g_string_new(resource);
}

void send_response(int fd, gchar * str, size_t len) {
    write_fd(fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 1);
    write_fd(fd, str, len, 0);
}

gsize get_next_char(gchar * str, gchar c, gsize n) {
    while (str[n] != c && str[n] != 0)
        n++;
    return n;
}

int process_resource(GString * resource, GString ** cmd, GString ** name, GString ** pswd, GString ** content) {
    gchar sep = '_';
    gsize cmdE = get_next_char(resource->str, sep, 0);
    *(cmd) = g_string_new(g_strndup(resource->str, cmdE));
    if (resource->str[cmdE] != '_')
        return 1;
    gsize nameE = get_next_char(resource->str, sep, cmdE + 1);
    *(name) = g_string_new(g_strndup(resource->str + cmdE + 1, nameE - cmdE - 1));
    if (resource->str[nameE] != '_')
        return 1;
    gsize pswdE = get_next_char(resource->str, sep, nameE + 1);
    *(pswd) = g_string_new(g_strndup(resource->str + nameE + 1, pswdE - nameE - 1));
    if (resource->str[pswdE] != '_')
        return 1;
    *(content) = g_string_new(g_strndup(resource->str + pswdE + 1, resource->len - pswdE - 1));
    return 0;
}

GString * process_file(GString * resource) {
    gchar * directory = "wwwtest/";
    size_t directoryLength = 8;
    gchar path[directoryLength + resource->len];
    strcpy(path, directory);
    strcat(path, resource->str);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        return NULL;
    }
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        return NULL;
    }
    int file = open(path, O_RDONLY);
    GString * response = read_from_file(file);
    close(file);
    return response;
}

void free_cmd(GString * cmd, GString * name, GString * pswd, GString *content) {
    g_string_free(cmd, TRUE);
    g_string_free(name, TRUE);
    g_string_free(pswd, TRUE);
    g_string_free(content, TRUE);
}

GString * process_command(GString * resource, LGame * game) {
    GString * cmd;
    GString * name;
    GString * pswd;
    GString * content;
    if (process_resource(resource, &cmd, &name, &pswd, &content))
        return process_file(g_string_new("index.html"));
    if (LOG)
        printf("Command %s from %s\n", cmd->str, name->str);

    if (!strcmp(cmd->str, "ping")) {
        free_cmd(cmd, name, pswd, content);
        return g_string_new("ping_server_Le serveur est en ligne\n");
    }
    if (!strcmp(cmd->str, "list")) {
        free_cmd(cmd, name, pswd, content);
        GString * msg = g_string_new("send_server_Liste des joueurs: ");
        for (int i = 0; i < atomic_load(&(game->player_count)); i++) {
            g_string_append(msg, game->players[i].name);
            g_string_append_len(msg, ".", 1);
        }
        g_string_append_len(msg, "\n", 1);
        return msg;
    }

    if (!strcmp(name->str, "joueur")) {
        free_cmd(cmd, name, pswd, content);
        return g_string_new("notconnected_server_Veuillez vous connecter avec /connect\n");
    }
    int player_state = validate_player(game, name->str, pswd->str);
    if (!strcmp(cmd->str, "connect")) {
        if (player_state == 0) {
            return g_string_new("connect_server_Vous êtes déjà connecté à la partie, reprise...");
        }
        if (player_state == 2) {
            return g_string_new("notconnected_server_Mot de passe incorrect, réessayez");
        }
        if (!add_player(game, name, pswd)) {
            free_cmd(cmd, name, pswd, content);
            return g_string_new("notconnected_server_Trop de personnes sont déjà connectées!\n");
        }
        if (LOG) {
            for (int i = 0; i < atomic_load(&(game->player_count)); i++) {
                printf("Player %s (%s)\n", game->players[i].name, game->players[i].password);
            }
        }
        printf("%s (%s) vient de se connecter\n", name->str, pswd->str);
        GString * response = g_string_new("connect_server_Bienvenue ");
        g_string_append_len(response, name->str, name->len);
        g_string_append_len(response, " !\n", 3);
        free_cmd(cmd, name, pswd, content);
        return response;
    }

    if (player_state == 2) {
        free_cmd(cmd, name, pswd, content);
        return g_string_new("send_server_Mot de passe incorrect, essayez de vous reconnecter (/connect)\n");
    }
    if (player_state == 1) {
        free_cmd(cmd, name, pswd, content);
        return g_string_new("notconnected_server_Connectez vous avec /connect\n");
    }
    Player * p = get_player(game, name->str);
    if (!strcmp(cmd->str, "send")) {
        GString * msg = g_string_new("send_");
        g_string_append_len(msg, name->str, name->len);
        g_string_append_len(msg, "_", 1);
        g_string_append_len(msg, content->str, content->len);
        g_string_append_len(msg, "\n", 1);
        for (int i = 0; i < atomic_load(&(game->player_count)); i++) {
            if (strcmp(p->name, game->players[i].name)) {
                send_to(&(game->players[i]), msg);
            }
        }
        free_cmd(cmd, name, pswd, content);
        return get_next_to_send(p);
    }
    if (!strcmp(cmd->str, "echo")) {
        send_to(p, g_string_new("send_server_echo2"));
        free_cmd(cmd, name, pswd, content);
        return g_string_new("send_server_echo1");
    }
    if (!strcmp(cmd->str, "update")) {
        free_cmd(cmd, name, pswd, content);
        return get_next_to_send(p);
    }
    free_cmd(cmd, name, pswd, content);
    return get_next_to_send(p);
}

int process_input(int fd, LGame * game) {
    GString * resource = get_resource(fd);
    if (resource == NULL) // If there is nothing sent by the client
        return 0;
    GString * rep = process_file(resource);
    if (rep == NULL)
        rep = process_command(resource, game);
    if (rep != NULL) {
        send_response(fd, rep->str, rep->len);
        g_string_free(rep, TRUE);
    }
    g_string_free(resource, TRUE);
    return 1;
}

//Threads
void * worker(void *arg) {
    GamePackage * pack = (GamePackage *) arg;
    int cnx = pack->fd;
    if (!process_input(cnx, pack->game)) {
        printf("%i: Client just pinging", cnx);
    }
    close(cnx);
    return NULL;
}



//Main loop
int main() {

    struct addrinfo *res;
    struct addrinfo hints;
    int addrinfo_error;
    char *port = "2048";
    char *host = "localhost";

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo_error = getaddrinfo(host, port, &hints, &res);
    if (addrinfo_error != 0) {
        errx(EXIT_FAILURE, "Failed getting address for %s on port %s: %s",
                host, port, gai_strerror(addrinfo_error));
    }

    int cnx;
    struct addrinfo *rp;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        int val = 1;
        cnx = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (setsockopt(cnx, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) == -1)
            errx(EXIT_FAILURE, "Couldn't configure correctly the socket");
        if (cnx == -1)
            continue;
        if (bind(cnx, rp->ai_addr, rp->ai_addrlen) != -1)
            break;
        close(cnx);
    }

    if (rp == NULL)
        errx(EXIT_FAILURE, "Couldn't open socket");

    LGame * game = init_game(3);

    printf("LG Server\n");
    listen(cnx, 5);
    printf("Listening to port %s...\n", port);
    while(1) {
        int ncnx = accept(cnx, rp->ai_addr, &(rp->ai_addrlen));
        if (ncnx == -1)
            errx(EXIT_FAILURE,"Connection failed...");
        GamePackage * pack = malloc(sizeof(GamePackage));
        if (pack == NULL)
            errx(EXIT_FAILURE,"GamePack malloc()");
        pack->game = game;
        pack->fd = ncnx;
        pthread_t thr;
        int e = pthread_create(&thr, NULL, worker, pack);
        if (e)
            errx(EXIT_FAILURE, "pthread_create()");
    }
    close(cnx);
    freeaddrinfo(res);
}
