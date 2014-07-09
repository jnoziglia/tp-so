/*
 ============================================================================
 Name        : umv.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

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
#include <semaphore.h>
#include <commons/log.h>


#define PUERTO "6668"
#define BACKLOG 5			// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo

/* Tabla de segmentos */
typedef struct segmento
{
	int idProceso;
	int idSegmento;
	int base;
	int tamanio;
	void* dirInicio;
	struct segmento *siguiente;
} t_segmento;

enum{
	kernel,
	cpu,
	programa
};

enum{
	operSolicitarBytes,
	operCrearSegmento,
	operDestruirSegmentos,
	operEnviarBytes
};

/* Funciones */
void* solicitarBytes(int base, int offset, int tamanio);
int enviarBytes(int base, int offset, int tamanio, void* buffer);
void* mainConsola();
void destruirSegmentos(int id);
void* mainEsperarConexiones();
int crearSegmento(int idProceso, int tamanio);
t_segmento* buscarSegmento(int base);
void* posicionarSegmento(int algoritmo, int tamanio);
void* first_fit(int tamanio);
void* worst_fit(int tamanio);
int nuevoIDSegmento(int idProceso);
void insertarSegmento(t_segmento* segmento);
void* compactar(void);
void dump(void);
void mostrarEstructuras();
void mostrarMemoria();
void mostrarContenidoDeMemoria();
void imprimirSegmento(t_segmento* segmento);
int cambioProcesoActivo(int idProceso);
int handshake(int id);
void* f_hiloKernel(void* socketCliente);
void* f_hiloCpu(void* socketCliente);

//TODO: Retardo(), handshake, conexiones, DUMP();


/* Variables globales */
void* memPpal;
void* finMemPpal;
t_segmento* tablaSegmentos = NULL;
int algoritmo = 1;	// 0 = FirstFit
int procesoActivo = 2;
int retardo = 0;
t_log* logi;

/* Semaforos */
sem_t s_cambioProcesoActivo;
sem_t s_TablaSegmentos;
sem_t s_cpu;


int main (void)
{
	pthread_t consola, esperarConexiones;
	int rhConsola, rhEsperarConexiones;
	int i;
	sem_init(&s_cambioProcesoActivo,0,1);
	sem_init(&s_TablaSegmentos,0,1);
	sem_init(&s_cpu,0,1);

	logi = log_create("/home/utnso/tp-2014-1c-unnamed/umv/src/log", "UMV", 0, LOG_LEVEL_INFO);

	t_config* configuracion = config_create("/home/utnso/tp-2014-1c-unnamed/umv/src/config.txt");
	int sizeMem = config_get_int_value(configuracion, "sizeMemoria");

	memPpal = malloc (sizeMem);	//El tamaño va por configuracion
	char* aux = (char*) memPpal;
	finMemPpal = memPpal + sizeMem; //El tamaño va por configuracion
	for(i=0; i<sizeMem; i++){aux[i] = 0;}

	rhConsola = pthread_create(&consola, NULL, mainConsola, NULL);
	rhEsperarConexiones = pthread_create(&esperarConexiones, NULL, mainEsperarConexiones, NULL);

	pthread_join(consola, NULL);
	pthread_join(esperarConexiones, NULL);

	printf("%d",rhConsola);
	printf("%d",rhEsperarConexiones);

	exit(0);
}


