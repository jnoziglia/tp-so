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
	int prioridad;
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
	proceso -> prioridad = 3; //un proceso nuevo tiene la menor prioridad
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
	//hay que tener en cuenta si llegan dos o mas procesos al mismo tiempo.
	//comparar prioridades


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

    /*//recibo conexion de un CPU disponible
    int socketCPU;
    int confirmacion;
    if(recv(socketCPU, &confirmacion, sizeof(int), 0)){
    cpuDisponibles ++;
    }
    */

    while(cantProcesosEnReady>=0)//Hay procesos en la lista
    {
    	t_nodoReady procesoActual = inicioReady;
    	int datosPrograma = procesoActual-> datosProgramas;
    	int usoCPU = procesoActual-> datosProgramas -> usoDelCPU;
    	pcb pcbPrograma = procesoActual-> datosProgramas -> pcbPrograma;

    	if(cpuDisponibles >=0){
    		//lo asigno a un CPU
    		send(socketCPU,&datosPrograma,sizeof(pcbPrograma),0);
    		cpuDisponibles -- ;
    		//recibe del cpu, el pcb del programa, su quantum y si se ejecuto correctamente
    		if(1/*no tira error el cpu*/){
    		//si hay un error y se suspende el proceso en ejecucion
    			int quantum;
    			cpuDisponibles ++ ;
    			usoCPU -= quantum;
    		}
    		//si hay un error y se suspende el proceso en ejecucion
    		//libera al proceso
    		else{
    			//avisar que se interrumpio
    		}


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
