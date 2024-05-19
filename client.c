#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <limits.h>

#include <arpa/inet.h>

#define PORT "3490"

#define UDP_PORT "4950"

#define FILENAME "received_song.mp3"

#define MAXMSGSIZE 10000

#define MAXDATASIZE 1000000

// Função para salvar o conteúdo do arquivo .mp3 recebido via UDP
void save_song(const char *filename, const char *content, int size) {
    FILE *fp = fopen(filename, "ab"); // Abre o arquivo para escrita binária
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

// função de recebimento de mensagem
void receive_msg(int socket, char *msg){
    int status = recv(socket, msg, MAXMSGSIZE - 1, 0);
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
    if (strlen(msg) > MAXMSGSIZE - 1){
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
    int sockfd, udp_sockfd, numbytes;  
    char buf[MAXMSGSIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_in udp_serv_addr;


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

    char msg[MAXMSGSIZE];
    
    receive_msg(sockfd, msg);

    while (strcmp(msg, "Fazendo download da música...\n") != 0)
    {
        if (strcmp(msg, "Serviço encerrado\n") == 0){
            exit(1);
        }
        printf("Aguardando Input...\n");
        scanf(" %[^\n]", msg);
        if(!strlen(msg)){
            exit(1);
        }
        send_msg(sockfd, msg);
        receive_msg(sockfd, msg);
    }

    close(sockfd);

    // Criar o socket UDP
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        perror("Erro ao abrir o socket UDP");
        exit(1);
    }

    // Configurar o endereço do servidor UDP
    bzero((char *) &udp_serv_addr, sizeof(udp_serv_addr));
    udp_serv_addr.sin_family = AF_INET;
    udp_serv_addr.sin_addr.s_addr = INADDR_ANY;
    udp_serv_addr.sin_port = htons(atoi(UDP_PORT));

    // Enviar uma mensagem para o servidor UDP
    char *udp_message = "Mensagem UDP do cliente";
    char udp_buf[MAXMSGSIZE];
    int udp_len = sizeof(udp_serv_addr);
    if (sendto(udp_sockfd, udp_message, strlen(udp_message), 0, (struct sockaddr *) &udp_serv_addr, udp_len) < 0) {
        perror("Erro ao enviar mensagem UDP");
        close(udp_sockfd);
        exit(1);
    }

    printf("Mensagem UDP enviada\n");

    int total_bytes = 0;
    while ((numbytes = recvfrom(udp_sockfd, udp_buf, MAXMSGSIZE, 0, NULL, NULL)) > 0) {
        // Salva o conteúdo do arquivo .mp3
        printf("%zd\n", numbytes);
        save_song(FILENAME, udp_buf, numbytes);
        total_bytes += numbytes;
    }
    printf("Total de bytes recebidos: %d\n", total_bytes);

    // Fechar o socket UDP
    close(udp_sockfd);

    return 0;
}