/* Prueba funciones */
/*{
	srand(time(NULL));
	printf("c");
		memPpal = malloc (65536);
		finMemPpal = memPpal+65536;
		char* buffer = malloc(70);
		buffer = "Holis";
		printf("%p \n\n", memPpal);

		crearSegmento(1,50);
		crearSegmento(2,50);
		crearSegmento(1,100);
		crearSegmento(2,30);
		crearSegmento(1,20);
		crearSegmento(2,20);

		t_segmento* aux = tablaSegmentos;
		printf("b");
		while (aux!=NULL)
		{
			printf("id: %d\n",aux->idProceso);
			printf("idSeg: %d\n",aux->idSegmento);
			printf("base: %d\n",aux->base);
			printf("tamaño: %d\n",aux->tamanio);
			printf("dir: %p\n\n",aux->dirInicio);
			aux = aux->siguiente;
		}
		destruirSegmentos(1);
		aux = tablaSegmentos;
		printf("-----------------------------\n");
		while (aux!=NULL)
		{
			printf("id: %d\n",aux->idProceso);
			printf("idSeg: %d\n",aux->idSegmento);
			printf("base: %d\n",aux->base);
			printf("tamaño: %d\n",aux->tamanio);
			printf("dir: %p\n\n",aux->dirInicio);
			aux = aux->siguiente;
		}
		printf("-----------------------------\n");
		crearSegmento(1,50);
		aux = tablaSegmentos;
		while (aux!=NULL)
		{
			printf("id: %d\n",aux->idProceso);
			printf("idSeg: %d\n",aux->idSegmento);
			printf("base: %d\n",aux->base);
			printf("tamaño: %d\n",aux->tamanio);
			printf("dir: %p\n\n",aux->dirInicio);
			aux = aux->siguiente;
		}

		enviarBytes(tablaSegmentos->base,0,1,buffer);
		printf("%s \n",(char*)solicitarBytes(tablaSegmentos->base,0,70));
		dump();
		return 0;
}*/

/* Hilo consola */
void* mainConsola()
{
	printf("Bienvenido a la consola\n\n");

	char* parametros = malloc(1000);
	while(1)
	{
		gets(parametros);
		//sleep(retardo);
		if(string_equals_ignore_case(parametros,"man"))
		{
			printf("Operaciones:\n");
			printf("\toperacion solicitar [pid] [base] [offset] [tamanio]\n");
			printf("\toperacion solicitar [pid] [base] [offset] [tamanio]\n");
			printf("\toperacion escribir [pid] [base] [offset] [tamanio]\n");
			printf("\toperacion crear-segmento [pid] [tamanio]\n");
			printf("\toperacion destruir-segmentos [pid]\n");
			printf("\tretardo [tiempo]\n");
			printf("\talgoritmo first-fit / worst-fit\n");
			printf("\tcompactacion\n");
			printf("\tdump\n");
			continue;
		}
		if(string_starts_with(parametros,"operacion "))
		{
			char* resto = string_substring_from(parametros,10);
			if(string_starts_with(resto,"solicitar "))
			{
				char* resto2 = string_substring_from(resto,10);
				char* pid;
				char* tamanio;
				char* base;
				char* offset;
				pid = strtok(resto2, " ");
				base = strtok(NULL, " ");
				offset = strtok(NULL, " ");
				tamanio = strtok(NULL, " ");
				if (pid != 0 || tamanio != 0)
				{
					char* buffer = malloc(atoi(tamanio));
					cambioProcesoActivo(atoi(pid));
					buffer = solicitarBytes(atoi(base), atoi(offset), atoi(tamanio));
					printf("%s",buffer);
					//cambioProcesoActivo(procesoAux);
					free(buffer);
				}
				else
				{
					printf("Argumentos incorrectos\n");
				}
				continue;
			}
			if(string_starts_with(resto,"escribir "))
			{
				char* resto2 = string_substring_from(resto,9);
				char* pid;
				char* tamanio;
				char* base;
				char* offset;
				pid = strtok(resto2, " ");
				base = strtok(NULL, " ");
				offset = strtok(NULL, " ");
				tamanio = strtok(NULL, " ");

				if (pid != 0 || tamanio != 0)
				{
					//TODO: tratar de cambiar el 128
					void* buffer = malloc (128);
					printf("Ingrese bytes a escribir: ");
					gets(buffer);
					printf("\n");
					cambioProcesoActivo(atoi(pid));
					enviarBytes(atoi(base), atoi(offset), atoi(tamanio), buffer);
					free(buffer);
					//cambioProcesoActivo(procesoAux);
				}
				else
				{
					printf("Argumentos incorrectos\n");
				}
				continue;
			}
			if(string_starts_with(resto,"crear-segmento "))
			{
				char* resto2 = string_substring_from(resto,15);
				char* pid;
				char* tamanio;
				pid = strtok(resto2, " ");
				tamanio = strtok(NULL, " ");
				if (tamanio != 0 || pid != 0)
				{
					crearSegmento(atoi(pid), atoi(tamanio));
					printf("Segmento creado\n");
				}
				else
				{
					printf("Argumentos incorrectos\n");

				}
				continue;

			}
			if(string_starts_with(resto,"destruir-segmentos "))
			{
				char* resto2 = string_substring_from(resto,19);
				if(atoi(resto2) != 0)
				{
					destruirSegmentos(atoi(resto2));
					printf("Segmentos destruidos\n");
				}
				else
				{
					printf("Argumentos incorrectos.\n");
				}
				continue;
			}
			printf("Argumento incorrecto\n");
			continue;
		}
		if(string_starts_with(parametros,"retardo "))
		{
			char* resto = string_substring_from(parametros,8);
			retardo = atoi(resto)/1000;
			printf("Retardo Cambiado\n");
			continue;
		}
		if(string_starts_with(parametros,"algoritmo "))
		{
			char* resto = string_substring_from(parametros,10);
			if(string_equals_ignore_case(resto,"worst-fit"))
			{
				algoritmo = 1;
				printf("Algoritmo cambiado a Worst-Fit\n");
				continue;
			}
			if(string_equals_ignore_case(resto,"first-fit"))
			{
				algoritmo = 0;
				printf("Algoritmo cambiado a First-Fit\n");
				continue;
			}
			printf("Argumento incorrecto");
			continue;
		}
		if(string_equals_ignore_case(parametros,"compactacion"))
		{
			compactar();
			printf("Compactado.\n");
			continue;
		}
		if(string_equals_ignore_case(parametros,"dump"))
		{
			dump();
			printf("Fin de dump\n");
			continue;
		}
		printf("Argumentos incorrectos.");
		continue;
	}

}

