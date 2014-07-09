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
#include <commons/config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>

/* Definiciones y variables para la conexión por Sockets */
//#define PUERTOPROGRAMA "6667"
//#define BACKLOG 5			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
//#define PACKAGESIZE 1024	// Define cual va a ser el size maximo del paquete a enviar
//#define PUERTOUMV "6668"
//#define IPUMV "127.0.0.1"
//#define PUERTOCPU "6680"
//#define IPCPU "127.0.0.1"

/* Estructuras de datos */
typedef struct pcb
{
	int pid;						//Lo creamos nosotros
	int segmentoCodigo;				//Direccion al inicio de segmento de codigo (base)
	int segmentoStack;				//Direccion al inicio de segmento de stack
	int cursorStack;				//Cursor actual del stack (contexto actual) Es el offset con respecto al stack.
	int indiceCodigo;				//Direccion al inicio de indice de codigo
	int indiceEtiquetas;			//
	int programCounter;
	int tamanioContextoActual;
	int tamanioIndiceEtiquetas;
	int tamanioIndiceCodigo;
	int peso;
	struct pcb *siguiente;
}t_pcb;

typedef struct new
{
	int pid;						//Lo creamos nosotros
	char* codigo;
	struct new *siguiente;
}t_new;

typedef struct  variableCompartida
{
	char* nombreVariable;
	int valorVariable;
}t_variableCompartida;

enum
{
	new,
	ready,
	exec,
	finish
};


/* Funciones */
void* f_hiloPCP();
void* f_hiloPLP();
int crearPcb(t_new programa, t_pcb* pcbAux);
void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer);
char* serializarEnvioBytes(int pid, int base, int offset, int tamanio, void* buffer);
void Programa_imprimirTexto(char* texto);
void encolarEnNew(t_new* programa);
void conexionCPU(void);
void conexionUMV(void);
int UMV_crearSegmentos(int mensaje[2]);
void UMV_destruirSegmentos(int pid);
void* f_hiloMostrarNew();
void serializarPcb(t_pcb* pcb, void* package);
t_pcb recibirSuperMensaje ( int superMensaje[11] );
void cargarConfig(void);
void destruirPCB(int pid);
void* f_hiloColaReady();
t_new desencolarNew(void);
void encolarEnReady(t_pcb* pcb);
t_pcb* desencolarReady(void);
void encolarExec(t_pcb* pcb);
void desencolarExec(t_pcb* pcb);
void encolarExit(t_pcb* pcb);
t_pcb readyAExec(void);
void execAReady(t_pcb pcb);
void execAExit(t_pcb pcb);
void cargarVariablesCompartidas(void);

/* Variables Globales */
t_new* l_new = NULL;
t_pcb* l_ready = NULL;
t_pcb* l_exec = NULL;
t_pcb* l_exit = NULL;
t_medatada_program* metadata;
int tamanioStack;
int socketUMV;
//int socketCPU; //VER
fd_set fdWPCP;
fd_set fdRPCP;
fd_set readPCP;
fd_set writePCP;
fd_set fdRPLP;
fd_set fdWPLP;
fd_set readPLP;
fd_set writePLP;
char* PUERTOPROGRAMA;
int BACKLOG;			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
int	PACKAGESIZE;	// Define cual va a ser el size maximo del paquete a enviar
char* PUERTOUMV;
char* IPUMV;
char* PUERTOCPU;
char* IPCPU;
t_variableCompartida* arrayVariablesCompartidas;
int cantidadVariableCompartidas = 0;


/* Semáforos */
sem_t s_Multiprogramacion; //Semáforo del grado de Multiprogramación. Deja pasar a Ready los PCB Disponibles.
sem_t s_ColaReady;
sem_t s_ColaExit;
sem_t s_ColaNew;
sem_t s_ComUmv;


/*Archivo de Configuración*/
t_config* configuracion;

int main(void) {
	pthread_t hiloPCP, hiloPLP, hiloMostrarNew, hiloColaReady;
	int rhPCP, rhPLP, rhMostrarNew, rhColaReady;
	sem_init(&s_Multiprogramacion,0,4);
	sem_init(&s_ColaReady,0,1);
	sem_init(&s_ColaExit,0,1);
	sem_init(&s_ColaNew,0,1);
	sem_init(&s_ComUmv,0,1);
	configuracion = config_create("/home/utnso/tp-2014-1c-unnamed/kernel/src/config.txt");
	cargarConfig();
	cargarVariablesCompartidas();

	printf("Puerto %s\n", PUERTOPROGRAMA);
	conexionUMV();
	rhPCP = pthread_create(&hiloPCP, NULL, f_hiloPCP, NULL);
	rhPLP = pthread_create(&hiloPLP, NULL, f_hiloPLP, NULL);
	rhMostrarNew = pthread_create(&hiloMostrarNew, NULL, f_hiloMostrarNew, NULL);
	rhColaReady = pthread_create(&hiloColaReady, NULL, f_hiloColaReady, NULL);
	pthread_join(hiloPCP, NULL);
	pthread_join(hiloPLP, NULL);
	pthread_join(hiloMostrarNew, NULL);
	//printf("%d",rhPCP);
	//printf("%d",rhPLP);

	return 0;
}


