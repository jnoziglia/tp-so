/*
 ============================================================================
 Name        : cpu.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

/* #include <stdio.h>
#include <stdlib.h>

int main(void) {



	//permanente contacto con el Proceso UMV
	// se conecta con el proceso Kernel y queda a la espera de que el PCP le envie el PCB del programa
	// incrementar valor del registro PC del pcb
	// utilizara el indice codigo para soliciar a UMV proxima sentencia
	// parsea la sentencia
	//ejecuar operaciones requeridas
	// actualizar los segmentos del programa en la UMV
	// actualizar PC del pcb
	// notificar PCP que concluyo un quantum

	int* crearConexionConElKernel()
	{
		int* descriptor = socket(); //crear socket
		bind(); //indicar numero de servicio que queremos atender
		listen(); //avisar al sistema que atienda dicha conexion
		accept(); //pide y acepta conexiones de clientes al sistema operativo
		write(); //escribe datos del cliente
		read(); //recibe datos del cliente
		close(); //cierre de la comunicacion/socket


		return descriptor;

	}




} */


#include <stdio.h>
#include <stdlib.h>
#include <commons/string.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <commons/config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>


#define PUERTO "6668"
#define BACKLOG 5	// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
#define PACKAGESIZE 1024 	// Define cual va a ser el size maximo del paquete a enviar

int main(){

	int crearConexionConElKernel(){
		struct addrinfo hints;
		struct addrinfo *serverInfo;
		int escucharConexiones;

		printf("Inicio del CPU.\n");
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
		hints.ai_flags = AI_PASSIVE;		// Asigna el address del localhost: 127.0.0.1
		hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

		getaddrinfo(NULL, PUERTO, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE
		escucharConexiones = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
		bind(escucharConexiones,serverInfo->ai_addr, serverInfo->ai_addrlen);
		printf("Bienvenido a la escucha\n");

		freeaddrinfo(serverInfo); // Ya no lo vamos a necesitar

		listen(escucharConexiones, BACKLOG);	// IMPORTANTE: listen() es una syscall BLOQUEANTE.
		struct sockaddr_in addr;	// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
		socklen_t addrlen = sizeof(addr);

		int socketKernel = accept(escucharConexiones,(struct sockaddr *) &addr, &addrlen);
		char package[PACKAGESIZE];
		int status = 1;

		while (status != 0){
		status = recv(socketKernel, (void*)package, PACKAGESIZE, 0);
		if (status != 0) printf("%s", package);
		}

		//cerramos conexiones
		close(socketKernel);
		close(escucharConexiones);

		return 0;
	}

}
