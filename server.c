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

#define MAXDATASIZE 10000

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

void generateAllInfo(cJSON *dataArray, char* allSongs) {
    // Definindo um array de strings
    char *properties[] = {"id", "titulo", "artista", "idioma", "tipo", "ref", "ano"};
    char *propertiesPrintable[] = {"Identificador Único: ", "Titulo: ", "Intérprete: ", "Idioma: ", "Tipo da música: ", "Refrão: ", "Ano de lançamento: "};

    // Obtendo o tamanho do array
    int size = sizeof(properties) / sizeof(properties[0]);

    strcat(allSongs, "Músicas: \n");

    for (int i = 0; i < cJSON_GetArraySize(dataArray); i++) {
        cJSON *item = cJSON_GetArrayItem(dataArray, i);
        if (item) {
            for (int j = 0; j < size; j++) {
                cJSON *prop = cJSON_GetObjectItemCaseSensitive(item, properties[j]); 
                strcat(allSongs, propertiesPrintable[j]); 
                if (cJSON_IsString(prop) && (prop->valuestring != NULL)) {
                    strcat(allSongs, prop->valuestring); 
                }
                else if (cJSON_IsNumber(prop) && (prop->valueint != NULL)) {
                    char num_str[20];
                    sprintf(num_str, "%d", prop->valueint);
                    strcat(allSongs, num_str); 
                }
                strcat(allSongs, "\n");
            }
        }
    }
    return;
}

void generateSongsStr(cJSON *filteredSongsArray, char* allSongs) {
    // Definindo um array de strings
    char *properties[] = {"id", "titulo", "artista"};
    char *propertiesPrintable[] = {"Identificador Único: ", "Titulo: ", "Intérprete: "};

    // Obtendo o tamanho do array
    int size = sizeof(properties) / sizeof(properties[0]);

    strcat(allSongs, "Músicas: \n");

    for (int i = 0; i < cJSON_GetArraySize(filteredSongsArray); i++) {
        cJSON *item = cJSON_GetArrayItem(filteredSongsArray, i);
        if (item) {
            for (int j = 0; j < size; j++) {
                cJSON *prop = cJSON_GetObjectItemCaseSensitive(item, properties[j]);
                strcat(allSongs, propertiesPrintable[j]); 
                if (cJSON_IsString(prop) && (prop->valuestring != NULL)) {
                    strcat(allSongs, prop->valuestring); 
                } 
                else if (cJSON_IsNumber(prop) && (prop->valueint != NULL)) {
                    char num_str[20];
                    sprintf(num_str, "%d", prop->valueint);
                    strcat(allSongs, num_str); 
                }
                strcat(allSongs, "\n");
            }
        }
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
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }

    // Obter o valor atual de 'index' no JSON
    cJSON *index_value = cJSON_GetObjectItem(json, "index");
    if (!cJSON_IsNumber(index_value)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'index' is not a number\n");
        send_msg(socket, "Error: 'index' is not a number\n");
        return;
    }
    id = index_value->valueint; // Usar o valor do 'index' como 'id'
    
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

    // Incrementar o valor do 'index' no JSON
    index_value->valueint++;
    cJSON_SetIntValue(index_value, id + 1); 

    // Escrever o JSON atualizado de volta no arquivo
    fp = fopen("data/data.json", "w");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
        cJSON_Delete(json);
        return;
    }
    char *json_string = cJSON_Print(json);
    fputs(json_string, fp);
    fclose(fp);
    free(json_string);
    // Liberar a memória alocada para o objeto JSON e enviar mensagem de sucesso para o client
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

    // Solicitar o id da música ao client
    send_msg(socket, "Informe o id da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%d", &id);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }

    // Procurar a música com o id especificado e removê-la do array
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

    // Enviar mensagem de erro caso a música não exista
    if (removed_song == NULL) {
        fprintf(stderr, "Error: Song with ID %d not found\n", id);
        char msg[MAXDATASIZE];
        sprintf(msg, "Error: Song with ID %d not found\n", id);
        send_msg(socket, msg);
        cJSON_Delete(json);
        return;
    }

    // Escrever o JSON atualizado de volta no arquivo
    fp = fopen("data/data.json", "w");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
        cJSON_Delete(json);
        cJSON_Delete(removed_song);
        return;
    }
    char *json_string = cJSON_Print(json);
    fputs(json_string, fp);
    fclose(fp);
    free(json_string);

    // Liberar a memória alocada para o objeto JSON e enviar mensagem de sucesso para o client
    cJSON_Delete(json);
    cJSON_Delete(removed_song);
    printf("Música deletada!\n");
    send_msg(socket, "Música deletada!\n");
    return;
}