/* Hilo para escuchar conexiones */
//TODO: Desarrollar funcion
void* mainEsperarConexiones()
{
	pthread_t hiloKernel, hiloCpu;
	int rhHiloKernel, rhHiloCpu;
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	int escucharConexiones;
	char id = -1;
	char confirmacion;
	printf("Inicio del UMV.\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;		// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP
	getaddrinfo(NULL, PUERTO, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE
	escucharConexiones = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	bind(escucharConexiones,serverInfo->ai_addr, serverInfo->ai_addrlen);
	printf("Bienvenido a la escucha %d\n", escucharConexiones);
	while(1)
	{
		id = -1;
		confirmacion = -1;
		listen(escucharConexiones, BACKLOG);		// IMPORTANTE: listen() es una syscall BLOQUEANTE.
		struct sockaddr_in programa;			// Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
		socklen_t addrlen = sizeof(programa);
		int socketCliente = accept(escucharConexiones, (struct sockaddr *) &programa, &addrlen);
		printf("%d\n", socketCliente);
		recv(socketCliente, &id, sizeof(char), 0);
		confirmacion = handshake(id);
		send(socketCliente, &confirmacion, sizeof(char), 0);
		if (confirmacion == 1)
		{
			if (id == kernel)
			{
				printf("soy kernel\n");
				rhHiloKernel = pthread_create(&hiloKernel, NULL, f_hiloKernel, (void*)socketCliente);
				continue;
			}
			if (id == cpu)
			{
				printf("Soy CPU :D\n");
				rhHiloCpu = pthread_create(&hiloCpu, NULL, f_hiloCpu, (void*)socketCliente);
				continue;
			}
		}
		else
		{
			continue;
		}
	}
}

void* f_hiloKernel(void* socketCliente)
{
	int socketKernel = (int)socketCliente;
	int status = 1;
	int mensaje[5];
	int respuesta;
	char operacion;
	char confirmacion;
	int pid, base, offset, tamanio;
	char* buffer;
	int i,j = 0;
	while(status != 0)
	{
		recv(socketKernel, &operacion, sizeof(char), 0);
		sem_wait(&s_cpu);
		printf("OPERACION: %d\n", operacion);
		if (operacion == operCrearSegmento)
		{
			confirmacion = 1;
			send(socketKernel, &confirmacion, sizeof(char), 0);
			j=0;
			status = recv(socketKernel, mensaje, 2*sizeof(int), 0);
			if(status != 0)
			{
				respuesta = crearSegmento(mensaje[0], mensaje[1]);
				send(socketKernel, &respuesta, sizeof(int), 0);
			}
		}
		else if(operacion == operEnviarBytes)
		{
			confirmacion = 1;
			send(socketKernel, &confirmacion, sizeof(char), 0);
			status = recv(socketKernel, &pid, sizeof(int), 0);
			status = recv(socketKernel, &base, sizeof(int), 0);
			status = recv(socketKernel, &offset, sizeof(int), 0);
			status = recv(socketKernel, &tamanio, sizeof(int), 0);
			printf("El tamaño es: %d\n", tamanio);
			buffer = malloc(tamanio);
			status = recv(socketKernel, buffer, tamanio, 0);
			cambioProcesoActivo(pid);
			enviarBytes(base, offset, tamanio, buffer);
			free(buffer);
			send(socketKernel,&confirmacion,sizeof(char),0);
		}
		else if(operacion == operDestruirSegmentos)
		{
			confirmacion = 1;
			send(socketKernel, &confirmacion, sizeof(char), 0);
			status = recv(socketKernel, &pid, sizeof(int), 0);
			if(status != 0)
			{
				destruirSegmentos(pid);
			}
		}
		sem_post(&s_cpu);
	}
	//exit(0);
	return 0;
}

void* f_hiloCpu(void* socketCliente)
{
	int socketCPU = (int)socketCliente;
	int status = 1;
	int mensaje[4];
	char operacion;
	char confirmacion;
	char error;
	int pid, base, offset, tamanio;
	void* buffer;
	int i;
	while(status != 0)
	{
		printf("Recibo operacion\n");
		recv(socketCPU, &operacion, sizeof(char), 0);
		mostrarContenidoDeMemoria(0,finMemPpal-memPpal);
		printf("La operacion es: %d\n", operacion);
		if (operacion == operSolicitarBytes)
		{
			confirmacion = 1;
			send(socketCPU, &confirmacion, sizeof(char), 0);
			status = recv(socketCPU, mensaje, 4*sizeof(int), 0);
			buffer = malloc(mensaje[3]);
			cambioProcesoActivo(mensaje[0]);
			for(i=0; i<4; i++)
			{
				printf("MEnsaje %d: %d\n", i, mensaje[i]);
			}
			buffer = solicitarBytes(mensaje[1], mensaje[2], mensaje[3]);
//			int aux;
//			memcpy(&aux, buffer, sizeof(int));
//			printf("AUX: %d\n", aux);
			if (buffer == NULL) printf("buffer nulo\n");
			send(socketCPU, buffer, mensaje[3], 0);
			free(buffer);
		}
		else if(operacion == operEnviarBytes)
		{
			confirmacion = 1;
			send(socketCPU, &confirmacion, sizeof(char), 0);
			status = recv(socketCPU, &pid, sizeof(int), 0);
			status = recv(socketCPU, &base, sizeof(int), 0);
			status = recv(socketCPU, &offset, sizeof(int), 0);
			status = recv(socketCPU, &tamanio, sizeof(int), 0);
			buffer = malloc(tamanio);
			status = recv(socketCPU, buffer, tamanio, 0);
			cambioProcesoActivo(pid);
			error = enviarBytes(base, offset, tamanio, buffer);
			printf("CONFIRMACION: %d\n", error);
			send(socketCPU, &error, sizeof(char), 0);
			free(buffer);
		}
		else if(operacion == operDestruirSegmentos)
		{
			confirmacion = 1;
			send(socketCPU, &confirmacion, sizeof(char), 0);
			status = recv(socketCPU, &pid, sizeof(int), 0);
			if(status != 0)
			{
				destruirSegmentos(pid);
			}
		}
		else
		{
			printf("ERROR");
		}
		sem_post(&s_cpu);
	}
	//exit(0);
	return 0;
}

int handshake(int id)
{
	if (id == kernel || id == cpu)
	{
		return 1;
	}
	return 0;
}

void destruirSegmentos( int id){
	mostrarEstructuras();
	sem_wait(&s_TablaSegmentos);
	t_segmento* aux = NULL;
	t_segmento* auxSiguiente = tablaSegmentos;
	while (auxSiguiente != NULL)
	{
		if(auxSiguiente->idProceso == id)
		{
			if (auxSiguiente == tablaSegmentos)
			{
				tablaSegmentos = auxSiguiente->siguiente;
				free(auxSiguiente);
				auxSiguiente = tablaSegmentos;
			}
			else
			{
				if (auxSiguiente->siguiente == NULL)
				{
					aux->siguiente = NULL;
					free(auxSiguiente);
				}
				else
				{
					aux->siguiente = auxSiguiente->siguiente;
					free(auxSiguiente);
				}
				auxSiguiente = aux->siguiente;
			}
		}
		else
		{
			aux = auxSiguiente;
			auxSiguiente = auxSiguiente->siguiente;
		}
	}
	sem_post(&s_TablaSegmentos);
	mostrarEstructuras();
	return;
}

/* Funcion solicitar bytes */
void* solicitarBytes(int base, int offset, int tamanio)
{
	sem_wait(&s_TablaSegmentos);
	void* pComienzo;
	t_segmento* segmentoBuscado = buscarSegmento(base);
	if ((segmentoBuscado == NULL) || (offset + tamanio > segmentoBuscado->tamanio))
	{
		printf("Segmentation Fault");
		sem_post(&s_cambioProcesoActivo);
		sem_post(&s_TablaSegmentos);
		return NULL;
	}
	pComienzo = segmentoBuscado->dirInicio + offset;
	void* buffer= malloc(tamanio);
	memcpy(buffer,pComienzo,tamanio);
	sem_post(&s_cambioProcesoActivo);
	sem_post(&s_TablaSegmentos);
	return buffer;
}

int enviarBytes(int base, int offset, int tamanio, void* buffer)
{
	sem_wait(&s_TablaSegmentos);
	void* pComienzo;
	t_segmento* segmentoBuscado = buscarSegmento(base);
	printf("CHEQUEO SI HAY SEG FAULT\n");
	if ((segmentoBuscado == NULL) || (offset + tamanio > segmentoBuscado->tamanio))
	{
		printf("Segmentation Fault");
		sleep(10);
		sem_post(&s_cambioProcesoActivo);
		sem_post(&s_TablaSegmentos);
		return -1;
	}
	pComienzo = segmentoBuscado->dirInicio + offset;
	memcpy(pComienzo,buffer,tamanio);
	sem_post(&s_cambioProcesoActivo);
	sem_post(&s_TablaSegmentos);
	return 0;
}

int crearSegmento(int idProceso, int tamanio)
{
	sem_wait(&s_TablaSegmentos);
	void* inicioNuevo = posicionarSegmento(algoritmo,tamanio);
	t_segmento* segmentoNuevo;
	if(inicioNuevo == NULL)
	{
		printf("No pudo posicionarse el segmento nuevo\n");
		sem_post(&s_TablaSegmentos);
		return -1;
	}
	segmentoNuevo = malloc(sizeof(t_segmento));
	segmentoNuevo->idProceso = idProceso;
	segmentoNuevo->idSegmento = nuevoIDSegmento(idProceso);
	segmentoNuevo->base = rand()%10001;
	segmentoNuevo->tamanio = tamanio;
	segmentoNuevo->dirInicio = inicioNuevo;
	insertarSegmento(segmentoNuevo);
	sem_post(&s_TablaSegmentos);
	return segmentoNuevo->base;
}

t_segmento* buscarSegmento(int base)
{
	t_segmento* aux = tablaSegmentos;
	while(aux != NULL)
	{
		if(aux->base == base && aux->idProceso == procesoActivo)
		{
			return aux;
		}
		aux = aux->siguiente;
	}
	return NULL;
}

void* posicionarSegmento(int algoritmo, int tamanio)
{
	void* inicioNuevo;
	if(algoritmo == 0)
	{
		inicioNuevo = first_fit(tamanio);
		return inicioNuevo;
	}
	else
	{
		inicioNuevo = worst_fit(tamanio);
		return inicioNuevo;
	}
}

/* Algoritmos First fit y Worst fit */
void* first_fit(int tamanio)
{
	int compactado = 0;
	t_segmento* aux;
	aux = tablaSegmentos;
	void* auxinicio = memPpal;
	while(aux != NULL)
	{
		if(tamanio <= (int)(aux->dirInicio - auxinicio))
		{
			return auxinicio;
		}
		else
		{
			auxinicio = aux->dirInicio + aux->tamanio;
			aux = aux->siguiente;
		}
	}
	while(compactado <= 1)
	{
		if(tamanio <= (int)(finMemPpal - auxinicio))
		{
			return auxinicio;
		}
		else
		{
			if(compactado == 0)
			{
				auxinicio = compactar();
				compactado = 1;
			}
			else
			{
				return 0;
			}
		}
	}
	return 0;
}

void* worst_fit(int tamanio)
{
	int compactado = 0;
	t_segmento* aux = tablaSegmentos;
	void* auxinicio = memPpal;
	int tamMax = 0;
	void* dirTamMax = NULL;
	if(tablaSegmentos == NULL)
	{
		if(tamanio < (int)(finMemPpal - memPpal))
		{
			return memPpal;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		tamMax = (int) (aux->dirInicio - auxinicio);
		dirTamMax = auxinicio;
		while(aux != NULL)
		{
			if(tamMax < (int)(aux->dirInicio - auxinicio))
			{
				tamMax = (int)(aux->dirInicio - auxinicio);
				dirTamMax = auxinicio;
			}
			else
			{
				auxinicio = aux->dirInicio + aux->tamanio;
				aux = aux->siguiente;
			}
		}
		if(tamMax < (int)(finMemPpal - auxinicio))
		{
			tamMax = (int)(finMemPpal - auxinicio);
			dirTamMax = auxinicio;
		}
		while(compactado <= 1)
		{
			if(tamMax >= tamanio)
			{
				return dirTamMax;
			}
			else
			{
				if(compactado == 0)
				{
					auxinicio = compactar();
					compactado = 1;
				}
				else
				{
					return 0;
				}
			}
		}
	}
	return 0;
}

int nuevoIDSegmento(int idProceso)
{
	t_segmento* aux = tablaSegmentos;
	int idSegmento = 0;
	while(aux != NULL)
	{
		if((aux->idProceso == idProceso) && (aux->idSegmento >= idSegmento))
		{
			idSegmento = aux->idSegmento + 1;
		}
		aux = aux->siguiente;
	}
	return idSegmento;
}

void insertarSegmento(t_segmento* segmento)
{
	t_segmento* auxAnterior = tablaSegmentos;
	t_segmento* aux;
	if(tablaSegmentos == NULL)
	{
		tablaSegmentos = segmento;
		tablaSegmentos->siguiente = NULL;
		return;
	}
	else
	{
		if (tablaSegmentos->dirInicio > segmento->dirInicio)
		{
			segmento->siguiente = tablaSegmentos;
			tablaSegmentos = segmento;
			return;
		}
		else
		{
			auxAnterior = tablaSegmentos;
			aux = auxAnterior->siguiente;
			while (aux != NULL)
			{
				if (aux->dirInicio > segmento->dirInicio)
				{
					segmento->siguiente = aux;
					auxAnterior->siguiente = segmento;
					return;
				}
				else
				{
					auxAnterior = aux;
					aux = aux->siguiente;
				}
			}
			auxAnterior->siguiente = segmento;
			segmento->siguiente = NULL;
			return;
		}
	}
}

void* compactar(void)
{
	t_segmento* auxSegmento = tablaSegmentos;
	void* aux = memPpal;

	while(auxSegmento != NULL)
	{
		if (aux != auxSegmento->dirInicio)
		{
			memcpy(aux,auxSegmento->dirInicio,auxSegmento->tamanio);
			auxSegmento->dirInicio = aux;
		}
		aux = auxSegmento->dirInicio+auxSegmento->tamanio;
		auxSegmento = auxSegmento->siguiente;
	}
	return aux;
}

void dump(void)
{
	sem_wait(&s_TablaSegmentos);
	mostrarEstructuras();
	//mostrarMemoria();
	mostrarContenidoDeMemoria(0,finMemPpal-memPpal);
	sem_post(&s_TablaSegmentos);
}

void mostrarEstructuras(void)
{
	t_segmento* auxSegmento = tablaSegmentos;
	log_info(logi, "Tabla de Segmentos:");
	//printf("\nTabla de Segmentos:\n\n");
		while (auxSegmento != NULL)
		{
			imprimirSegmento(auxSegmento);
			auxSegmento = auxSegmento->siguiente;
		}
	log_info(logi, "----------------------------------------------");
	//printf("--------------------\n");
}

void mostrarMemoria(void)
{
	t_segmento* auxSegmento = tablaSegmentos;
	int tamanioSegmentos = 0;
	void* contador = memPpal;
	printf("Memoria principal:\n\n");
	while (contador < finMemPpal)
	{
		if(auxSegmento != NULL && contador == auxSegmento->dirInicio)
		{
			printf("Desde la posicion %p hasta la %p pertenecen al programa %d, segmento %d\n",auxSegmento->dirInicio
																						,auxSegmento->dirInicio+auxSegmento->tamanio
																						,auxSegmento->idProceso
																						,auxSegmento->idSegmento);
			contador = contador + auxSegmento->tamanio;
			auxSegmento = auxSegmento->siguiente;
		}
		else
		{
			printf("%p -- Libre\n",contador);
			contador++;
		}
	}
	auxSegmento = tablaSegmentos;
	while(auxSegmento != NULL)
	{
		tamanioSegmentos += auxSegmento->tamanio;
		auxSegmento = auxSegmento->siguiente;
	}
	printf("\nMemoria libre: %d bytes de %d bytes\n\n",(finMemPpal - memPpal) - tamanioSegmentos, finMemPpal - memPpal);
	printf("-------------------------------------\n");
}

void mostrarContenidoDeMemoria(int offset, int tamanio)
{
	char* contador = memPpal + offset;
	int i;
	log_info(logi, "Contenido de la memoria desde %p hasta %p:", contador,contador+tamanio-1);
	//printf("Contenido de la memoria desde %p hasta %p:\n\n", contador,contador+tamanio-1);
	for (i=0; i<tamanio; i++)
	{
		log_info(logi, "%p --- %d\t",contador+i,*(contador+i));
		log_info(logi, "%c",*(contador+i));
//		printf("%p --- %d\t",contador+i,*(contador+i));
//		printf("%c\n",*(contador+i));
	}
}

void imprimirSegmento(t_segmento* segmento)
{
	log_info(logi, "Proceso: %d \t Segmento: %d \n Base: %d" ,segmento->idProceso, segmento->idSegmento, segmento->base);
	//log_info(logi, "Segmento: %d" ,segmento->idSegmento);
	//log_info(logi, "Base: %d" ,segmento->base);
	//log_info(logi, "Tamaño: %d" ,segmento->tamanio);
	//log_info(logi, "Direccion fisica: %p" ,segmento->dirInicio);
//	printf("\nProceso: %d \n",segmento->idProceso);
//	printf("Segmento: %d \n",segmento->idSegmento);
//	printf("Base: %d \n",segmento->base);
//	printf("Tamaño: %d \n",segmento->tamanio);
//	printf("Direccion fisica: %p \n",segmento->dirInicio);
//	printf("-----------------------------------\n");
}

int cambioProcesoActivo(int idProceso)
{
	sem_wait(&s_cambioProcesoActivo);
	int aux = procesoActivo;
	procesoActivo = idProceso;
	return aux;
}