void* f_hiloColaReady()
{
	t_new programa;
	int conf;
	while(1)
	{
		if(l_new != NULL)
		{
			sem_wait(&s_Multiprogramacion);
			programa = desencolarNew();
			t_pcb* nuevoPCB = malloc(sizeof(t_pcb));
			conf = crearPcb(programa, nuevoPCB);
			if(conf == 1)
			{
				printf("Nuevo PCB Creado\n");
				encolarEnReady(nuevoPCB);
			}
		}
	}
}


void* f_hiloPCP()
{
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	int socketPCP, socketAux;
	int i, j, maximo = 0;
	int superMensaje[11];
	int status = 1;
	char mensaje;
	t_pcb pcbAEnviar;
	t_pcb pcbRecibido;
	//void* package = malloc(sizeof(t_pcb));
	//t_pcb* pcb;
	//t_pcb* puntero;
	printf("Inicio del PCP.\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;		// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP
	getaddrinfo(NULL, PUERTOCPU, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE
	struct sockaddr_in conexioncpu;			// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(conexioncpu);

	socketPCP = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	maximo = socketPCP;
	bind(socketPCP,serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(socketPCP, BACKLOG);

	FD_ZERO(&fdRPCP);
	FD_ZERO(&fdWPCP);
	FD_ZERO(&readPCP);
	FD_ZERO(&writePCP);

	FD_SET (socketPCP, &fdRPCP);

	while(1)
	{
		FD_ZERO(&readPCP);
		FD_ZERO(&writePCP);
		readPCP = fdRPCP;
		writePCP = fdWPCP;
		select(maximo + 1, &readPCP, &writePCP, NULL, NULL);
		for(i=3; i<=maximo; i++)
		{
			if(FD_ISSET(i, &readPCP))
			{
				if(i == socketPCP)
				{
					printf("Nueva CPU: %d\n", socketPCP);
					socketAux = accept(socketPCP, (struct sockaddr *) &conexioncpu, &addrlen);
					FD_SET(socketAux, &fdWPCP);
					FD_SET(socketAux, &fdRPCP);
					if (socketAux > maximo) maximo = socketAux;
				}
				else
				{
//					FD_CLR(i, &fdRPCP);
//					FD_SET(i, &fdWPCP);
					//Cuando no es un CPU, recibe el PCB o se muere el programa;
					recv(i,&mensaje,sizeof(char),0);
					//recv(i,&superMensaje,sizeof(superMensaje),0);
					//t_pcb* pcb = malloc(sizeof(t_pcb));
					//pcbRecibido = recibirSuperMensaje(superMensaje);
					//desencolarExec(pcb);
					//pcb->siguiente = NULL;
					if(mensaje == 0) //todo:podria ser ENUM
					{
						//Se muere el programa
						recv(i,&superMensaje,sizeof(superMensaje),0);
						pcbRecibido = recibirSuperMensaje(superMensaje);
						printf("Llegó un programa para encolar en Exit\n");
						//encolarEnExit
						execAExit(pcbRecibido);
					}
					else if (mensaje == 1)
					{
						//Se termina el quantum o va a block
						recv(i,&superMensaje,sizeof(superMensaje),0);
						pcbRecibido = recibirSuperMensaje(superMensaje);
						printf("Llegó un programa para encolar en Ready\n");
						execAReady(pcbRecibido);
					}
					else if(mensaje == 2) //todo:podria ser ENUM
					{
						//Manejo de variables compartidas.
						printf("Llegó un programa para manejar variables compartidas.\n");
						char mensaje2;
						recv(i,&mensaje2,sizeof(char),0);
						printf("Operación: %c\n", mensaje2);
						int tamanio = 0, valorVariable = -1;
						recv(i,&tamanio,sizeof(int),0);
						char* variable = malloc(tamanio);
						printf("Tamanio del nombre de la variable: %d\n",tamanio);
						recv(i,variable,tamanio,0);
						printf("Nombre de la variable: %s\n",variable);
						if(mensaje2 == 0)
						{
							//Obtener valor variable compartida.
							printf("Obtener valor variable compartida %s\n", variable);
							int j;
							for(j = 0; j < cantidadVariableCompartidas; j++)
							{
								printf("Busco las variables: Buscada: %s, actual: %s\n", variable, arrayVariablesCompartidas[j].nombreVariable);
								if(string_equals_ignore_case(arrayVariablesCompartidas[j].nombreVariable, variable))
								{
									valorVariable = arrayVariablesCompartidas[j].valorVariable;
									printf("Variable Global pedida: %s Valor %d\n",variable,valorVariable);
								}
							}
							send(i,&valorVariable,sizeof(int),0);
							free(variable);
						}
						else if (mensaje2 == 1)
						{
							//Asignar variable compartida.
							recv(i,&valorVariable,sizeof(int),0);
							printf("Asignar valor variable compartida: %s\n", variable);
							int j;
							for(j = 0; j < cantidadVariableCompartidas; j++)
							{
								printf("Busco las variables: Buscada: %s, actual: %s\n", variable, arrayVariablesCompartidas[j].nombreVariable);
								if(string_equals_ignore_case(variable, arrayVariablesCompartidas[j].nombreVariable))
								{
									 arrayVariablesCompartidas[j].valorVariable = valorVariable;
									 printf("Variable Global asignada: %s Valor %d\n",variable,valorVariable);
								}
							}
							//send(i,&valorVariable,sizeof(int),0);
							free(variable);
						}
						else
						{
							//Error.
							printf("Error accediendo a variables compartidas.\n");
						}
					}
				}
			}
			else if(FD_ISSET(i, &writePCP))
			{
//				//FD_SET(i, &writePCP);
//				//Sacar de Ready
				if(l_ready != NULL)	//TODO: PONER SEMAFORO!!!
				{
////					FD_CLR(i, &fdWPCP);
////					FD_SET(i, &fdRPCP);
					pcbAEnviar = readyAExec();
//					t_pcb* pcbAux;
//					pcbAux = desencolarReady();
					superMensaje[0] = pcbAEnviar.pid;
					superMensaje[1] = pcbAEnviar.segmentoCodigo;
					superMensaje[2] = pcbAEnviar.segmentoStack;
					superMensaje[3] = pcbAEnviar.cursorStack;
					superMensaje[4] = pcbAEnviar.indiceCodigo;
					superMensaje[5] = pcbAEnviar.indiceEtiquetas;
					superMensaje[6] = pcbAEnviar.programCounter;
					superMensaje[7] = pcbAEnviar.tamanioContextoActual;
					superMensaje[8] = pcbAEnviar.tamanioIndiceEtiquetas;
					superMensaje[9] = pcbAEnviar.tamanioIndiceCodigo;
					superMensaje[10] = pcbAEnviar.peso;
//					//free(l_new);
//					//l_new = NULL;
					status = send(i, superMensaje, 11*sizeof(int), 0);
//					encolarExec(pcbAux);
				}

			}
		}
	}
}

void* f_hiloPLP()
{
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	int socketServidor;
	int socketAux;
	int i, j, status, maximo = 0;
	int maximoAnterior;
	char package[PACKAGESIZE], mensajePrograma;
	t_pcb* listaExit;
	printf("Inicio del PLP.\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;		// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP
	getaddrinfo(NULL, PUERTOPROGRAMA, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE
	struct sockaddr_in programa;			// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(programa);

	socketServidor = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	maximo = socketServidor;
	bind(socketServidor,serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(socketServidor, BACKLOG);
	printf("SocketServidor: %d\n",socketServidor);
//
//	int socketllegado = accept(socketServidor, (struct sockaddr *) &programa, &addrlen);
//	printf("SocketLlegado: %d\n",socketllegado);
//
//	status = recv(socketllegado,(void*)package, PACKAGESIZE, 0);
//	printf("status: %d\n",status);
//
//	t_pcb* nuevoPCB;
//	nuevoPCB = crearPcb(package);
//	if(nuevoPCB != NULL)
//	{
//		printf("Nuevo PCB Creado\n");
//		encolarEnNew(nuevoPCB);
//	}


	FD_ZERO(&readPLP);
	FD_ZERO(&writePLP);
	FD_ZERO(&fdRPLP);
	FD_ZERO(&fdWPLP);

	FD_SET (socketServidor, &fdRPLP);

	while(1)
	{
		//maximoAnterior = 2;
		readPLP = fdRPLP;
		writePLP = fdWPLP;
		//if(FD_ISSET(7, &writePLP)) printf("HOLA %d\n", maximo);
		select(maximo + 1, &readPLP, &writePLP, NULL, NULL);
		//printf("salio del select\n");
		for(i=3; i<=maximo; i++)
		{
			if(FD_ISSET(i, &readPLP))
			{
				if(i == socketServidor)
				{
					printf("conecto program %d\n", socketServidor);
					socketAux = accept(socketServidor, (struct sockaddr *) &programa, &addrlen);
					FD_SET(socketAux, &fdRPLP);
					if (socketAux > maximo) maximo = socketAux;
				}
				else
				{
					status = recv(i, (void*)package, PACKAGESIZE, 0);
					printf("Codigo Recibido. %d\n", status);
					if (status != 0)
					{
						FD_CLR(i, &fdRPLP);
						FD_SET(i, &fdWPLP);
						//t_pcb* nuevoPCB;
						//printf("%s", package);
						//nuevoPCB = crearPcb(package, i);
//						if(nuevoPCB != NULL)
//						{
//							printf("Nuevo PCB Creado\n");
//							encolarEnNew(nuevoPCB, new);
//						}
						package[status+1] = '\0';
						t_new* nodoNew = malloc(sizeof(t_new));
						nodoNew->pid = i;
						nodoNew->codigo = package;
						nodoNew->siguiente = NULL;
						encolarEnNew(nodoNew);

					}
					else
					{
						FD_CLR(i, &fdRPLP);
						close(i);
						if (i == maximo)
						{
							for(j=3; j<maximo; j++)
							{
								if(FD_ISSET(j, &fdRPLP) || FD_ISSET(j, &fdWPLP))
								{
									printf("baje maximo\n");
									maximoAnterior = j;
								}
							}
							maximo = maximoAnterior;
						}
					}
				}
			}
			else if (FD_ISSET(i, &writePLP))
			{
				//printf("entro al isset %d\n", i);
				listaExit = l_exit;
				while(listaExit != NULL)
				{
					if(i == listaExit->pid)
					{
						printf("entro al exit %d\n", i);
						mensajePrograma = 2;
						send(i, &mensajePrograma, sizeof(char), 0);
						UMV_destruirSegmentos(i);
						close(i);
						if (i == maximo)
						{
							for(j=3; j<maximo; j++)
							{
								if(FD_ISSET(j, &fdWPLP) || FD_ISSET(j, &fdRPLP))
								{
									printf("baje maximo\n");
									maximoAnterior = j;
								}
							}
							maximo = maximoAnterior;
						}
						FD_CLR(i, &fdWPLP);
						destruirPCB(i);
						sem_post(&s_Multiprogramacion);
						break;
					}
					else
					{
						listaExit = listaExit->siguiente;
					}
				}
			}
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



int crearPcb(t_new programa, t_pcb* pcbAux)
{
	//t_pcb* pcbAux = malloc (sizeof(t_pcb));
	t_medatada_program* metadataAux = metadata_desde_literal(programa.codigo);
	printf("%s\n", programa.codigo);
	int mensaje[2];
	int respuesta, i;
	pcbAux->pid = programa.pid;
	pcbAux->programCounter = metadataAux->instruccion_inicio;
	pcbAux->tamanioIndiceEtiquetas = metadataAux->etiquetas_size;
	pcbAux->cursorStack = 0;
	pcbAux->tamanioContextoActual = 0;
	pcbAux->siguiente = NULL;
	pcbAux->tamanioIndiceCodigo = (metadataAux->instrucciones_size*sizeof(t_intructions));
	mensaje[0] = pcbAux->pid;
	mensaje[1] = tamanioStack;
	printf("PID: %d\n", pcbAux->pid);
	printf("STACK: %d\n", tamanioStack);
	respuesta = UMV_crearSegmentos(mensaje);
	if(respuesta == -1)
	{
		printf("NO SE CREO EL SEGMENTO\n");
		//avisar al programa :D
		Programa_imprimirTexto("Holis, No se pudo crear el programa");
		//UMV_destruirSegmentos(pcbAux->pid);
		printf("no se creo el pcb\n");
		free (pcbAux);
		return -1;
	}
	pcbAux->segmentoStack = respuesta;
	mensaje[1] = strlen(programa.codigo);
	respuesta = UMV_crearSegmentos(mensaje);
	if(respuesta == -1)
	{
		//avisar al programa :D
		Programa_imprimirTexto("Holis, No se pudo crear el programa");
		UMV_destruirSegmentos(pcbAux->pid);
		printf("no se creo el pcb\n");
		free (pcbAux);
		return -1;
	}
	pcbAux->segmentoCodigo = respuesta;
	mensaje[1] = (metadataAux->instrucciones_size*sizeof(t_intructions));
	respuesta = UMV_crearSegmentos(mensaje);
	if(respuesta == -1)
	{
		//avisar al programa :D
		Programa_imprimirTexto("Holis, No se pudo crear el programa");
		UMV_destruirSegmentos(pcbAux->pid);
		printf("no se creo el pcb\n");
		free (pcbAux);
		return -1;
	}
	pcbAux->indiceCodigo = respuesta;
	mensaje[1] = metadataAux->etiquetas_size;
	if(mensaje[1] != 0)
	{
		respuesta = UMV_crearSegmentos(mensaje);
		if(respuesta == -1)
		{
			//avisar al programa :D
			Programa_imprimirTexto("Holis, No se pudo crear el programa");
			UMV_destruirSegmentos(pcbAux->pid);
			printf("no se creo el pcb\n");
			free (pcbAux);
			return -1;
		}
		pcbAux->indiceEtiquetas = respuesta;
	}
	else
	{
		pcbAux->indiceEtiquetas = -1;
	}
	printf("indiceEtiquetas %d\n", pcbAux->indiceEtiquetas);
	pcbAux->peso = (5* metadataAux->cantidad_de_etiquetas) + (3* metadataAux->cantidad_de_funciones) + (metadataAux->instrucciones_size);
	printf("Se crea el pcb\n");
	printf("codigo: %d\n", strlen(programa.codigo));
	UMV_enviarBytes(pcbAux->pid, pcbAux->segmentoCodigo,0,strlen(programa.codigo),programa.codigo);
	printf("Creado segmento Codigo con el tamaño %d\n",strlen(programa.codigo));
	if(metadataAux->etiquetas_size != 0)
	{
		UMV_enviarBytes(pcbAux->pid, pcbAux->indiceEtiquetas,0,metadataAux->etiquetas_size,metadataAux->etiquetas);
		printf("Creado indiceEtiquetas de tamaño %d\n", metadataAux->etiquetas_size);
	}
	UMV_enviarBytes(pcbAux->pid, pcbAux->indiceCodigo,0,pcbAux->tamanioIndiceCodigo,metadataAux->instrucciones_serializado);
	printf("Creado indiceCodigo de tamaño %d\n",pcbAux->tamanioIndiceCodigo);
	return 1;
}

int UMV_crearSegmentos(int mensaje[2])
{
	sem_wait(&s_ComUmv);
	int status = 1;
	char operacion = 1;
	char confirmacion;
	int respuesta;
	send(socketUMV, &operacion, sizeof(char), 0);
	recv(socketUMV, &confirmacion, sizeof(char), 0);
	if(confirmacion == 1)
	{
		send(socketUMV, mensaje, 2*sizeof(int), 0);
		status = recv(socketUMV, &respuesta, sizeof(int), 0);
		printf("CONFIRMACION : %d\n",respuesta);
		sem_post(&s_ComUmv);
		return respuesta;
	}
	sem_post(&s_ComUmv);
	return -1;
}

void UMV_destruirSegmentos(int pid)
{
	sem_wait(&s_ComUmv);
	char operacion = 2;
	char confirmacion;
	send (socketUMV, &operacion, sizeof(char), 0);
	recv(socketUMV, &confirmacion, sizeof(char), 0);
	if (confirmacion ==  1)
	{
		send(socketUMV, &pid, sizeof(int), 0);
		sem_post(&s_ComUmv);
		return;
	}
	sem_post(&s_ComUmv);
	return;
}



void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer)
{
	sem_wait(&s_ComUmv);
	int status;
	char operacion = 3;
	char confirmacion;
	char* package;
	send(socketUMV, &operacion, sizeof(char), 0);
	recv(socketUMV, &confirmacion, sizeof(char), 0);
	if(confirmacion == 1)
	{
		package = serializarEnvioBytes(pid, base, offset, tamanio, buffer);
		send(socketUMV, package, 4*sizeof(int) + tamanio, 0);
	}
	status = recv(socketUMV,&confirmacion, sizeof(char),0);
	if(status==0)
	{
		printf("Cerró\n");
		sem_post(&s_ComUmv);
		return;
	}
	sem_post(&s_ComUmv);
	return;
}

char* serializarEnvioBytes(int pid, int base, int offset, int tamanio, void* buffer)
{
	int despl = 0;
	char* package = malloc(4*sizeof(int)+tamanio);

	memcpy(package + despl, &pid, sizeof(int));
	despl += sizeof(int);

	memcpy(package + despl, &base, sizeof(int));
	despl += sizeof(int);

	memcpy(package + despl, &offset, sizeof(int));
	despl += sizeof(int);

	memcpy(package + despl, &tamanio, sizeof(int));
	despl += sizeof(int);

	memcpy(package + despl, (char*)buffer, tamanio);

	return package;
}

void Programa_imprimirTexto(char* texto)
{

}

void encolarEnNew(t_new* programa)
{
	sem_wait(&s_ColaNew);
	t_new* aux = l_new;
	printf("Encolando %d\n",programa->pid);
	if(aux == NULL)
	{
		l_new = programa;
		l_new->siguiente = NULL;
		sem_post(&s_ColaNew);
		return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = programa;
		sem_post(&s_ColaNew);
		return;
	}

//		if (l_new->peso > pcb->peso)
//		{
//			pcb->siguiente = l_new;
//			l_new = pcb;
//			return;
//		}
//		else
//		{
//			auxAnterior = l_new;
//			aux = auxAnterior->siguiente;
//			while (aux != NULL)
//			{
//				if (aux->peso > pcb->peso)
//				{
//					pcb->siguiente = aux;
//					auxAnterior->siguiente = pcb;
//					return;
//				}
//				else
//				{
//					auxAnterior = aux;
//					aux = aux->siguiente;
//				}
//			}
//			auxAnterior->siguiente = pcb;
//			pcb->siguiente = NULL;
//			return;
//		}
}


void conexionUMV(void)
{
		struct addrinfo hintsumv;
		struct addrinfo *umvInfo;
		char id = 0;
		char conf = 0;

		memset(&hintsumv, 0, sizeof(hintsumv));
		hintsumv.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
		hintsumv.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

		getaddrinfo(IPUMV, PUERTOUMV, &hintsumv, &umvInfo);	// Carga en umvInfo los datos de la conexion

		socketUMV = socket(umvInfo->ai_family, umvInfo->ai_socktype, umvInfo->ai_protocol);

		int a = connect(socketUMV, umvInfo->ai_addr, umvInfo->ai_addrlen);
		printf("Conexión con la UMV: %d", a);
		freeaddrinfo(umvInfo);	// No lo necesitamos mas
		send(socketUMV, &id, sizeof(char), 0);
		recv(socketUMV, &conf, sizeof(char), 0);
//		int idKernel = 0;
//		int confirmacion;
//		send(socketUMV, (void*)idKernel, sizeof(int), 0);
//		printf("Enviado Tipo:Kernel\n");
//		recv(socketUMV, (void*)confirmacion, sizeof(int),0);
//		printf("Confirmación: %d \n",confirmacion);
		return;
}

void* f_hiloMostrarNew()
{
	char* ingreso = malloc(100);
	while(1)
	{
		gets(ingreso);
		if(string_equals_ignore_case(ingreso,"mostrar-new"))
		{
			printf("Procesos encolados en New\n");
			printf("PID: \t\tPeso:\n");
			printf("-----------------------\n");
			t_new* aux = l_new;
			while(aux != NULL)
			{
				printf("%d\t\t",aux->pid);
				//printf("%d\n",aux->peso);
				aux = aux->siguiente;
			}
			continue;
		}
		else
		{
			printf("Error de comando.");
			continue;
		}
	}
	free(ingreso);
	return 0;
}

void serializarPcb(t_pcb* pcb, void* package)
{
	int offset = 0;
	//void* package = malloc(sizeof(t_pcb));

	memcpy(package, &(pcb->pid), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->programCounter), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->tamanioIndiceEtiquetas), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->cursorStack), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->tamanioContextoActual), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->siguiente), sizeof(t_pcb*));
	offset += sizeof(t_pcb*);

	memcpy(package, &(pcb->tamanioIndiceCodigo), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->segmentoStack), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->segmentoCodigo), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->indiceCodigo), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->indiceEtiquetas), sizeof(int));
	offset += sizeof(int);

	memcpy(package, &(pcb->peso), sizeof(int));
	offset += sizeof(int);

	return;
}

