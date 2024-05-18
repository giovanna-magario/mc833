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

// função de recebimento de mensagem
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

// gera as informações de todos os dados em formato de string para serem mostrados no console
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

// gera as informações de 3 dados (id, titulo e interprete) em formato de string para serem mostrados no console
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
    char titulo[MAXDATASIZE], artista[MAXDATASIZE], idioma[MAXDATASIZE], tipo[MAXDATASIZE], ref[MAXDATASIZE];

    // Solicitar cada dado ao cliente individualmente
    send_msg(socket, "Informe o título da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%9999[^\n]", titulo);

    send_msg(socket, "Informe o artista da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%9999[^\n]", artista);

    send_msg(socket, "Informe o idioma da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%9999[^\n]", idioma);

    send_msg(socket, "Informe o tipo da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%9999[^\n]", tipo);

    send_msg(socket, "Informe a refrão da música: \n");
    receive_msg(socket, buffer);
    sscanf(buffer, "%9999[^\n]", ref);

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
    char json_buffer[MAXDATASIZE];
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
    char json_buffer[MAXDATASIZE];
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
    char json_buffer[MAXDATASIZE];
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
    char json_buffer[MAXDATASIZE];
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
    char json_buffer[MAXDATASIZE];
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
    char json_buffer[MAXDATASIZE];
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
    char json_buffer[MAXDATASIZE];
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


// Função para enviar um arquivo via UDP
void send_file_udp(int sockfd, struct sockaddr_in *addr, const char *filename) {
    int filefd, numbytes;
    char filebuf[MAXDATASIZE];

    // Abre o arquivo para leitura
    if ((filefd = open(filename, O_RDONLY)) == -1) {
        perror("open");
        exit(1);
    }

    // Lê o conteúdo do arquivo e envia via UDP
    while ((numbytes = read(filefd, filebuf, MAXDATASIZE)) > 0) {
        if (sendto(sockfd, filebuf, numbytes, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1) {
            perror("sendto");
            exit(1);
        }
    }

    // Fecha o arquivo
    close(filefd);
}

void download_song(int socketTCP, int socketUDP, struct sockaddr_in *addr) {
    int id;
    char buffer[MAXDATASIZE], filename[MAXDATASIZE];

    // Solicitar id da música ao client
    send_msg(socketTCP, "Informe o id da música: \n");
    receive_msg(socketTCP, buffer);
    sscanf(buffer, "%d", &id);
    // Constrói o caminho do arquivo .mp3
    sprintf(filename, "data/%s.mp3", song_id);

    send_file_udp(socketUDP, addr, filename);

}

void commands(int socket)
{
    // Mostrar lista de comandos possíveis para o client
    send_msg(socket, "Opções de comandos para músicas:\n1: cadastrar nova música\n2: remover uma música\n3: listar músicas por ano\n4: listar músicas por idioma e ano\n5: listar músicas por tipo\n6: detalhes da música\n7: detalhes de todas as músicas\nc: ver lista de comandos\ns: sair\n");
    return;
}

// Menu que lida com as opções do client
void menu(int socketTCP, int socketUDP, struct sockaddr_in *addr)
{
    char message[MAXDATASIZE];

    commands(socketTCP);
    while(1)
    {
        printf("aguardando novas mensagens...\n");
        receive_msg(socketTCP, message);
        switch (message[0])
        {
        case '1':
            printf("Cadastrar nova música\n");
            add_song(socketTCP);
            break;
        case '2':
            printf("Deletar uma música\n");
            remove_song(socketTCP);
            break;
        case '3':
            printf("Listar todas as músicas (identificador, título e intérprete) lançadas em um determinado ano\n");
            song_by_year(socketTCP);
            break;
        case '4':
            printf("Listar todas as músicas (identificador, título e intérprete) em um dado idioma lançadas num certo ano\n");
            song_by_language(socketTCP);
            break;
        case '5':
            printf("Listar todas as músicas (identificador, título e intérprete) de um certo tipo\n");
            song_by_type(socketTCP);
            break;
        case '6':
            printf("Listar todas as informações de uma música dado o seu identificador\n");
            song_details(socketTCP);
            break;
        case '7':
            printf("Listar todas as informações de todas as músicas\n");
            list_all_songs(socketTCP);
            break;
        case '8':
            printf("Fazer download da música de uma música dado o seu identificador\n");
            download_song(socketTCP, socketUDP, addr);
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

int main(void) {
    int listenfd, connfd, udpfd, nready, maxfdp1;
    char mesg[MAXLINE];
    pid_t childpid;
    fd_set rset;
    ssize_t n;
    socklen_t len;
    const int on = 1;
    struct sockaddr_in cliaddr, servaddr;
    void sig_chld(int);

    /* create listening TCP socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listenfd, LISTENQ);

    /* create UDP socket */
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    bind(udpfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    FD_ZERO(&rset);
    maxfdp1 = listenfd > udpfd ? listenfd + 1 : udpfd + 1;

    for (;;) {
        FD_SET(listenfd, &rset);
        FD_SET(udpfd, &rset);

        if ((nready = select(maxfdp1, &rset, NULL, NULL, NULL)) < 0) {
            if (errno == EINTR)
                continue;
            else
                perror("select error");
        }

        if (FD_ISSET(listenfd, &rset)) {
            len = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
            if ((childpid = fork()) == 0) { /*child process */
                close(listenfd); /* close listening socket */
                menu(connfd, udpfd, &cliaddr); /* process the request */
                exit(0);
            }
            close(connfd); /* parent closes connected socket */
        }
    }
    return 0;
}