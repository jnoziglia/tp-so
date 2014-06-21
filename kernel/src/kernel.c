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
#include <wait.h>

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
t_pcb* crearPcb(t_new programa);
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
t_pcb* recibirSuperMensaje ( int superMensaje[11] );
void cargarConfig(void);
void destruirPCB(int pid);
void* f_hiloColaReady();
t_new desencolarNew(void);
void encolarEnReady(t_pcb* pcb);
t_pcb* desencolarReady(void);
void encolarExec(t_pcb* pcb);
void desencolarExec(t_pcb* pcb);
void encolarExit(t_pcb* pcb);


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


/* Semáforos */
int s_Multiprogramacion = 10; //Semáforo del grado de Multiprogramación. Deja pasar a Ready los PCB Disponibles.

int main(void) {
	pthread_t hiloPCP, hiloPLP, hiloMostrarNew, hiloColaReady;
	int rhPCP, rhPLP, rhMostrarNew, rhColaReady;

	cargarConfig();

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
	while(1)
	{
		if(l_new != NULL)
		{
			//wait(&s_Multiprogramacion);
			programa = desencolarNew();
			t_pcb* nuevoPCB;
			nuevoPCB = crearPcb(programa);
			if(nuevoPCB != NULL)
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
	int i, maximo = 0;
	int superMensaje[11];
	int status = 1;
	char mensaje;
	void* package = malloc(sizeof(t_pcb));
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
					if (socketAux > maximo) maximo = socketAux;
				}
				else
				{
					FD_CLR(i, &fdRPCP);
					FD_SET(i, &fdWPCP);
					//Cuando no es un CPU, recibe el PCB o se muere el programa;
					recv(i,&mensaje,sizeof(char),0);
					recv(i,&superMensaje,sizeof(superMensaje),0);
					//t_pcb* pcb = malloc(sizeof(t_pcb));
					t_pcb* pcb = recibirSuperMensaje(superMensaje);
					desencolarExec(pcb);
					if(mensaje == 0) //todo:podria ser ENUM
					{
						//Se muere el programa

						printf("Llegó un programa para encolar en Exit\n");
						//encolarEnExit
						encolarExit(pcb);
					}
					else
					{
						//Se termina el quantum o va a block
						printf("Llegó un programa para encolar en Ready\n");
						encolarEnReady(pcb);
						int z;
						t_pcb* aux = l_ready;
						while(aux != NULL)
						{
							printf("PID en Ready: %d\n",aux->pid);
							printf("SegmentoCodigo en Ready: %d\n",aux->segmentoCodigo);
							printf("SegmentoStack en Ready: %d\n",aux->segmentoStack);
							aux = aux->siguiente;
						}
					}
				}
			}
			else if(FD_ISSET(i, &writePCP))
			{
				//FD_SET(i, &writePCP);
				//Sacar de Ready
				if(l_ready != NULL)	//TODO: PONER SEMAFORO!!!
				{
					FD_CLR(i, &fdWPCP);
					FD_SET(i, &fdRPCP);
					t_pcb* pcbAux;
					pcbAux = desencolarReady();
					superMensaje[0] = pcbAux->pid;
					superMensaje[1] = pcbAux->segmentoCodigo;
					superMensaje[2] = pcbAux->segmentoStack;
					superMensaje[3] = pcbAux->cursorStack;
					superMensaje[4] = pcbAux->indiceCodigo;
					superMensaje[5] = pcbAux->indiceEtiquetas;
					superMensaje[6] = pcbAux->programCounter;
					superMensaje[7] = pcbAux->tamanioContextoActual;
					superMensaje[8] = pcbAux->tamanioIndiceEtiquetas;
					superMensaje[9] = pcbAux->tamanioIndiceCodigo;
					superMensaje[10] = pcbAux->peso;
					//free(l_new);
					//l_new = NULL;
					status = send(i, superMensaje, sizeof(t_pcb), 0);
					encolarExec(pcbAux);
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
	t_pcb* listaExit = l_exit;
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
		listaExit = l_exit;
		maximoAnterior = 2;
		readPLP = fdRPLP;
		writePLP = fdWPLP;
		select(maximo + 1, &readPLP, &writePLP, NULL, NULL);
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
									maximo = j;
								}
							}
						}
					}
				}
			}
			else if (FD_ISSET(i, &writePLP))
			{
				while(listaExit != NULL)
				{
					if(i == listaExit->pid)
					{
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
									maximo = j;
								}
							}
						}
						FD_CLR(i, &fdWPLP);
						destruirPCB(i);
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