t_pcb recibirSuperMensaje ( int superMensaje[11] )
{
	t_pcb pcb;
//	int i;
//	t_pcb* pcb = l_exec;
//	while(pcb->pid != superMensaje[0] && pcb != NULL)
//	{
//		pcb= pcb->siguiente;
//	}
//	if(pcb == NULL) printf("El PCB no existe\n");
	pcb.pid = superMensaje[0];
	pcb.segmentoCodigo = superMensaje[1];
	pcb.segmentoStack=	superMensaje[2] ;
	pcb.cursorStack=superMensaje[3]  ;
	pcb.indiceCodigo=superMensaje[4] ;
	pcb.indiceEtiquetas=superMensaje[5]  ;
	pcb.programCounter=superMensaje[6] ;
	pcb.tamanioContextoActual=superMensaje[7] ;
	pcb.tamanioIndiceEtiquetas=superMensaje[8] ;
	pcb.tamanioIndiceCodigo=superMensaje[9] ;
	pcb.peso=superMensaje[10] ;

//	for(i=0; i<11; i++){
//		printf("pcb: %d\n", superMensaje[i]);
//	}
	return pcb;
}

void cargarConfig(void)
{
	//t_config* configuracion = config_create("/home/utnso/tp-2014-1c-unnamed/kernel/src/config.txt");
	PUERTOPROGRAMA = config_get_string_value(configuracion, "PUERTOPROGRAMA");
	BACKLOG = config_get_int_value(configuracion, "BACKLOG");			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
	PACKAGESIZE = config_get_int_value(configuracion, "PACKAGESIZE");	// Define cual va a ser el size maximo del paquete a enviar
	PUERTOUMV = config_get_string_value(configuracion, "PUERTOUMV");
	IPUMV = config_get_string_value(configuracion, "IPUMV");
	PUERTOCPU = config_get_string_value(configuracion, "PUERTOCPU");
	IPCPU = config_get_string_value(configuracion, "IPCPU");
	tamanioStack = config_get_int_value(configuracion, "TAMANIOSTACK");
}

