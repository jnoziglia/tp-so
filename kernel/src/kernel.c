/*
 ============================================================================
 Name        : kernel.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <parser/metadata_program.h>
#include <commons/string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

/* Definiciones y variables para la conexión por Sockets */
#define PUERTO "6667"
#define BACKLOG 5			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
#define PACKAGESIZE 1024	// Define cual va a ser el size maximo del paquete a enviar
#define PUERTOUMV "6668"
#define IPUMV "127.0.0.1"
#define PUERTOCPU "6667"
#define IPCPU "127.0.0.1"

/* Estructuras de datos */
typedef struct pcb
{
	int pid;						//Lo creamos nosotros
	int segmentoCodigo;				//Direccion al inicio de segmento de codigo (base)
	int segmentoStack;				//Direccion al inicio de segmento de stack
	int cursorStack;				//Cursor actual del stack (contexto actual)
	int indiceCodigo;				//Direccion al inicio de indice de codigo
	int indiceEtiquetas;			//
	int programCounter;
	int tamanioContextoActual;
	int tamanioIndiceEtiquetas;
	int peso;
}t_pcb;


/* Funciones */
void* f_hiloPCP();
void* f_hiloPLP();
t_pcb* crearPcb(char* codigo);
int generarPid(void);
void UMV_enviarBytes(int base, int offset, int tamanio, void* buffer);
void Programa_imprimirTexto(char* texto);
void pasarAReady(void);
void conexionCPU(void);
void conexionUMV(void);
int* UMV_crearSegmentos(int mensaje[5], int* info);



/* Variables Globales */
void* l_new = NULL;
void* l_ready = NULL;
void* l_exec = NULL;
void* l_exit = NULL;
t_medatada_program* metadata;
int tamanioStack = 5;
int ultimoPid = 1;
int socketUMV;
int socketCPU; //VER
fd_set readfds;


/* Semáforos */
int s_Multiprogramacion; //Semáforo del grado de Multiprogramación. Deja pasar a Ready los PCB Disponibles.

int main(void) {
	pthread_t hiloPCP, hiloPLP;
	int rhPCP, rhPLP;
	conexionUMV();
	//rhPCP = pthread_create(&hiloPCP, NULL, f_hiloPCP, NULL);
	rhPLP = pthread_create(&hiloPLP, NULL, f_hiloPLP, NULL);
	//pthread_join(hiloPCP, NULL);
	pthread_join(hiloPLP, NULL);
	//printf("%d",rhPCP);
	printf("%d",rhPLP);

	return 0;
}


void* f_hiloPCP()
{
	return 0;
}

