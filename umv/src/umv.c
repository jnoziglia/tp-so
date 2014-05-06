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
void* solicitarBytes(void* inicio, int offset, int tamanio);
void enviarBytes(void* inicio, int offset, int tamanio, void* buffer);
int retornarBytesSolicitados(void* inicio, int offset, int tamanio);
void* mainConsola();
void* mainEsperarConexiones();
void* crearSegmento(int idProceso, int tamanio);
void* posicionarSegmento(int algoritmo, int tamanio);
void* first_fit(int tamanio);
void* worst_fit(int tamanio);
int nuevoIDSegmento(int idProceso);
void insertarSegmento(t_segmento* segmento);


/* Variables globales */
void* memPpal;
void* finMemPpal;
//const RAND_MAX = 64;
t_segmento* tablaSegmentos = NULL;
int algoritmo = 0;


int main (void)
{
	pthread_t consola, esperarConexiones;
	int rhConsola, rhEsperarConexiones;
	memPpal = malloc (1024);	//El tamaño va por configuracion
	finMemPpal = memPpal + 1024; //El tamaño va por configuracion

	rhConsola = pthread_create(&consola, NULL, mainConsola, NULL);
	rhEsperarConexiones = pthread_create(&esperarConexiones, NULL, mainEsperarConexiones, NULL);

	pthread_join(consola, NULL);
	pthread_join(esperarConexiones, NULL);

	printf("%d",rhConsola);
	printf("%d",rhEsperarConexiones);

	exit(0);
}


/* Hilo consola */
void* mainConsola()
{
	char* texto = malloc (20);
	printf("Bienvenido a la consola\n");
	while(1)
	{
		scanf("%s",texto);
		printf("Consola: %s\n",texto);
		if(string_equals_ignore_case(texto,"exit"))
		{
			free(texto);
			return NULL;
		}
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



int retornarBytesSolicitados(void* inicio,int offset, int tamanio) {
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
}


/* Funcion solicitar bytes */
void* solicitarBytes(void* inicio, int offset, int tamanio)
{
	void* pComienzo;
	pComienzo = inicio + offset;
	void* buffer= malloc(tamanio);
	memcpy(buffer,pComienzo,tamanio);
	return buffer;
}



void enviarBytes(void* inicio, int offset, int tamanio, void* buffer)
{
	void* pComienzo;
	pComienzo = inicio + offset;
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
	segmentoNuevo->base = random();
	segmentoNuevo->tamanio = tamanio;
	segmentoNuevo->dirInicio = inicioNuevo;
	insertarSegmento(segmentoNuevo);
	return inicioNuevo;
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
				//auxinicio = compactar();
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
	t_segmento* aux;
	aux = tablaSegmentos;
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
				tamMax = (int)(aux->dirInicio - auxinicio);
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
						//auxinicio = compactar();
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
		aux->siguiente = segmento;
	}
	return;
}