void cargarVariablesCompartidas(void)
{
	char** vars = malloc(1000);
	vars = config_get_array_value(configuracion, "VARIABLES_COMPARTIDAS");
	int i;
	while(vars[cantidadVariableCompartidas] != NULL)
	{
		//printf("Variable compartida %d: %s\n",(cantTot+1), vars[cantTot]);
		cantidadVariableCompartidas++;
	}
	printf("Cantidad total de variables compartidas: %d\n",cantidadVariableCompartidas);
	arrayVariablesCompartidas = malloc(cantidadVariableCompartidas*sizeof(t_variableCompartida));
	for(i = 0; i < cantidadVariableCompartidas; i++)
	{
		arrayVariablesCompartidas[i].nombreVariable = vars[i];
		printf("NombreVar: %s\n", arrayVariablesCompartidas[i].nombreVariable);
	}
}

void destruirPCB(int pid)
{
	sem_wait(&s_ColaExit);
	t_pcb* listaExit = l_exit;
	t_pcb* listaAux = NULL;
	t_pcb* aux = l_exit;
	while(aux != NULL)
	{
		printf("PID en Exit: %d\n",aux->pid);
		aux = aux->siguiente;
	}
	if(listaExit->pid == pid)
	{
		listaAux = l_exit;
		l_exit = l_exit->siguiente;
		free(listaAux);
		printf("Destrui unico PCB\n");
		sem_post(&s_ColaExit);
		return;
	}
	listaAux = listaExit;
	listaExit = listaExit->siguiente;
	while(listaExit != NULL)
	{
		if(listaExit->pid == pid)
		{
			printf("destruyo pcb\n");
			listaAux->siguiente = listaExit->siguiente;
			free(listaExit);
			sem_post(&s_ColaExit);
			return;
		}
		listaAux = listaExit;
		listaExit = listaExit->siguiente;
	}
	printf("Salgo del Destruir Segmento\n");
}

