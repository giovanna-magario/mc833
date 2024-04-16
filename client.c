#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490"

#define MAXDATASIZE 10000

// função de recebimento de mensagem
void receive_msg(int socket, char *msg){
    int status = recv(socket, msg, MAXDATASIZE - 1, 0);
    if (status == -1){
        perror("Error receiving message");
        return;
    }
    msg[status] = '\0';
    printf(msg);
    return;
}

// função de envio de mensagem
void send_msg(int socket, char *msg){
    if (strlen(msg) > MAXDATASIZE - 1){
        perror("Message not supported: length is bigger than the maximum allowed");
        return;
    }
    int status = send(socket, msg, strlen(msg), 0);
    if (status == -1){
        perror("Error sending message");
        return;
    }
    return;
}

// função de requisição para o servidor
void send_to_server(int socket)
{
    char msg[MAXDATASIZE];
    
    receive_msg(socket, msg);

    while (1)
    {
        if (strcmp(msg, "Serviço encerrado\n") == 0){
            exit(1);
        }
        printf("Aguardando Input...\n");
        scanf(" %[^\n]", msg);
        if(!strlen(msg)){
            exit(1);
        }
        send_msg(socket, msg);
        receive_msg(socket, msg);
    }
    return;
}


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
 
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);

    send_to_server(sockfd);

    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);

    close(sockfd);

    return 0;
}