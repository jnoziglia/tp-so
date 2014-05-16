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

/* Funciones */
void* solicitarBytes(int base, int offset, int tamanio);
void enviarBytes(int base, int offset, int tamanio, void* buffer);
//int retornarBytesSolicitados(void* inicio, int offset, int tamanio);
void* mainConsola();
void destruirSegmentos(int id);
void* mainEsperarConexiones();
void* crearSegmento(int idProceso, int tamanio);
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
//TODO: revisar insertarSegmento(); Retardo(), handshake, consola,conexiones, DUMP(), revisar Destruir segmentos;


/* Variables globales */
void* memPpal;
void* finMemPpal;
t_segmento* tablaSegmentos = NULL;
int algoritmo = 1;
int procesoActivo = 2;
int retardo = 0;


int main (void)
{
	pthread_t consola, esperarConexiones;
	int rhConsola, rhEsperarConexiones;
	memPpal = malloc (10);	//El tamaño va por configuracion
	finMemPpal = memPpal + 10; //El tamaño va por configuracion

	rhConsola = pthread_create(&consola, NULL, mainConsola, NULL);
	//rhEsperarConexiones = pthread_create(&esperarConexiones, NULL, mainEsperarConexiones, NULL);

	pthread_join(consola, NULL);
	//pthread_join(esperarConexiones, NULL);

	printf("%d",rhConsola);
	//printf("%d",rhEsperarConexiones);

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
	printf("Bienvenido a la consola\n");
	char* parametros = malloc(1000);
	while(1)
	{
		gets(parametros);
		//sleep(retardo);
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
					int procesoAux = cambioProcesoActivo(atoi(pid));
					buffer = solicitarBytes(atoi(base), atoi(offset), atoi(tamanio));
					printf("%s",buffer);
					cambioProcesoActivo(procesoAux);
					free(buffer);
				}
				else
				{
					printf("Argumentos incorrectos");
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
					gets(buffer);
					int procesoAux = cambioProcesoActivo(atoi(pid));
					enviarBytes(atoi(base), atoi(offset), atoi(tamanio), buffer);
					cambioProcesoActivo(procesoAux);
				}
				else
				{
					printf("Argumentos incorrectos");
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
					printf("Argumentos incorrectos");

				}
				continue;

			}
			if(string_starts_with(resto,"destruir-segmentos "))
			{
				char* resto2 = string_substring_from(resto,19);
				if(atoi(resto2) != 0)
				{
					destruirSegmentos(atoi(resto2));
					printf("Segmentos destruidos");
				}
				else
				{
					printf("Argumentos incorrectos.");
				}
				continue;
			}
			printf("Argumento incorrecto");
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
	char* texto = malloc (20);
	printf("Bienvenido a la escucha\n");
	while(1)
	{
		scanf("%s",texto);
		printf("Conexiones: %s\n",texto);
		if(string_equals_ignore_case(texto,"exit"))
		{
			free(texto);
			return NULL;
		}
	}
}

void destruirSegmentos( int id){
	t_segmento* aux = NULL;
	t_segmento* auxSiguiente = tablaSegmentos;

	while (auxSiguiente->idProceso == id)
	{
		tablaSegmentos = auxSiguiente->siguiente;
		free(auxSiguiente);
		auxSiguiente = tablaSegmentos;
	}
	aux = tablaSegmentos;
	auxSiguiente = aux->siguiente;
	while(auxSiguiente->siguiente != NULL)
	{
		if(auxSiguiente->idProceso == id)
		{
			aux->siguiente = auxSiguiente->siguiente;
			free(auxSiguiente);
			auxSiguiente = aux->siguiente;
		}
		else
		{
			aux=aux->siguiente;
			auxSiguiente= aux->siguiente;
		}
	}
	if(auxSiguiente->idProceso == id)
	{
		aux->siguiente = NULL;
		free (auxSiguiente);
	}
}