t_new desencolarNew(void)
{
	sem_wait(&s_ColaNew);
	int peso, pesoMax;
	t_new* aux = l_new;
	t_new* auxAnt;
	t_new* maxAnt;
	t_new* maximo = aux;
	t_new max;
	t_medatada_program* metadataAux = metadata_desde_literal(maximo->codigo);
	pesoMax = (5* metadataAux->cantidad_de_etiquetas) + (3* metadataAux->cantidad_de_funciones) + (metadataAux->instrucciones_size);
	if (aux->siguiente == NULL)
	{
		l_new = NULL;
		max = *maximo;
		free(maximo);
		sem_post(&s_ColaNew);
		return max;
	}
	else
	{
		auxAnt = aux;
		aux = aux->siguiente;
		while(aux != NULL)
		{
			metadataAux = metadata_desde_literal(aux->codigo);
			peso = (5* metadataAux->cantidad_de_etiquetas) + (3* metadataAux->cantidad_de_funciones) + (metadataAux->instrucciones_size);
			if(peso > pesoMax)
			{
				pesoMax = peso;
				maximo = aux;
				maxAnt = auxAnt;
			}
			auxAnt = aux;
			aux = aux->siguiente;
		}
		if(maximo == l_new)
		{
			l_new = l_new->siguiente;
			max = (*maximo);
			free(maximo);
			sem_post(&s_ColaNew);
			return max;
		}
		maxAnt->siguiente = maximo->siguiente;
		max = *maximo;
		free(maximo);
		sem_post(&s_ColaNew);
		return max;
	}
}