void song_by_year(int socket)
{
    int ano;
    char buffer[MAXDATASIZE], allSongs[MAXDATASIZE];

    // Solicitar o ano buscado ao client
    send_msg(socket, "Informe o ano de lançamento das músicas que você quer buscar: ");
    receive_msg(socket, buffer);
    sscanf(buffer, "%d", &ano);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }

    cJSON *filtered_array = cJSON_CreateArray();

    cJSON *item;
    cJSON_ArrayForEach(item, data_array) {
        // Verificar se o objeto possui uma propriedade "tipo"
        cJSON *ano_item = cJSON_GetObjectItem(item, "ano");
        if (ano_item && cJSON_IsNumber(ano_item)) {
            // Verificar se o valor da propriedade "tipo" é o valor desejado
            if (ano_item->valueint == ano) {
                // Adicionar o objeto ao array filtrado
                cJSON_AddItemToArray(filtered_array, cJSON_Duplicate(item, 1));
            }
        }
    }
    memset(allSongs, 0, sizeof(allSongs));
    generateSongsStr(filtered_array, allSongs);
    // Liberar a memória alocada para o objeto JSON e enviar mensagem de sucesso para o client
    cJSON_Delete(json);
    send_msg(socket, allSongs);
    return;
}

void song_by_language(int socket)
{
    int ano;
    char buffer[MAXDATASIZE], idioma[MAXDATASIZE], allSongs[MAXDATASIZE];

    // Solicitar cada dado ao cliente individualmente
    send_msg(socket, "Informe o ano de lançamento das músicas que você quer buscar: ");
    receive_msg(socket, buffer);
    sscanf(buffer, "%d", &ano);
    send_msg(socket, "Informe o idioma das músicas que você quer buscar: ");
    receive_msg(socket, buffer);
    sscanf(buffer, "%99[^\n]", idioma);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }

    cJSON *filtered_array = cJSON_CreateArray();

    cJSON *item;
    cJSON_ArrayForEach(item, data_array) {
        // Verificar se o objeto possui uma propriedade "tipo"
        cJSON *ano_item = cJSON_GetObjectItem(item, "ano");
        cJSON *idioma_item = cJSON_GetObjectItem(item, "idioma");
        if (ano_item && cJSON_IsNumber(ano_item) && idioma_item && cJSON_IsString(idioma_item)) {
            // Verificar se o valor da propriedade "tipo" é o valor desejado
            if (ano_item->valueint == ano) {
                if (strcmp(idioma_item->valuestring, idioma) == 0) {
                    // Adicionar o objeto ao array filtrado
                    cJSON_AddItemToArray(filtered_array, cJSON_Duplicate(item, 1));
                }
            }
        }
    }
    memset(allSongs, 0, sizeof(allSongs));
    generateSongsStr(filtered_array, allSongs);
    // Liberar a memória alocada para o objeto JSON e enviar mensagem de sucesso para o client
    cJSON_Delete(json);
    send_msg(socket, allSongs);
    return;
}

void song_by_type(int socket)
{
    char buffer[MAXDATASIZE], tipo[MAXDATASIZE], allSongs[MAXDATASIZE];

    // Solicitar id da música ao client
    send_msg(socket, "Informe o tipo das músicas que você quer buscar: ");
    receive_msg(socket, buffer);
    sscanf(buffer, "%99[^\n]", tipo);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }

    cJSON *filtered_array = cJSON_CreateArray();

    cJSON *item;
    cJSON_ArrayForEach(item, data_array) {
        // Verificar se o objeto possui uma propriedade "tipo"
        cJSON *tipo_item = cJSON_GetObjectItem(item, "tipo");
        if (tipo_item && cJSON_IsString(tipo_item)) {
            // Verificar se o valor da propriedade "tipo" é o valor desejado
            if (strcmp(tipo_item->valuestring, tipo) == 0) {
                // Adicionar o objeto ao array filtrado
                cJSON_AddItemToArray(filtered_array, cJSON_Duplicate(item, 1));
            }
        }
    }
    memset(allSongs, 0, sizeof(allSongs));
    generateSongsStr(filtered_array, allSongs);
    // Liberar a memória alocada para o objeto JSON e enviar mensagem de sucesso para o client
    cJSON_Delete(json);
    send_msg(socket, allSongs);
    return;
}

