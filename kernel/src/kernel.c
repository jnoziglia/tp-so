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
#define PUERTOCPU "6680"
#define IPCPU "127.0.0.1"

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


/* Funciones */
void* f_hiloPCP();
void* f_hiloPLP();
t_pcb* crearPcb(char* codigo);
int generarPid(void);
void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer);
char* serializarEnvioBytes(int pid, int base, int offset, int tamanio, void* buffer);
void Programa_imprimirTexto(char* texto);
void encolarEnNew(t_pcb* pcb);
void conexionCPU(void);
void conexionUMV(void);
int UMV_crearSegmentos(int mensaje[2]);
void UMV_destruirSegmentos(int pid);
void* f_hiloMostrarNew();
void serializarPcb(t_pcb* pcb, void* package);



/* Variables Globales */
t_pcb* l_new = NULL;
t_pcb* l_ready = NULL;
t_pcb* l_exec = NULL;
t_pcb* l_exit = NULL;
t_medatada_program* metadata;
int tamanioStack = 10;
int ultimoPid = 0;
int socketUMV;
//int socketCPU; //VER
//fd_set readfds;


/* Semáforos */
int s_Multiprogramacion; //Semáforo del grado de Multiprogramación. Deja pasar a Ready los PCB Disponibles.

int main(void) {
	pthread_t hiloPCP, hiloPLP, hiloMostrarNew;
	int rhPCP, rhPLP, rhMostrarNew;
	conexionUMV();
	rhPCP = pthread_create(&hiloPCP, NULL, f_hiloPCP, NULL);
	rhPLP = pthread_create(&hiloPLP, NULL, f_hiloPLP, NULL);
	rhMostrarNew = pthread_create(&hiloMostrarNew, NULL, f_hiloMostrarNew, NULL);
	pthread_join(hiloPCP, NULL);
	pthread_join(hiloPLP, NULL);
	pthread_join(hiloMostrarNew, NULL);
	//printf("%d",rhPCP);
	//printf("%d",rhPLP);

	return 0;
}


void* f_hiloPCP()
{
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	int socketPCP;
	int socketCliente[10];
	fd_set writefds;
	fd_set readfds;
	int i, maximo = -1;
	int status = 1;
	int j = 1;
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
	bind(socketPCP,serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(socketPCP, BACKLOG);

	int socketpiola = accept(socketPCP, (struct sockaddr *) &conexioncpu, &addrlen);

	while(l_new == NULL)
	{

	}

	t_pcb* pcbAux;
	pcbAux = l_new;
	int superMensaje[11];
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
	//serializarPcb(pcbAux, package)

	status = send(socketpiola, superMensaje, sizeof(t_pcb), 0);
	//l_new = NULL;


//	while(1)
//	{
//		FD_ZERO(&writefds);
//		FD_ZERO(&readfds);
//		FD_SET (socketPCP, &readfds);
//		for (i=0; i<=maximo; i++)
//		{
//			FD_SET (socketCliente[i], &writefds);
//			//printf("socketkernCPU %d\n",socketCliente[i]);
//		}
//
//		select(socketPCP + j, &readfds, &writefds, NULL, NULL);
//
//		for(i=0; i<=maximo; i++)
//		{
//			if(FD_ISSET(socketCliente[i], &writefds))
//			{
//				//printf("entre al if.\n");
//				if (status != 0)
//				{
//					//printf("entre al if2.\n");
//					if(l_new != NULL)
//					{
//						//printf("entre al if3.\n");
//						t_pcb* pcbAux;
//						pcbAux = l_new;
//						serializarPcb(pcbAux, package);
//						status = send(socketCliente[i], package, sizeof(t_pcb), 0);
//						l_new = NULL;
//						//printf("PCB Enviado.\n");
//					}
//
//				}
//				else
//				{
//					maximo--;
//				}
//			}
//		}
//
//		if(FD_ISSET(socketPCP, &readfds))
//		{
//			//printf("socket CPU %d\n", socketPCP);
//			maximo++;
//			j++;
//			socketCliente[maximo] = accept(socketPCP, (struct sockaddr *) &conexioncpu, &addrlen);
//		}
//	}
//	return NULL;
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
	printf("SocketServidor: %d\n",socketServidor);

	int socketllegado = accept(socketServidor, (struct sockaddr *) &programa, &addrlen);
	printf("SocketLlegado: %d\n",socketllegado);

	status = recv(socketllegado,(void*)package, PACKAGESIZE, 0);
	printf("status: %d\n",status);

	t_pcb* nuevoPCB;
	nuevoPCB = crearPcb(package);
	if(nuevoPCB != NULL)
	{
		printf("Nuevo PCB Creado\n");
		encolarEnNew(nuevoPCB);
	}


	/*while(1)
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
				printf("Codigo Recibido. %d\n", status);
				if (status != 0)
				{
					t_pcb* nuevoPCB;
					//printf("%s", package);
					nuevoPCB = crearPcb(package);
					if(nuevoPCB != NULL)
					{
						printf("Nuevo PCB Creado\n");
						encolarEnNew(nuevoPCB);

					}

				}
				else
				{
					maximo--;
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
	return NULL;*/
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
	int mensaje[2];
	int respuesta, i;
	pcbAux->pid = generarPid();
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
	mensaje[1] = strlen(codigo);
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
	pcbAux->peso = (5* metadataAux->cantidad_de_etiquetas) + (3* metadataAux->cantidad_de_funciones);
	printf("Se crea el pcb\n");
	printf("codigo: %d\n", strlen(codigo));
	UMV_enviarBytes(pcbAux->pid, pcbAux->segmentoCodigo,0,strlen(codigo),codigo);
	printf("Creado segmento Codigo con el tamaño %d\n",strlen(codigo));
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

int generarPid(void)
{
	ultimoPid++;
	return ultimoPid;
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

void encolarEnNew(t_pcb* pcb)
{
	t_pcb* auxAnterior = l_new;
	t_pcb* aux;
	if(l_new == NULL)
	{
		l_new = pcb;
		l_new->siguiente = NULL;
		return;
	}
	else
	{
		if (l_new->peso > pcb->peso)
		{
			pcb->siguiente = l_new;
			l_new = pcb;
			return;
		}
		else
		{
			auxAnterior = l_new;
			aux = auxAnterior->siguiente;
			while (aux != NULL)
			{
				if (aux->peso > pcb->peso)
				{
					pcb->siguiente = aux;
					auxAnterior->siguiente = pcb;
					return;
				}
				else
				{
					auxAnterior = aux;
					aux = aux->siguiente;
				}
			}
			auxAnterior->siguiente = pcb;
			pcb->siguiente = NULL;
			return;
		}
	}
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
			t_pcb* aux = l_new;
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