void encolarEnReady(t_pcb* pcb)
{
	sem_wait(&s_ColaReady);
	t_pcb* aux = l_ready;
	if(l_ready == NULL)
	{
		l_ready = pcb;
		sem_post(&s_ColaReady);
		return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = pcb;
		pcb->siguiente = NULL;
		sem_post(&s_ColaReady);
		return;
	}
}

t_pcb* desencolarReady(void)
{
	sem_wait(&s_ColaReady);
	t_pcb* aux = l_ready;
	l_ready = aux->siguiente;
	sem_post(&s_ColaReady);
	return aux;
}

void encolarExec(t_pcb* pcb)
{
	t_pcb* aux = l_exec;
	if(l_exec == NULL)
	{
		l_exec = pcb;
		pcb->siguiente = NULL;
		//return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = pcb;
		pcb->siguiente = NULL;
		//return;
	}
	aux = l_exec;
	while(aux != NULL)
	{
		printf("PID en exec: %d", aux->pid);
		aux = aux->siguiente;
	}
	return;
}

void desencolarExec(t_pcb* pcb)
{
	t_pcb* aux = l_exec;
	t_pcb* auxAnt;
	if (aux->pid == pcb->pid)
	{
		l_exec = l_exec->siguiente;
	}
	auxAnt = l_exec;
	aux = aux->siguiente;
	while(aux != NULL)
	{
		if(aux->pid == pcb->pid)
		{
			auxAnt->siguiente = aux->siguiente;
			//free(aux);
			return;
		}
		auxAnt = aux;
		aux = aux->siguiente;
	}
}