void song_details(int socket)
{
    char buffer[MAXDATASIZE];
    char allSongs[MAXDATASIZE];

    // Variáveis para armazenar os dados da nova música
    int id;

    // Solicitar id da música ao client
    send_msg(socket, "Informe o id da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%d", &id);

    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }

    // Procurar a música com o ID especificado e removê-la do array
    cJSON *filtered_array = cJSON_CreateArray();
    cJSON *item;
    cJSON_ArrayForEach(item, data_array) {
        // Verificar se o objeto possui uma propriedade "tipo"
        cJSON *song_id = cJSON_GetObjectItem(item, "id");
        if (song_id && cJSON_IsNumber(song_id)) {
            // Verificar se o valor da propriedade "tipo" é o valor desejado
            if (song_id->valueint == id) {
                // Adicionar o objeto ao array filtrado
                cJSON_AddItemToArray(filtered_array, cJSON_Duplicate(item, 1));
            }
        }
    }

    // Enviar mensagem de erro caso a música não exista
    if (cJSON_GetArraySize(filtered_array) == 0) {
        fprintf(stderr, "Error: Song with ID %d not found\n", id);
        char msg[MAXDATASIZE];
        sprintf(msg, "Error: Song with ID %d not found\n", id);
        send_msg(socket, msg);
        cJSON_Delete(json);
        return;
    }
    memset(allSongs, 0, sizeof(allSongs));
    generateAllInfo(filtered_array, allSongs);

    // Liberar a memória alocada para os objetos JSON e enviar mensagem de sucesso para o client
    cJSON_Delete(json);
    send_msg(socket, allSongs);
    return;
}

void list_all_songs(int socket)
{
    char allSongs[MAXDATASIZE];
    // Abrir o arquivo data.json para leitura
    FILE *fp = fopen("data/data.json", "r");
    if (fp == NULL) {
        perror("Error opening file");
        send_msg(socket, "Error opening file");
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
            char msg[MAXDATASIZE];
            sprintf(msg, "Error parsing JSON: %s\n", error_ptr);
            send_msg(socket, msg);
        }
        return;
    }

    // Acessar o array 'data'
    cJSON *data_array = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data_array)) {
        cJSON_Delete(json);
        fprintf(stderr, "Error: 'data' is not an array\n");
        send_msg(socket, "Error: 'data' is not an array\n");
        return;
    }
    memset(allSongs, 0, sizeof(allSongs));
    generateAllInfo(data_array, allSongs);
    
    // Liberar a memória alocada para o objeto JSON e enviar mensagem de sucesso para o client
    cJSON_Delete(json);
    send_msg(socket, allSongs);
    return;
}

void commands(int socket)
{
    // Mostrar lista de comandos possíveis para o client
    send_msg(socket, "Opções de comandos para músicas:\n1: cadastrar nova música\n2: remover uma música\n3: listar músicas por ano\n4: listar músicas por idioma e ano\n5: listar músicas por tipo\n6: detalhes da música\n7: detalhes de todas as músicas\nc: ver lista de comandos\ns: sair\n");
    return;
}

// Menu que lida com as opções do client
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
        case '3':
            printf("Listar todas as músicas (identificador, título e intérprete) lançadas em um determinado ano\n");
            song_by_year(socket);
            break;
        case '4':
            printf("Listar todas as músicas (identificador, título e intérprete) em um dado idioma lançadas num certo ano\n");
            song_by_language(socket);
            break;
        case '5':
            printf("Listar todas as músicas (identificador, título e intérprete) de um certo tipo\n");
            song_by_type(socket);
            break;
        case '6':
            printf("Listar todas as informações de uma música dado o seu identificador\n");
            song_details(socket);
            break;
        case '7':
            printf("Listar todas as informações de todas as músicas\n");
            list_all_songs(socket);
            break;
        case 'c':
            commands(socket);
            break;
        case 's':
            send_msg(socket, "Serviço encerrado\n");
            break;
        default:
            printf("Comando inválido\n");
            send_msg(socket, "Comando inválido\n");
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