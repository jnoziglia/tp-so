/*
 ============================================================================
 Name        : plp.c
 Author      : tu macho
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

/*{
    }*/

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




//llega un PCB, cargo estos datos

typedef struct datosPrograma{
	int finish;
	int arribo;
	int waitAverage;
	int lastSchedulerActiveTime;
	int stopCounter;
	int processTime;
	int pid;
}t_datosPrograma;

typedef struct nodoReady{
	t_datosPrograma datosProgramas;
	int *siguiente;

}t_nodoReady;

t_nodoReady *readyList;
t_nodoReady *inicioLista;
t_nodoReady *finLista;

void llegaUnPrograma(pcb pcbProceso){
	//encolarlo a ready
	t_nodoReady proceso;
	proceso -> datosProgramas -> finish = 0;
	//proceso -> datosProgramas -> arribo = ;
	proceso -> datosProgramas -> waitAverage = 0;
	proceso -> datosProgramas -> lastSchedulerActiveTime = 0;
	proceso -> datosProgramas -> stopCounter = 0;
	proceso -> datosProgramas -> processTime ;
	proceso -> datosProgramas -> pid = pcbProceso -> pid;
	agregarAListaReady(proceso);
}

*t_nodoReady agregarAListaReady(t_nodoReady){
	}

//Global Variables
struct customList;//the element of the list where we will placed


//contextchange tiempo que tarda en cambiar de contexto


void roundRobin(struct customList *readyList, int quantum, int contextchangue)
    {
    int actualTime = 0; //tiempo que va pasando, tiempo actual
    int cantProcesosEnReady;
    cantProcesosEnReady = getElementsCount(readyList);
    //printf("We have %d process\n",elements);

    while(cantProcesosEnReady>=0)//Hay procesos en la lista
        {
        //actualTime+=contextchangue; si consideramos el tiempo que tarda en cambiar de contexto lo ponemos, si no no.


    	if (readyList->finished==0 && readyList->arrivetime<=actualTime)//process can be accepted by the round robin policy
    		//si el proceso no termino y el tiempo de arribo es menor o igual al tiempo actual
    		{
            //verificar waitaverange
            if (readyList->waitaverange == 0)
                {
                readyList->waitaverange = actualTime-readyList->arrivetime;
                }
            else//Gets CPU
                {
                readyList->waitaverange=(readyList->waitaverange+(actualTime-readyList->lastScheculerActiveTime))/2;
                }
            readyList->stopCounter++;
            readyList->processtime=(readyList->processtime)-quantum;

            //printf("processtime == %d\n",readyList->processtime);
            if (readyList->processtime<=0)//The process has ended
                {
                actualTime-=readyList->processtime;
                //readyList->processtime=0;
                readyList->finished=1;
                cantProcesosEnReady--;
                printf("Process %d ended\n",readyList->pid);
                }
            else//The process need more cpu time
                {
                actualTime+=quantum;
                readyList->lastScheculerActiveTime = actualTime;
                }
            }
            //Move next element
            readyList=getCustomListPointer((readyList->list).next);
        }


 }

