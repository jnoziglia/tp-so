/*
 ============================================================================
 Name        : pcp.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>    // boolean datatype


//Estructuras de datos

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


typedef struct datosPrograma{
	int finish;
	int usoDelCPU;
	pcb *pcbPrograma;
}t_datosPrograma;

typedef struct nodoReady{
	t_datosPrograma datosProgramas;
	int *siguiente;

}t_nodoReady;

typedef struct nodoExit{
	pcb *pcbPrograma;
	int *siguiente;
}t_nodoExit;

//variables globales

t_nodoReady *inicioReady;
t_nodoReady *finReady;
t_nodoExit *inicioExit;
t_nodoExit *finExit;
int cantidadCPU;
int cpuDisponibles = 10 ;

//Definicion de funciones
void llegaUnPrograma(pcb pcbProceso);
t_nodoReady crearNodoReady(pcb pcbProceso);
void encolarAReady(t_nodoReady proceso);
void encolarAExit(t_nodoReady proceso);
void planificacionRoundRobin(t_nodoReady inicioReady);



//funciones

void llegaUnPrograma(pcb pcbProceso){
	t_nodoReady proceso = crearNodoReady(pcbProceso);
	encolarAReady(proceso);
}

t_nodoReady crearNodoReady(pcb pcbProceso){
	t_nodoReady proceso;
	proceso -> datosProgramas -> finish = 0;
	proceso -> datosProgramas -> usoDelCPU = sizeof(pcbProceso -> indiceCodigo);
	proceso -> datosProgramas -> pcbPrograma = pcbProceso;
	proceso -> siguiente = NULL;
	return proceso;
}


void encolarAReady(t_nodoReady proceso){

	//si hay algo en la lista
	if(inicioReady != NULL){
		finReady -> siguiente = proceso;
		finReady = proceso;

	}

	//si la lista esta vacia
	if(inicioReady == NULL){
		inicioReady = proceso;
		finReady = proceso;
	}

}

void encolarAExit(t_nodoReady proceso){
	t_nodoExit procesoAFinalizar;

	//verificar que haya terminado de ejecutar
	if(proceso -> datosProgramas -> finish == 1){
		procesoAFinalizar->	pcbPrograma = proceso -> datosProgramas -> pcbPrograma;

		//si hay algo en la lista
		if(inicioExit != NULL){
			finExit -> siguiente = procesoAFinalizar;
			procesoAFinalizar-> siguiente = finExit;
		}

		//si la lista esta vacia
		if(inicioExit == NULL){
				inicioExit = proceso;
				finExit = proceso;
		}
	}

}


void planificacionRoundRobin(t_nodoReady inicioReady){

    int cantProcesosEnReady = getElementsCount(inicioReady);

    while(cantProcesosEnReady>=0)//Hay procesos en la lista
    {
    	t_nodoReady procesoActual = inicioReady;
    	int usoCPU = procesoActual-> datosProgramas -> usoDelCPU;


    	if(cpuDisponibles >=0){
    		cpuDisponibles -- ;
    		//le asigno cpu, le mando pbc y usoDelCPU
    		//sacar su quantum
    		//libera al proceso
    		int quantum;
    		cpuDisponibles ++ ;
    		usoCPU -= quantum;

    	}

    	//si termino de ejecutarse
    	if(usoCPU <= 0){
    		inicioReady->datosProgramas->finish = 1;
    		encolarEnExit(procesoActual);

    	}
    	//si no termino de ejecutarse
    	else{
    		encolarAReady(procesoActual);
    	}

    	int *aux = inicioReady -> siguiente;
    	inicioReady = aux;
    }

}
