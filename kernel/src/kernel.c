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

/* Estructuras de datos */
typedef struct pcb
{
	int pid;
	int segmentoCodigo;
	int segmentoStack;
	int cursorStack;
	int indiceCodigo;
	int indiceEtiquetas;
	int programCounter;
	int tamanioContextoActual;
	int tamanioIndiceEtiquetas;
}pcb;

/* Funciones */
void* hiloPCP();
void* hiloPLP();

/* Variables Globales */
void* l_new = NULL;
void* l_ready = NULL;
void* l_exec = NULL;
void* l_exit = NULL;


int main(void) {
	pthread_t hiloPCP, hiloPLP;
	int rhPCP, rhPLP;
	rhPCP = pthread_create(&hiloPCP, NULL, (void*)hiloPCP, NULL);
	rhPLP = pthread_create(&hiloPLP, NULL, (void*)hiloPLP, NULL);
	pthread_join(hiloPCP, NULL);
	pthread_join(hiloPLP, NULL);
	printf("%d",rhPCP);
	printf("%d",rhPLP);
	return 0;
	//return EXIT_SUCCESS;
}


void* hiloPCP()
{
	return 0;
}

void* hiloPLP()
{
return 0;
}
