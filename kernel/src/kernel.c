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
int UMV_crearSegmento(int tamanio);
void UMV_enviarBytes(int base, int offset, int tamanio, void* buffer);
void Programa_imprimirTexto(char* texto);
void pasarAReady(void);
//int cantidadDeLineas(char* string);


/* Variables Globales */
void* l_new = NULL;
void* l_ready = NULL;
void* l_exec = NULL;
void* l_exit = NULL;
t_medatada_program* metadata;
int tamanioStack;
int ultimoPid = 0;

/* Semáforos */
int s_Multiprogramacion; //Semáforo del grado de Multiprogramación. Deja pasar a Ready los PCB Disponibles.

int main(void) {
	pthread_t hiloPCP, hiloPLP;
	int rhPCP, rhPLP;
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
	int escucharConexiones;
	printf("Inicio del PLP.\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;		// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP
	getaddrinfo(NULL, PUERTO, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE
	escucharConexiones = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	bind(escucharConexiones,serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(escucharConexiones, BACKLOG);		// IMPORTANTE: listen() es una syscall BLOQUEANTE.
	struct sockaddr_in programa;			// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(programa);
	int socketCliente = accept(escucharConexiones, (struct sockaddr *) &programa, &addrlen);
	char package[PACKAGESIZE];
	int status = 1;		// Estructura que maneja el status de los recieve.
	printf("Proceso Programa conectado. Recibiendo codigo.\n");

	while (status != 0){
		status = recv(socketCliente, (void*) package, PACKAGESIZE, 0);
		if (status != 0)
		{
			t_pcb* nuevoPCB;
			nuevoPCB = crearPcb(package);
			printf("Nuevo PCB Creado\n");
		}
	}
	return 0;
}



t_pcb* crearPcb(char* codigo)
{
	t_pcb* pcbAux = malloc (sizeof(t_pcb));
	t_medatada_program* metadataAux = metadata_desde_literal(codigo);
	pcbAux->pid = generarPid();
	pcbAux->programCounter = metadataAux->instruccion_inicio;
	pcbAux->tamanioIndiceEtiquetas = metadataAux->cantidad_de_etiquetas;
	pcbAux->segmentoStack = 0; //TODO: UMV_crearSegmento(tamanioStack);
	pcbAux->cursorStack = pcbAux->segmentoStack;
	pcbAux->segmentoCodigo = 0; // todo: UMV_crearSegmento(sizeof(*codigo));
	pcbAux->tamanioContextoActual = 0;
	pcbAux->indiceCodigo = 0; // todo: UMV_crearSegmento(metadataAux->instrucciones_size)*(sizeof(t_intructions));
	pcbAux->indiceEtiquetas = 0; // todo: UMV_crearSegmento(metadataAux->etiquetas_size);
	pcbAux->peso = (5* metadataAux->cantidad_de_etiquetas) + (3* metadataAux->cantidad_de_funciones);
	if(pcbAux->segmentoStack == 0 || pcbAux->segmentoCodigo == 0 || pcbAux->indiceCodigo == 0 || pcbAux->indiceEtiquetas == 0)
	{
		//avisar al programa :D
		Programa_imprimirTexto("Holis, No se pudo crear el programa");
	}
	//UMV_enviarBytes(pcbAux->segmentoCodigo,0,sizeof(*codigo),codigo);
	//UMV_enviarBytes(pcbAux->indiceEtiquetas,0,metadataAux->etiquetas_size,metadataAux->etiquetas);
	//UMV_enviarBytes(pcbAux->indiceCodigo,0,(metadataAux->instrucciones_size)*(sizeof(t_intructions)),metadataAux->instrucciones_serializado);
	return pcbAux;
}

int generarPid(void)
{
	ultimoPid++;
	return ultimoPid;
}

int UMV_crearSegmento(int tamanio)
{
	return 0;
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

//int cantidadDeLineas(char* string)
//{
//	char c;
//	int cantLineas = 0;
//	  while (string != EOF)
//	  {
//		  if (c == '\n')
//	      ++cantLineas;
//	  }
//	  return cantLineas;
//}