t_pcb* crearPcb(t_new programa)
{
	t_pcb* pcbAux = malloc (sizeof(t_pcb));
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
	respuesta = UMV_crearSegmentos(mensaje);
	if(respuesta == -1)
	{
		//avisar al programa :D
		Programa_imprimirTexto("Holis, No se pudo crear el programa");
		UMV_destruirSegmentos(pcbAux->pid);
		printf("no se creo el pcb\n");
		free (pcbAux);
		return NULL;
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
		return NULL;
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
		return NULL;
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
			return NULL;
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
	for (i=0; i<=pcbAux->tamanioIndiceCodigo;i++){
		printf("instruccion: %d\n", metadataAux->instrucciones_serializado[i].start);
		printf("tamanio: %d\n\n", metadataAux->instrucciones_serializado[i].offset);
	}
	return pcbAux;
}

int UMV_crearSegmentos(int mensaje[2])
{
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
		return respuesta;
	}
	return -1;
}

void UMV_destruirSegmentos(int pid)
{
	char operacion = 2;
	char confirmacion;
	send (socketUMV, &operacion, sizeof(char), 0);
	recv(socketUMV, &confirmacion, sizeof(char), 0);
	if (confirmacion ==  1)
	{
		send(socketUMV, &pid, sizeof(int), 0);
		return;
	}
	return;
}



void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer)
{
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
		return;
	}
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
	t_new* aux = l_new;
	if(aux == NULL)
	{
		l_new = programa;
		l_new->siguiente = NULL;
		return;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			aux = aux->siguiente;
		}
		aux->siguiente = programa;
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

/*void conexionCPU(void)
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
}*/

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
			t_pcb* aux = l_ready;
			while(aux != NULL)
			{
				printf("%d\t\t",aux->pid);
				printf("%d\n",aux->peso);
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

t_pcb* recibirSuperMensaje ( int superMensaje[11] )
{
	int i;
	t_pcb* pcb = l_exec;
	while(pcb->pid != superMensaje[0] && pcb != NULL)
	{
		pcb= pcb->siguiente;
	}
	if(pcb == NULL) printf("El PCB no existe\n");
	pcb->pid = superMensaje[0];
	pcb->segmentoCodigo = superMensaje[1];
	pcb->segmentoStack=	superMensaje[2] ;
	pcb->cursorStack=superMensaje[3]  ;
	pcb->indiceCodigo=superMensaje[4] ;
	pcb->indiceEtiquetas=superMensaje[5]  ;
	pcb->programCounter=superMensaje[6] ;
	pcb->tamanioContextoActual=superMensaje[7] ;
	pcb->tamanioIndiceEtiquetas=superMensaje[8] ;
	pcb->tamanioIndiceCodigo=superMensaje[9] ;
	pcb->peso=superMensaje[10] ;

	for(i=0; i<11; i++){
		printf("pcb: %d\n", superMensaje[i]);
	}
	return pcb;
}

void cargarConfig(void)
{
	t_config* configuracion = config_create("/home/utnso/tp-2014-1c-unnamed/kernel/src/config.txt");
	PUERTOPROGRAMA = config_get_string_value(configuracion, "PUERTOPROGRAMA");
	BACKLOG = config_get_int_value(configuracion, "BACKLOG");			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
	PACKAGESIZE = config_get_int_value(configuracion, "PACKAGESIZE");	// Define cual va a ser el size maximo del paquete a enviar
	PUERTOUMV = config_get_string_value(configuracion, "PUERTOUMV");
	IPUMV = config_get_string_value(configuracion, "IPUMV");
	PUERTOCPU = config_get_string_value(configuracion, "PUERTOCPU");
	IPCPU = config_get_string_value(configuracion, "IPCPU");
	tamanioStack = config_get_int_value(configuracion, "TAMANIOSTACK");
}

void destruirPCB(int pid)
{
	t_pcb* listaExit = l_exit;
	t_pcb* listaAux = NULL;
	if(listaExit->siguiente == NULL)
	{
		printf("destruyo pcb\n");
		free(listaExit);
		l_exit = NULL;
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
			return;
		}
		listaAux = listaExit;
		listaExit = listaExit->siguiente;
	}
}

t_new desencolarNew(void)
{
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
		maxAnt->siguiente = maximo->siguiente;
		max = *maximo;
		free(maximo);
		return max;
	}
}


void encolarEnReady(t_pcb* pcb)
{
	t_pcb* aux = l_ready;
	if(l_ready == NULL)
	{
		l_ready = pcb;
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
		return;
	}
}

t_pcb* desencolarReady(void)
{
	t_pcb* aux = l_ready;
	l_ready = aux->siguiente;
	return aux;
}

void encolarExec(t_pcb* pcb)
{
	t_pcb* aux = l_exec;
	if(l_exec == NULL)
	{
		l_exec = pcb;
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
		return;
	}
}

void desencolarExec(t_pcb* pcb)
{
	t_pcb* aux = l_exec;
	t_pcb* auxAnt;
	if(aux->siguiente == NULL)
	{
		l_exec = NULL;
		//free(aux);
		return;
	}
	auxAnt = aux;
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
	t_pcb* aux = l_exit;
	if(l_exit == NULL)
	{
		l_exit = pcb;
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
		return;
	}
}
