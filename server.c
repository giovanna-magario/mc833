#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <cjson/cJSON.h>

#define PORT "3490"

#define BACKLOG 10

#define MAXDATASIZE 10000 // max number of bytes we can get at once 

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void receive_msg(int socket, char *msg){
    int status = recv(socket, msg, MAXDATASIZE - 1, 0);
    if (status == -1){
        perror("Error receiving message");
        return;
    }
    msg[status] = '\0';
    printf("Message received: %s\n", msg);
    return;
}

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

void add_song(int socket)
{
    char buffer[MAXDATASIZE];

    // Variáveis para armazenar os dados da nova música
    int id, ano;
    char titulo[100], artista[100], idioma[50], tipo[50], ref[100];

    // Solicitar cada dado ao cliente individualmente
    send_msg(socket, "Informe o título da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%99[^\n]", titulo);

    send_msg(socket, "Informe o artista da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%99[^\n]", artista);

    send_msg(socket, "Informe o idioma da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%49[^\n]", idioma);

    send_msg(socket, "Informe o tipo da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%49[^\n]", tipo);

    send_msg(socket, "Informe a refrão da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%99[^\n]", ref);

    send_msg(socket, "Informe o ano da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%d", &ano);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }

    // Ler o conteúdo do arquivo em uma string
    char json_buffer[1024];
    size_t len = fread(json_buffer, 1, sizeof(json_buffer), fp);
    fclose(fp);

    // Analisar a string JSON
    cJSON *json = cJSON_Parse(json_buffer);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        return;
    }

    // Obter o valor atual de 'index' no JSON
    cJSON *index_value = cJSON_GetObjectItem(json, "index");
    if (!cJSON_IsNumber(index_value)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'index' is not a number\n");
        return;
    }
    id = index_value->valueint; // Usar o valor de 'index' como 'id'
    
    // Criar um novo objeto JSON para a nova música
    cJSON *new_song_json = cJSON_CreateObject();
    cJSON_AddItemToArray(data_array, new_song_json);
    cJSON_AddNumberToObject(new_song_json, "id", id);
    cJSON_AddStringToObject(new_song_json, "titulo", titulo);
    cJSON_AddStringToObject(new_song_json, "artista", artista);
    cJSON_AddStringToObject(new_song_json, "idioma", idioma);
    cJSON_AddStringToObject(new_song_json, "tipo", tipo);
    cJSON_AddStringToObject(new_song_json, "ref", ref);
    cJSON_AddNumberToObject(new_song_json, "ano", ano);

    // Incrementar o valor de 'index' no JSON
    index_value->valueint++;

    cJSON_SetIntValue(index_value, id + 1); 

    // Escrever o JSON atualizado de volta no arquivo
    fp = fopen("data/data.json", "w");
    if (fp == NULL) {
        perror("Error opening file");
        cJSON_Delete(json);
        return;
    }
    char *json_string = cJSON_Print(json);
    fputs(json_string, fp);
    fclose(fp);
    free(json_string);
    // Liberar a memória alocada para o objeto JSON
    cJSON_Delete(json);
    printf("Nova música cadastrada!\n");
    send_msg(socket, "Nova música cadastrada!\n");
    return;
}

