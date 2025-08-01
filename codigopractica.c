#include <stdio.h> 

#include <stdlib.h> 

#include <string.h> 

#include <unistd.h> // Para close() 

#include <arpa/inet.h> // Para socket, bind, listen, accept, sockaddr_in 

 

#define PORT 8088 

#define BUFFER_SIZE 1024 

 

int main() { 

int server_fd, new_socket; 

struct sockaddr_in address; 

int addrlen = sizeof(address); 

char buffer[BUFFER_SIZE] = {0}; 

const char *hello = "¡Hola desde el servidor!"; 

 

// 1. Crear el socket del servidor: AF_INET (IPv4), SOCK_STREAM (TCP) 

if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 

perror("Error al crear el socket del servidor"); 

exit(EXIT_FAILURE); 

} 

 

// Configurar la dirección del servidor 

address.sin_family = AF_INET; 

address.sin_addr.s_addr = INADDR_ANY; // Escuchar en cualquier interfaz disponible 

address.sin_port = htons(PORT); // Convertir el puerto a network byte order 

 

// 2. Asignar una dirección al socket (bind) 

// SO_REUSEADDR permite reutilizar el puerto inmediatamente después de cerrar el socket 

int opt = 1; 

if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) { 

perror("setsockopt failed"); 

exit(EXIT_FAILURE); 

} 

 

if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { 

perror("Error en el bind"); 

exit(EXIT_FAILURE); 

} 

 

// 3. Poner el socket en modo de escucha (listen) 

// El 3 es el tamaño de la cola de conexiones pendientes 

if (listen(server_fd, 3) < 0) { 

perror("Error en el listen"); 

exit(EXIT_FAILURE); 

} 

 

printf("Servidor escuchando en el puerto %d...\n", PORT); 

 

while (1) { // Bucle infinito para aceptar múltiples conexiones (secuencialmente) 

// 4. Aceptar una conexión entrante (accept) 

// Esto bloquea hasta que un cliente se conecta 

if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 

perror("Error en el accept"); 

exit(EXIT_FAILURE); 

} 

printf("Conexión aceptada desde %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 

 

// 5. Leer datos del cliente 

ssize_t valread = read(new_socket, buffer, BUFFER_SIZE); 

if (valread < 0) { 

perror("Error al leer del socket"); 

} else if (valread == 0) { 

printf("Cliente desconectado (read returned 0).\n"); 

} else { 

printf("Mensaje del cliente: %s\n", buffer); 

} 

 

// 6. Enviar respuesta al cliente 

send(new_socket, hello, strlen(hello), 0); 

printf("Mensaje enviado al cliente.\n"); 

 

// 7. Cerrar el socket de conexión con el cliente actual 

close(new_socket); 

printf("Conexión con el cliente cerrada.\n"); 

 

memset(buffer, 0, BUFFER_SIZE); // Limpiar buffer para la próxima conexión 

} 

 

// Cierre del socket del servidor (normalmente no se llega aquí en un bucle infinito) 

close(server_fd); 

return 0; 

} 