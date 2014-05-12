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
}t_pcb;

/* Funciones */
void* hiloPCP();
void* hiloPLP();
t_pcb* crearPcb(char* codigo);
int generarPid(void);
int UMV_crearSegmento(int tamanio);
void UMV_enviarBytes(int base, int offset, int tamanio, void* buffer);
void Programa_imprimirTexto(char* texto);


/* Variables Globales */
void* l_new = NULL;
void* l_ready = NULL;
void* l_exec = NULL;
void* l_exit = NULL;
int gradoMultiprogramacion;
t_medatada_program* metadata;
int tamanioStack;

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

t_pcb* crearPcb(char* codigo)
{
	t_pcb* pcbAux = malloc (sizeof(t_pcb));
	t_medatada_program* metadataAux = metadata_desde_literal(codigo);
	pcbAux->pid = generarPid();
	pcbAux->programCounter = metadataAux->instruccion_inicio;
	pcbAux->tamanioIndiceEtiquetas = metadataAux->cantidad_de_etiquetas;
	pcbAux->segmentoStack = UMV_crearSegmento(tamanioStack);
	pcbAux->cursorStack = pcbAux->segmentoStack;
	pcbAux->segmentoCodigo = UMV_crearSegmento(sizeof(*codigo));
	pcbAux->tamanioContextoActual = 0;
	pcbAux->indiceCodigo = UMV_crearSegmento(metadataAux->instrucciones_size)*(sizeof(t_intructions));
	pcbAux->indiceEtiquetas = UMV_crearSegmento(metadataAux->etiquetas_size);
	if(pcbAux->segmentoStack == 0 || pcbAux->segmentoCodigo == 0 || pcbAux->indiceCodigo == 0 || pcbAux->indiceEtiquetas == 0)
	{
		//avisar al programa :D
		Programa_imprimirTexto("No se pudo crear el programa");
	}
	UMV_enviarBytes(pcbAux->segmentoCodigo,0,sizeof(*codigo),codigo);
	UMV_enviarBytes(pcbAux->indiceEtiquetas,0,metadataAux->etiquetas_size,metadataAux->etiquetas);
	UMV_enviarBytes(pcbAux->indiceCodigo,0,(metadataAux->instrucciones_size)*(sizeof(t_intructions)),metadataAux->instrucciones_serializado);
	return pcbAux;
}

int generarPid(void)
{
	return 0;
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