void encolarExit(t_pcb* pcb)
{
	sem_wait(&s_ColaExit);
	t_pcb* aux = l_exit;

	if(l_exit == NULL)
	{
		l_exit = pcb;
		pcb->siguiente = NULL;
		printf("Encolando PRIMERO en exit %d\n", pcb->pid);
		sem_post(&s_ColaExit);
		return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = pcb;
		pcb->siguiente = NULL;
		printf("Encolando ULTIMO en exit %d\n", pcb->pid);
		sem_post(&s_ColaExit);
		return;
	}
}

t_pcb readyAExec(void)
{
	sem_wait(&s_ColaReady);
	t_pcb* auxReady = l_ready;
	l_ready = auxReady->siguiente;
	sem_post(&s_ColaReady);

	t_pcb* auxExec = l_exec;
	if(l_exec == NULL)
	{
		l_exec = auxReady;
		auxReady->siguiente = NULL;
		//return;
	}
	else
	{
		while(auxExec->siguiente != NULL)
		{
			auxExec = auxExec->siguiente;
		}
		auxExec->siguiente = auxReady;
		auxReady->siguiente = NULL;
		//return;
	}
//	aux = l_exec;
//	while(aux != NULL)
//	{
//		printf("PID en exec: %d", aux->pid);
//		aux = aux->siguiente;
//	}
	return *auxReady;
}

