#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <errno.h>
#include <signal.h>

#include "extra.h"  // utility functions
#include "sock.h"   // make socket and socket handlers
#include "signal.h" // signal handlers

// default constants
#define IP "0.0.0.0"
#define SIZE 1000
#define MAXCONN 100

// server codes
#define SERV_FILE 1
#define SERV_DIR 2

// error codes
#define EPARSE 23
#define ESOCK 73

// file codes
#define DFILE 12
#define DIR 13
#define UNKN 404

// response headers
#define HEAD    "HTTP/1.1 %d OK\r\n"
#define HEAD200 "HTTP/1.1 200 OK\r\n" // success
#define HEAD404 "HTTP/1.1 404 OK\r\n" // file not found
#define HEAD500 "HTTP/1.1 500 OK\r\n" // server error
#define HEAD403 "HTTP/1.1 403 OK\r\n" // access denied

struct server data;
int debug = 0;
int connection = 0;
int single_file = 0;



struct response {
    int type;
    char ext[20];
    char dir[500];
    int scode;
    size_t size;
};

void init(struct server *data) {
    data->watch = 0;
    strcpy(data->ADDR, IP);
    data->PORT = 9000;
    data->sfd = 0;
    data->first = data->last = NULL;
    data->n = 0;
}


char htmlhead[] = 
"<!DOCTYPE html>"
"<html lang=\"en\">"
"  <head>"
"    <style type=\"text/css\">"
"      * {"
"        box-sizing: border-box;"
"      }"
"      body {"
"        margin: 5px;"
"      }"
"      .row {"
"        padding: 2px;"
"        display: flex;"
"        width: 100%;"
"      }"
"      .row:nth-child(n+2):hover{"
"        background-color: #AAA;"
"      }"
"      .name {"
"        width: 100%;"
"      }"
"      .size {"
"        width: 100%;"
"      }"
"    </style>"
"  </head>"
"  <body>"
"    <div class=\"row\">"
"      <div class=\"name\">file / dir</div>"
"      <div class=\"size\">size</div>"
"    </div>";

FILE* file = NULL;

int parseCLA(int argc, char *argv[], struct server *data) {
    if(debug) printf("parseCLA()\n");
    int set = 0;

    for(int i=1;i<argc;i++) {
        if(argv[i][0] == '-') {
            for(int t = 1;t<strlen(argv[i]);t++)
                switch(argv[i][t]) {
                case 'w':
                    data->watch = 1;
                    break;
                case 'p':
                    if(i + 1 == argc) goto error;
                    data->PORT = atoi(argv[i+1]);
                    i++;
                    break;
                case 'd':
                    debug = 1;
                default:
                    break;
                }
        } else {
            if(set) goto error;
            sprintf(data->base_dir, "%s", argv[i]);
            sprintf(data->curr_dir, "%s", argv[i]);
            if(isFILE(argv[i])) {
                single_file = 1;
                printf("serving file %s at all urls.\n", argv[i]);
            } else {

            }
            set = 1;
        }
    }

    if(!set) {
error:
        printf("USE : %s <dirname/filename> <flags>\ncurrently supported flags :\n"
            "\t\t-p specify port number[ ... -p <PORT NUMBER> ... ]\n"
            "\t\t-d for printing debug information.\n", argv[0]);
        return 1;
    }
    return 0;
}

void _send(int fd, char dat[]) {
    send(fd, dat, strlen(dat), 0);
}

void _sendRaw(int fd, char dat[], int n) {
    send(fd, dat, n, 0);
}



void sendHTML(int fd, struct response *res) {
    if(debug) printf("makeHTML()\n");

    _send(fd, htmlhead);
    char dirf[500];
    if (debug) printf("dir %s\n", res->dir);
    sprintf(dirf, "ls --group-directories-first %s", res->dir);
    FILE* ls = popen(dirf, "r");
    char token[100];
    char line[600];
    while(!feof(ls) && !is_interrupted()) {
        int b = fscanf(ls, "%s", token, 100);

        // perror("fscanf");
        if(b <= 0) break;

        sprintf(dirf, "%s/%s", res->dir, token);
        double sz = isFILE(dirf);
        sprintf(line ,"");
        char size[100];
        sprintf(size, "");
        if(sz) sprintf(size, "%d B", (long long)sz);
        else if(isDIR(dirf)) strcat(token, "/");
        else continue;
        sprintf(
            line, 
            "<div class=\"row\">"
            "    <div class=\"name\">"
            "        <a href=\"%s\">%s</a>"
            "    </div>"
            "    <div class=\"size\">%s</div>"
            "</div>\n", token, token, size);
        _send(fd, line);
    }
    if(is_interrupted()) res->scode = 500;
}