/*int retornarBytesSolicitados(void* inicio,int offset, int tamanio) {
	void* bufferLleno;
	int* aux;
	int i;
	bufferLleno = solicitarBytes(inicio,3,tamanio);
	aux = bufferLleno;
	printf("\n buffer:\n");
	for (i=0;i<tamanio;i++)
	{
		printf("%p - ", aux+i);
		printf("%d\n", *(aux+i));
	}

	printf("\n Valor de aux: %d", *aux);
	return 0;
}*/


//TODO: Arreglar funciones
/* Funcion solicitar bytes */
void* solicitarBytes(int base, int offset, int tamanio)
{
	void* pComienzo;
	t_segmento* segmentoBuscado = buscarSegmento(base);
	if ((segmentoBuscado == NULL) || (offset + tamanio > segmentoBuscado->tamanio))
	{
		printf("Segmentation Fault");
		return NULL;
	}
	pComienzo = segmentoBuscado->dirInicio + offset;
	void* buffer= malloc(tamanio);
	memcpy(buffer,pComienzo,tamanio);
	return buffer;
}



void enviarBytes(int base, int offset, int tamanio, void* buffer)
{
	void* pComienzo;
	t_segmento* segmentoBuscado = buscarSegmento(base);
	if ((segmentoBuscado == NULL) || (offset + tamanio > segmentoBuscado->tamanio))
	{
		printf("Segmentation Fault");
		return;
	}
	pComienzo = segmentoBuscado->dirInicio + offset;
	memcpy(pComienzo,buffer,tamanio);
}


void* crearSegmento(int idProceso, int tamanio)
{
	void* inicioNuevo = posicionarSegmento(algoritmo,tamanio);
	t_segmento* segmentoNuevo;
	if(inicioNuevo == NULL)
	{
		printf("No pudo posicionarse el segmento nuevo\n");
		return NULL;
	}
	segmentoNuevo = malloc(sizeof(t_segmento));
	segmentoNuevo->idProceso = idProceso;
	segmentoNuevo->idSegmento = nuevoIDSegmento(idProceso);
	segmentoNuevo->base = rand()%10001;
	segmentoNuevo->tamanio = tamanio;
	segmentoNuevo->dirInicio = inicioNuevo;
	insertarSegmento(segmentoNuevo);
	return inicioNuevo;
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
	t_segmento* aux = tablaSegmentos;
	t_segmento* auxSiguiente;
	if(tablaSegmentos == NULL)
	{
		tablaSegmentos = segmento;
		tablaSegmentos->siguiente = NULL;
	}
	else
	{
		while(aux->siguiente != NULL)
		{
			if(segmento->dirInicio > aux->dirInicio)
			{
				aux = aux->siguiente;
			}
			else
			{
				auxSiguiente = aux->siguiente;
				aux->siguiente = segmento;
				segmento->siguiente = auxSiguiente;
				return;
			}
		}
		if(segmento->dirInicio > aux->dirInicio)
		{
			aux->siguiente = segmento;
		}
		else
		{
			auxSiguiente = aux->siguiente;
			aux->siguiente = segmento;
			segmento->siguiente = auxSiguiente;
		}

		segmento->siguiente = NULL;
	}
	return;
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
	mostrarEstructuras();
	//mostrarMemoria();
	//mostrarContenidoDeMemoria();
}

void mostrarEstructuras(void)
{
	t_segmento* auxSegmento = tablaSegmentos;
	printf(" ");
		while (auxSegmento != NULL)
		{
			imprimirSegmento(auxSegmento);
			auxSegmento = auxSegmento->siguiente;
		}
}

void imprimirSegmento(t_segmento* segmento)
{
	printf("Proceso: %d \n",segmento->idProceso);
	printf("Segmento: %d \n",segmento->idSegmento);
	printf("Base: %d \n",segmento->base);
	printf("Tamaño: %d \n",segmento->tamanio);
	printf("Direccion fisica: %p \n\n",segmento->dirInicio);
}

int cambioProcesoActivo(int idProceso)
{
	int aux = procesoActivo;
	procesoActivo = idProceso;
	return aux;
}