void execAReady(t_pcb pcb)
{
	t_pcb* auxExec = l_exec;
	t_pcb* auxAnt;
	//t_pcb* pcbAMover;
	while (auxExec != NULL)
	{
		printf("PID EN EXEC: %d\n", auxExec->pid);
		auxExec = auxExec->siguiente;
	}
	auxExec = l_exec;
	if(auxExec->siguiente == NULL && auxExec->pid == pcb.pid)
	{
		l_exec = NULL;
		//free(aux);
		//return;
	}
	else
	{
		auxAnt = auxExec;
		auxExec = auxExec->siguiente;
		while(auxExec != NULL)
		{
			if(auxExec->pid == pcb.pid)
			{
				auxAnt->siguiente = auxExec->siguiente;
				//free(aux);
			}
			else
			{
				auxAnt = auxExec;
				auxExec = auxExec->siguiente;
			}
		}
	}

	printf("Desencole\n");

	//pcbAMover = auxExec;
	auxExec->pid = pcb.pid;
	auxExec->segmentoCodigo = pcb.segmentoCodigo;
	auxExec->segmentoStack = pcb.segmentoStack;
	auxExec->cursorStack = pcb.cursorStack;
	auxExec->indiceCodigo = pcb.indiceCodigo;
	auxExec->indiceEtiquetas = pcb.indiceEtiquetas;
	auxExec->programCounter = pcb.programCounter;
	auxExec->tamanioContextoActual = pcb.tamanioContextoActual;
	auxExec->tamanioIndiceEtiquetas = pcb.tamanioIndiceEtiquetas;
	auxExec->tamanioIndiceCodigo = pcb.tamanioIndiceCodigo;
	auxExec->peso = pcb.peso;
	//auxExec->siguiente = NULL;

	printf("Asigne\n");

	sem_wait(&s_ColaReady);
	t_pcb* aux = l_ready;
	if(l_ready == NULL)
	{
		l_ready = auxExec;
		sem_post(&s_ColaReady);
		return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = auxExec;
		auxExec->siguiente = NULL;
		sem_post(&s_ColaReady);
		return;
	}
}

void execAExit(t_pcb pcb)
{
	t_pcb* auxExec = l_exec;
	t_pcb* auxAnt;
	//t_pcb* pcbAMover;
	if(auxExec->siguiente == NULL && auxExec->pid == pcb.pid)
	{
		l_exec = NULL;
		//free(aux);
		//return;
	}
	else
	{
		auxAnt = auxExec;
		auxExec = auxExec->siguiente;
		while(auxExec != NULL)
		{
			if(auxExec->pid == pcb.pid)
			{
				auxAnt->siguiente = auxExec->siguiente;
				//free(aux);
				//return;
			}
			auxAnt = auxExec;
			auxExec = auxExec->siguiente;
		}
	}

	//pcbAMover = auxExec;
	auxExec->pid = pcb.pid;
	auxExec->segmentoCodigo = pcb.segmentoCodigo;
	auxExec->segmentoStack = pcb.segmentoStack;
	auxExec->cursorStack = pcb.cursorStack;
	auxExec->indiceCodigo = pcb.indiceCodigo;
	auxExec->indiceEtiquetas = pcb.indiceEtiquetas;
	auxExec->programCounter = pcb.programCounter;
	auxExec->tamanioContextoActual = pcb.tamanioContextoActual;
	auxExec->tamanioIndiceEtiquetas = pcb.tamanioIndiceEtiquetas;
	auxExec->tamanioIndiceCodigo = pcb.tamanioIndiceCodigo;
	auxExec->peso = pcb.peso;
	auxExec->siguiente = NULL;

	sem_wait(&s_ColaExit);
	t_pcb* aux = l_exit;

	if(l_exit == NULL)
	{
		l_exit = auxExec;
		auxExec->siguiente = NULL;
		printf("Encolando PRIMERO en exit %d\n", auxExec->pid);
		sem_post(&s_ColaExit);
		return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = auxExec;
		auxExec->siguiente = NULL;
		printf("Encolando ULTIMO en exit %d\n", auxExec->pid);
		sem_post(&s_ColaExit);
		return;
	}
}