void sendData(int fd, struct response *res) {
    if(debug) printf("sendData()\n");
    if(res->type == DIR) {
        res->scode = 200;
        _send(fd, HEAD200);
        _send(fd, "\r\n");
        // _send(fd, res->html);
        sendHTML(fd, res);
    } else if(res->type == DFILE) {

        int rem = (int) res->size;

        res->scode = 200;

        char data[SIZE+100];

        FILE* f = fopen(res->dir, "r");

        int ffd;
        if(f) {
            ffd = fileno(f);
            _send(fd, HEAD200);
        } else {
            res->scode = 403;
            _send(fd, HEAD403);
            _send(fd, "\r\n");
            return;
        }

        if(rem > 1.6e7) _send(fd, "Content-Disposition: attachment;\r\n"); // > 2MB files are directly downloaded
        char buff[100];
        sprintf(buff, "Content-Length: %d\r\n", rem);
        _send(fd, buff);

        _send(fd, "\r\n");

        if(debug)
            printf("Size %d fd%d\n", rem, ffd);

        while(rem > 0 && !feof(f) && !is_interrupted()) {
            int by = read(ffd, data, SIZE);
            int b = min(rem, SIZE);
            rem = rem - b;
            _sendRaw(fd, data, b);
        }
        if(is_interrupted()) res->scode = 500;

    } else {
        char buffer[100];
        sprintf(buffer, HEAD, res->scode);
        _send(fd, buffer);
        _send(fd, "\r\n");
        if(debug) printf("%s\n", buffer);
    }
}

void setType(char dir[], struct response *res) {
    if(debug) printf("setType()\n");
    struct stat path_stat;
    stat(dir, &path_stat);

    if(S_ISREG(path_stat.st_mode)) res->type = DFILE, res->size = path_stat.st_size;
    else if(S_ISDIR(path_stat.st_mode)) res->type = DIR;
    else res->type = UNKN;
}

int prepareData(char dir[], struct response *res) {
    if(debug) printf("prepareData()\n");

    setType(dir, res);
    if(res->type == DFILE) {
        if(debug) printf("TYPE : FILE\n");
        sprintf(res->dir, dir);
        get_filename_ext(dir, res->ext);
    } else if(res->type == DIR) {
        if(debug) printf("TYPE : DIR\n");
        sprintf(res->dir, dir);
    } else {
        res->scode = 404;
    }
}


void parseURL(char buffer[], struct server *data) {
    if(debug) printf("parseURL()\n");
    char type[4], url[500], buff[500];
    sscanf(buffer, "%s %s", type, buff);
    int t = 0;

    // remove `../` to guard outer directories.. a little secure tho not so much
    for(int i=0;i<=strlen(buff);i++) {
        if(i < strlen(buff) + 3 && buff[i] == '.' && buff[i+1] == '.' && buff[i+2] == '/') i += 2;
        else url[t++] = buff[i];
    }
    printf("%s : %s ", type, url);
    if(single_file) sprintf(url, "");
    sprintf(data->curr_dir, "%s%s", data->base_dir, url);
}

void serve_client(int nsfd, struct sockaddr_in caddr, socklen_t len, char buffer[]) {
    if(debug) printf("connection %d opened.\n", connection);

    ssize_t nread = recvfrom(nsfd, buffer, SIZE, 0,
                (struct sockaddr *) &caddr, &len);
    
    if(nread > 0) {
        printpeerInfo(&caddr, nsfd);

        struct response res = {0};
        parseURL(buffer, &data);

        prepareData(data.curr_dir, &res);

        sendData(nsfd, &res);
        printf("\t%d\n", res.scode);

        reset_interrupts();
    }

    shutdown(nsfd, SHUT_RDWR);
    close(nsfd);

    if(debug) printf("connection %d closed.\n", connection);
}


int main(int argc, char *argv[]) {

    if (exit_signal(SIGINT) ||
        exit_signal(SIGTERM) ||
        exit_signal(SIGHUP) ||
        ignore_signal(SIGPIPE)) {
        fprintf(stderr, "Cannot install signal handlers: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    init(&data);
    if(parseCLA(argc, argv, &data)) {
        return EPARSE;
    }

    if(makeSOCKET(&data)) {
        return ESOCK;
    }

    struct sockaddr_in caddr;
    socklen_t len;
    int nsfd;
    
    char buffer[SIZE];
    if(!buffer) {
        perror("malloc");
        return 1;
    }

    while(is_running()) {
        nsfd = accept(data.sfd, (struct sockaddr *) &caddr, &len);
        if(nsfd < 0) {
            if(debug) perror("accept");
            break;
        }

        connection++;
        if(fork() == 0) {
            close(data.sfd);
            serve_client(nsfd, caddr, len, buffer);
            return 0;
        }
        close(nsfd);
    }

    printf("\n\nExiting.");

    shutdown(data.sfd, SHUT_RDWR);
    close(data.sfd);
}

