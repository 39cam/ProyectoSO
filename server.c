#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define DEFAULT_IP "192.168.0.10"
#define DEFAULT_PORT 80
#define BUFFER_SIZE 2048
#define CACHE_SIZE 3

typedef struct {
    char path[256];
    char *content;
    long size;
    int hits;
} CacheEntry;

CacheEntry cache[CACHE_SIZE] = {0};
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

CacheEntry* cache_lookup(const char *path) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (strcmp(cache[i].path, path) == 0 && cache[i].content != NULL) {
            cache[i].hits++;
            return &cache[i];
        }
    }
    return NULL;
}

void cache_insert(const char *path, const char *content, long size) {
    int min_hits = cache[0].hits, idx = 0;
    for (int i = 1; i < CACHE_SIZE; i++) {
        if (cache[i].hits < min_hits) {
            min_hits = cache[i].hits;
            idx = i;
        }
    }
    free(cache[idx].content);
    strcpy(cache[idx].path, path);
    cache[idx].content = malloc(size);
    memcpy(cache[idx].content, content, size);
    cache[idx].size = size;
    cache[idx].hits = 1;
}

void log_connection(struct sockaddr_in *client_addr) {
    FILE *logf = fopen("server.log", "a");
    if (!logf) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strlen(timestamp)-1] = 0; // Remove newline
    fprintf(logf, "%s:%d - %s\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), timestamp);
    fclose(logf);
}

void send_file(int client_socket, const char *file_path, const char *content_type) {
    FILE *fp = fopen(file_path, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        rewind(fp);
        char *content = malloc(fsize);
        fread(content, 1, fsize, fp);
        fclose(fp);

        char header[256];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: %s\r\n\r\n", fsize, content_type);
        send(client_socket, header, strlen(header), 0);
        send(client_socket, content, fsize, 0);

        free(content);
    } else {
        char *notfound = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        send(client_socket, notfound, strlen(notfound), 0);
    }
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr *)&client_addr, &client_len);

    log_connection(&client_addr);

    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread = read(client_socket, buffer, BUFFER_SIZE-1);
    if (valread <= 0) {
        close(client_socket);
        pthread_exit(NULL);
    }

    char method[8], path[256];
    sscanf(buffer, "%s %s", method, path);

    // Map path to file
    char file_path[256];
    if (strcmp(path, "/") == 0) strcpy(file_path, "index.htm");
    else if (strcmp(path, "/quienes") == 0) strcpy(file_path, "quienes.htm");
    else if (strcmp(path, "/productos") == 0) strcpy(file_path, "productos.htm");
    else if (strcmp(path, "/contacto") == 0) strcpy(file_path, "contacto.htm");
    else if (strcmp(path, "/sedes") == 0) strcpy(file_path, "sedes.htm");
    else strcpy(file_path, "404.htm");

    // Buffer/cache
    pthread_mutex_lock(&cache_mutex);
    CacheEntry *entry = cache_lookup(file_path);
    pthread_mutex_unlock(&cache_mutex);

    if (entry) {
        char header[256];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: text/html\r\n\r\n", entry->size);
        send(client_socket, header, strlen(header), 0);
        send(client_socket, entry->content, entry->size, 0);
    } else {
        FILE *fp = fopen(file_path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            rewind(fp);
            char *content = malloc(fsize);
            fread(content, 1, fsize, fp);
            fclose(fp);

            pthread_mutex_lock(&cache_mutex);
            cache_insert(file_path, content, fsize);
            pthread_mutex_unlock(&cache_mutex);

            char header[256];
            sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: text/html\r\n\r\n", fsize);
            send(client_socket, header, strlen(header), 0);
            send(client_socket, content, fsize, 0);

            free(content);
        } else {
            char *notfound = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
            send(client_socket, notfound, strlen(notfound), 0);
        }
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    char *ip = DEFAULT_IP;
    int port = DEFAULT_PORT;
    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 1. Crear el socket del servidor: AF_INET (IPv4), SOCK_STREAM (TCP)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket del servidor");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR permite reutilizar el puerto inmediatamente después de cerrar el socket
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);

    // 2. Asignar una dirección al socket (bind)
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en el bind");
        exit(EXIT_FAILURE);
    }

    // 3. Poner el socket en modo de escucha (listen)
    if (listen(server_fd, 10) < 0) {
        perror("Error en el listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en %s:%d...\n", ip, port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *new_socket = malloc(sizeof(int));
        *new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (*new_socket < 0) {
            perror("Error en el accept");
            free(new_socket);
            continue;
        }
        printf("Conexión aceptada desde %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, new_socket);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}