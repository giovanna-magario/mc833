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

#define FILENAME "received_song.mp3" // Nome do arquivo a ser salvo


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

// Função para salvar o conteúdo do arquivo .mp3 recebido via UDP
void save_song(const char *filename, const char *content, int size) {
    FILE *fp = fopen(filename, "wb"); // Abre o arquivo para escrita binária
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    if (fwrite(content, 1, size, fp) != size) { // Escreve o conteúdo no arquivo
        perror("fwrite");
        exit(1);
    }
    fclose(fp); // Fecha o arquivo
    printf("Música salva com sucesso: %s\n", filename);
}

int main(int argc, char *argv[]) {
    int tcp_sockfd, udp_sockfd, numbytes, rv, maxfdp1;
    char buf[MAXDATASIZE], udp_buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    fd_set master, read_fds;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Criação do socket TCP
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((tcp_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(tcp_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(tcp_sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // Tudo feito com essa estrutura

    // Criação do socket UDP
    struct sockaddr_in udp_servaddr;
    bzero(&udp_servaddr, sizeof(udp_servaddr));
    udp_servaddr.sin_family = AF_INET;
    udp_servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, argv[1], &udp_servaddr.sin_addr);

    if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    FD_SET(tcp_sockfd, &master);
    FD_SET(udp_sockfd, &master);
    maxfdp1 = tcp_sockfd > udp_sockfd ? tcp_sockfd + 1 : udp_sockfd + 1;

    while (1) {
        read_fds = master;
        if (select(maxfdp1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        if (FD_ISSET(tcp_sockfd, &read_fds)) {
            // Manipulação de dados TCP
            send_to_server(tcp_sockfd);
            if ((numbytes = recv(tcp_sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            buf[numbytes] = '\0';
            printf("client: received '%s'\n", buf);
            close(tcp_sockfd);
            break;
        }

        if (FD_ISSET(udp_sockfd, &read_fds)) {
            // Recebimento do arquivo .mp3 via UDP
            int numbytes;
            int total_bytes = 0;
            while ((numbytes = recvfrom(udp_sockfd, udp_buf, MAXDATASIZE - 1, 0, NULL, NULL)) > 0) {
                // Salva o conteúdo do arquivo .mp3
                save_song(FILENAME, udp_buf, numbytes);
                total_bytes += numbytes;
            }
            printf("Total de bytes recebidos: %d\n", total_bytes);
            close(udp_sockfd);
            break;
        }
    }

    return 0;
}