void remove_song(int socket)
{
    char buffer[MAXDATASIZE];

    // Variáveis para armazenar os dados da nova música
    int id;

    // Solicitar cada dado ao cliente individualmente
    send_msg(socket, "Informe o id da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%d", &id);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }

    // Ler o conteúdo do arquivo em uma string
    char json_buffer[1024];
    size_t len = fread(json_buffer, 1, sizeof(json_buffer), fp);
    fclose(fp);

    // Analisar a string JSON
    cJSON *json = cJSON_Parse(json_buffer);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        return;
    }

    // Procurar a música com o ID especificado e removê-la do array
    int array_size = cJSON_GetArraySize(data_array);
    cJSON *removed_song = NULL;
    for (int i = 0; i < array_size; ++i) {
        cJSON *song = cJSON_GetArrayItem(data_array, i);
        cJSON *song_id = cJSON_GetObjectItem(song, "id");
        if (cJSON_IsNumber(song_id) && song_id->valueint == id) {
            removed_song = cJSON_DetachItemFromArray(data_array, i);
            break;
        }
    }

    if (removed_song == NULL) {
        fprintf(stderr, "Error: Song with ID %d not found\n", id);
        cJSON_Delete(json);
        return;
    }

    // Escrever o JSON atualizado de volta no arquivo
    fp = fopen("data/data.json", "w");
    if (fp == NULL) {
        perror("Error opening file");
        cJSON_Delete(json);
        cJSON_Delete(removed_song);
        return;
    }
    char *json_string = cJSON_Print(json);
    fputs(json_string, fp);
    fclose(fp);
    free(json_string);

    // Liberar a memória alocada para os objetos JSON
    cJSON_Delete(json);
    cJSON_Delete(removed_song);
    printf("Música deletada!\n");
    send_msg(socket, "Música deletada!\n");
    return;
}

void song_by_year(int socket, char message)
{

}

void song_by_language(int socket, char message)
{

}

void song_by_type(int socket, char message)
{

}

void song_details(int socket, char message)
{

}

void list_all_songs(int socket)
{
    char allSongs[MAXDATASIZE];
    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }

    // Ler o conteúdo do arquivo em uma string
    char json_buffer[1024];
    size_t len = fread(json_buffer, 1, sizeof(json_buffer), fp);
    fclose(fp);

    // Analisar a string JSON
    cJSON *json = cJSON_Parse(json_buffer);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        return;
    }

    // Definindo um array de strings
    char *properties[] = {"titulo", "artista", "idioma", "tipo", "ref", "ano"};
    char *propertiesPrintable[] = {"Titulo: ", "Artista: ", "Idioma: ", "Tipo: ", "Refrão: ", "Ano: "};

    // Obtendo o tamanho do array
    int size = sizeof(properties) / sizeof(properties[0]);

    strcat(allSongs, "Músicas: \n");

    for (int i = 0; i < cJSON_GetArraySize(data_array); i++) {
        cJSON *item = cJSON_GetArrayItem(data_array, i);
        if (item) {
            for (int j = 0; j < size; j++) {
                cJSON *prop = cJSON_GetObjectItemCaseSensitive(item, properties[j]); 
                if (cJSON_IsString(prop) && (prop->valuestring != NULL)) {
                    strcat(allSongs, propertiesPrintable[j]); 
                    strcat(allSongs, prop->valuestring); 
                } 
                strcat(allSongs, "\n");
            }
        }
    }

    // Liberar a memória alocada para o objeto JSON
    cJSON_Delete(json);
    send_msg(socket, allSongs);
    free(allSongs);
    return;
}

void commands(int socket)
{
    send_msg(socket, "Opções de comandos para músicas:\n1: cadastrar nova música\n2: remover uma música\n3: listar músicas por ano\n4: listar músicas por idioma e ano\n5: listar músicas por tipo\n6: detalhes da música\n7: detalhes de todas as músicas\nc: ver lista de comandos\ns: sair\n");
    return;
}

void menu(int socket)
{
    char message[MAXDATASIZE];

    commands(socket);
    while(1)
    {
        printf("aguardando novas mensagens...\n");
        receive_msg(socket, message);
        switch (message[0])
        {
        case '1':
            printf("Cadastrar nova música\n");
            add_song(socket);
            break;
        case '2':
            printf("Deletar uma música\n");
            remove_song(socket);
            break;
        case '7':
            printf("Listar todas as informações de todas as músicas\n");
            list_all_songs(socket);
            break;
        case 'c':
            commands(socket);
            break;
        case 's':
            break;
        default:
            printf("Comando inválido");
        }
    }
    return;
}

int main(void)
{
    int sockfd, new_fd, st;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((st = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(st));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) {
            close(sockfd);
            menu(new_fd);
            close(new_fd);
            exit(0);
        }
        
        close(new_fd);
    }

    return 0;
}