#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <glib.h>
#include <pthread.h>
#include <ctype.h>
#include "game.h"
#include "network.h"

#define LOG 1

void urldecode2(char *dst, const char *src)
{
        char a, b;
        while (*src) {
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a')
                                a -= 'a'-'A';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'a'-'A';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b;
                        src+=3;
                } else if (*src == '+') {
                        *dst++ = ' ';
                        src++;
                } else {
                        *dst++ = *src++;
                }
        }
        *dst++ = '\0';
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
    char *output = malloc(sizeof(char) * (resource->len+1));
    urldecode2(output, resource->str);
    if (process_resource(g_string_new(output), &cmd, &name, &pswd, &content)) {
        free(output);
        return process_file(g_string_new("index.html"));
    }
    free(output);
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
        GString * response = g_string_new("connect_server_Bienvenue ");
        g_string_append_len(response, name->str, name->len);
        g_string_append_len(response, " !\n", 3);

        for (int i = 0; i < atomic_load(&(game->player_count)); i++) {
            if (strcmp(name->str, game->players[i].name)) {
                send_to(&(game->players[i]), response);
            }
        }
        printf("%s (%s) vient de se connecter\n", name->str, pswd->str);
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
        printf("%i: Client just pinging\n", cnx);
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

    addrinfo_error = getaddrinfo(NULL, port, &hints, &res);
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