void* f_hiloPLP()
{
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	int socketServidor;
	int socketCliente[10];
	fd_set readfds;
	int i, status, maximo = -1;
	int j = 1;
	char package[PACKAGESIZE];
	printf("Inicio del PLP.\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;		// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP
	getaddrinfo(NULL, PUERTO, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE
	struct sockaddr_in programa;			// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(programa);

	socketServidor = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	bind(socketServidor,serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(socketServidor, BACKLOG);

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET (socketServidor, &readfds);
		for (i=0; i<=maximo; i++)
		{
			FD_SET (socketCliente[i], &readfds);
			printf("socket %d\n",socketCliente[i]);
		}

		select(socketServidor + j, &readfds, NULL, NULL, NULL);

		for(i=0; i<=maximo; i++)
		{
			if(FD_ISSET(socketCliente[i], &readfds))
			{
				status = recv(socketCliente[i], (void*)package, PACKAGESIZE, 0);
				printf("Llegó acá.\n");printf("Codigo Recibido. %d\n", status);
				if (status != 0)
				{
					t_pcb* nuevoPCB;
					printf("Llegó acá.\n");
					printf("%s", package);
					nuevoPCB = crearPcb(package);
					if(nuevoPCB != NULL)
					{
						printf("Nuevo PCB Creado\n");
					}
				}
			}
		}

		if(FD_ISSET(socketServidor, &readfds))
		{
			printf("socket serv %d\n", socketServidor);
			maximo++;
			j++;
			socketCliente[maximo] = accept(socketServidor, (struct sockaddr *) &programa, &addrlen);
		}
	}
	return NULL;
}

	/*escucharConexiones = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	bind(escucharConexiones,serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(escucharConexiones, BACKLOG);		// IMPORTANTE: listen() es una syscall BLOQUEANTE.

	FD_SET(escucharConexiones, &readfds);

	struct sockaddr_in programa;			// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(programa);

	int socketCliente = accept(escucharConexiones, (struct sockaddr *) &programa, &addrlen);
	char package[PACKAGESIZE];
	int status = 1;		// Estructura que maneja el status de los recieve.
	printf("Proceso Programa conectado. Recibiendo codigo.\n");

	//while (status != 0){
		status = recv(socketCliente, (void*) package, PACKAGESIZE, 0);
		printf("Codigo Recibido. %d\n", status);
		if (status != 0)
		{
			t_pcb* nuevoPCB;
			printf("Llegó acá.\n");
			printf("%s", package);
			nuevoPCB = crearPcb(package);
			if(nuevoPCB != NULL)
			{
				printf("Nuevo PCB Creado\n");
			}
		}
	//}
	select(escucharConexiones+1, &readfds, NULL, NULL, NULL);
	return 0;
}*/



t_pcb* crearPcb(char* codigo)
{
	t_pcb* pcbAux = malloc (sizeof(t_pcb));
	t_medatada_program* metadataAux = metadata_desde_literal(codigo);
	int mensaje[6];
	int* info = malloc (4*sizeof(int));
	pcbAux->pid = generarPid();
	pcbAux->programCounter = metadataAux->instruccion_inicio;
	pcbAux->tamanioIndiceEtiquetas = metadataAux->cantidad_de_etiquetas;
	pcbAux->cursorStack = pcbAux->segmentoStack;
	pcbAux->tamanioContextoActual = 0;
	mensaje[1] = pcbAux->pid;
	mensaje[2] = tamanioStack;
	mensaje[3] = sizeof(*codigo);
	mensaje[4] = (metadataAux->instrucciones_size)*(sizeof(t_intructions));
	mensaje[5] = metadataAux->etiquetas_size;
	info = UMV_crearSegmentos(mensaje, info);
	pcbAux->peso = (5* metadataAux->cantidad_de_etiquetas) + (3* metadataAux->cantidad_de_funciones);
	if(info == NULL)
	{
		//avisar al programa :D
		Programa_imprimirTexto("Holis, No se pudo crear el programa");
		printf("no se creo el pcb\n");
		free (pcbAux);
		return NULL;
	}
	else
	{
		printf("Se crea el pcb");
		//UMV_enviarBytes(pcbAux->segmentoCodigo,0,sizeof(*codigo),codigo);
		//UMV_enviarBytes(pcbAux->indiceEtiquetas,0,metadataAux->etiquetas_size,metadataAux->etiquetas);
		//UMV_enviarBytes(pcbAux->indiceCodigo,0,(metadataAux->instrucciones_size)*(sizeof(t_intructions)),metadataAux->instrucciones_serializado);
		return pcbAux;
	}
}

int* UMV_crearSegmentos(int mensaje[6], int* info)
{
	int status = 1;
	//int* info[4];
	mensaje[0] = 1;
	send(socketUMV, mensaje, 5*sizeof(int), 0);
	status = recv(socketUMV, info, 4*sizeof(int), 0);
	printf("status : %d\n",status);
	if (info[0] == -1)
	{
		return NULL;
	}
	else
	{
		return info;
	}
}

int generarPid(void)
{
	ultimoPid++;
	return ultimoPid;
}


void UMV_enviarBytes(int base, int offset, int tamanio, void* buffer)
{

}

void Programa_imprimirTexto(char* texto)
{

}

void pasarAReady(void)
{

}

void conexionCPU(void)
{
		struct addrinfo hintsCPU;
		struct addrinfo *cpuInfo;

		memset(&hintsCPU, 0, sizeof(hintsCPU));
		hintsCPU.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
		hintsCPU.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

		getaddrinfo(IPCPU, PUERTOCPU, &hintsCPU, &cpuInfo);	// Carga en umvInfo los datos de la conexion

		socketCPU = socket(cpuInfo->ai_family, cpuInfo->ai_socktype, cpuInfo->ai_protocol);

		connect(socketCPU, cpuInfo->ai_addr, cpuInfo->ai_addrlen);
		printf("Conexión con la CPU: %d", socketCPU);
		freeaddrinfo(cpuInfo);	// No lo necesitamos mas

		//ENVIAR DATOS AL CPU

		return;
}

void conexionUMV(void)
{
		struct addrinfo hintsumv;
		struct addrinfo *umvInfo;

		memset(&hintsumv, 0, sizeof(hintsumv));
		hintsumv.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
		hintsumv.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

		getaddrinfo(IPUMV, PUERTOUMV, &hintsumv, &umvInfo);	// Carga en umvInfo los datos de la conexion

		socketUMV = socket(umvInfo->ai_family, umvInfo->ai_socktype, umvInfo->ai_protocol);

		connect(socketUMV, umvInfo->ai_addr, umvInfo->ai_addrlen);
		printf("Conexión con la UMV: %d", socketUMV);
		freeaddrinfo(umvInfo);	// No lo necesitamos mas
//		int idKernel = 0;
//		int confirmacion;
//		send(socketUMV, (void*)idKernel, sizeof(int), 0);
//		printf("Enviado Tipo:Kernel\n");
//		recv(socketUMV, (void*)confirmacion, sizeof(int),0);
//		printf("Confirmación: %d \n",confirmacion);

		return;
}
