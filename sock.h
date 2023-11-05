#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>

void printpeerInfo(struct sockaddr_in *sock, int fd) {
    socklen_t len = sizeof(*sock);
    if (getpeername(fd, (struct sockaddr *)sock, &len) == -1)
        perror("getsockname");
    else {
        printf("%s:%d ", inet_ntoa(sock->sin_addr), ntohs(sock->sin_port));
    }
}


void printInfo(struct sockaddr_in *sock, int fd) {
    socklen_t len = sizeof(*sock);
    if (getsockname(fd, (struct sockaddr *)sock, &len) == -1)
        perror("getsockname");
    else {
        printf("%s:%d ", inet_ntoa(sock->sin_addr), ntohs(sock->sin_port));
    }
}

struct conn {
    int fd;
    struct conn* next;
};

struct server {
    int watch;
    char ADDR[20];
    int PORT;
    char base_dir[20];
    char curr_dir[100];
    int sfd;
    struct conn* first;
    struct conn* last;
    int n;
};

int makeSOCKET(struct server *data) {
    data->sfd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(data->PORT);
    servaddr.sin_addr.s_addr = inet_addr(data->ADDR);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    int t = 1;
    setsockopt(data->sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&t, sizeof(int));

    int e = bind(
        data->sfd,
        (struct sockaddr *) &servaddr,
        sizeof(servaddr)
    );

    if(e < 0) {
        printf("bind");
        return 1;
    }

    e = listen(data->sfd, 100);
    if (e < 0) {
        printf("listen");
        return 1;
    }
    
    printf("serving at ");
    printInfo(&servaddr, data->sfd);
    printf("\n");
    return 0;
}
