/*
 ============================================================================
 Name        : cpu.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

/* #include <stdio.h>
#include <stdlib.h>

int main(void) {



	//permanente contacto con el Proceso UMV
	// se conecta con el proceso Kernel y queda a la espera de que el PCP le envie el PCB del programa
	// incrementar valor del registro PC del pcb
	// utilizara el indice codigo para soliciar a UMV proxima sentencia
	// parsea la sentencia
	//ejecuar operaciones requeridas
	// actualizar los segmentos del programa en la UMV
	// actualizar PC del pcb
	// notificar PCP que concluyo un quantum

	int* crearConexionConElKernel()
	{
		int* descriptor = socket(); //crear socket
		bind(); //indicar numero de servicio que queremos atender
		listen(); //avisar al sistema que atienda dicha conexion
		accept(); //pide y acepta conexiones de clientes al sistema operativo
		write(); //escribe datos del cliente
		read(); //recibe datos del cliente
		close(); //cierre de la comunicacion/socket


		return descriptor;

	}




} */


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
#include <parser/metadata_program.h>
#include <parser/parser.h>
#include <signal.h>

/* Definiciones y variables para la conexión por Sockets */
#define PUERTOUMV "6667"
#define IPUMV "127.0.0.1"
#define PUERTOKERNEL "6668"
#define IPKERNEL "127.0.0.1"
#define BACKLOG 3	// Define cuantas conexiones vamos a mantener pendientes al mismo tiempo
#define PACKAGESIZE 1024 	// Define cual va a ser el size maximo del paquete a enviar

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
	int tamanioIndiceCodigo;
	int peso;
	struct pcb *siguiente;
}t_pcb;

/* Funciones */
void conectarConKernel();
void conectarConUMV();
void* UMV_solicitarBytes(int pid, int base, int offset, int tamanio);
void dejarDeDarServicio();

/* Variables Globales */
int kernelSocket;
int umvSocket;
int quantum = 3; //todo:quantum que lee de archivo de configuración
int estadoCPU;
bool matarCPU = 0;

//todo:Primitivas, Hot Plug.

int main(){
	t_pcb* pcb = malloc(sizeof(pcb));
	t_intructions* indiceCodigo;
	t_intructions instruccionABuscar;
	int quantumUtilizado = 1;
	signal(SIGUSR1,dejarDeDarServicio);
	conectarConKernel();
	conectarConUMV();
	while(1)
	{
		recv(kernelSocket,pcb,sizeof(pcb),0);

		while(quantumUtilizado<=quantum)
		{
			indiceCodigo = UMV_solicitarBytes(pcb->pid,pcb->indiceCodigo,0,pcb->tamanioIndiceCodigo);
			instruccionABuscar = indiceCodigo[pcb->programCounter];
			char* instruccionAEjecutar = malloc(instruccionABuscar.offset);
			instruccionAEjecutar = UMV_solicitarBytes(pcb->pid,pcb->segmentoCodigo,instruccionABuscar.start,instruccionABuscar.offset);
			if(instruccionAEjecutar == NULL)
			{
				printf("Error en la lectura de memoria. Finalizando la ejecución del programa");
				//finalizar(); // Es una primitiva.
				return 0;
			}
			analizadorLinea(instruccionAEjecutar,NULL,NULL); //Todo: fijarse el \0 al final del STRING. Faltan 2 argumentos
			pcb->programCounter++;
			quantumUtilizado++;
		}
		if(matarCPU == 1)
		{
			int estadoCPU = 0;
			send(kernelSocket,(int*)estadoCPU,sizeof(int),0); //avisar que se muere
			send(umvSocket,(int*)estadoCPU,sizeof(int),0); //avisar que se muere
			close(kernelSocket);
			close(umvSocket);
			return 0;
		}
		estadoCPU = 1;
		send(kernelSocket,(int*)estadoCPU,sizeof(int),0); //avisar que se termina el quantum
	}


	return 0;
}

void conectarConKernel()
{
	struct addrinfo hintsKernel;
	struct addrinfo *kernelInfo;

	memset(&hintsKernel, 0, sizeof(hintsKernel));
	hintsKernel.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hintsKernel.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(IPKERNEL, PUERTOKERNEL, &hintsKernel, &kernelInfo);	// Carga en serverInfo los datos de la conexion
	kernelSocket = socket(kernelInfo->ai_family, kernelInfo->ai_socktype, kernelInfo->ai_protocol);

	connect(kernelSocket, kernelInfo->ai_addr, kernelInfo->ai_addrlen);
	freeaddrinfo(kernelInfo);	// No lo necesitamos mas

}

void conectarConUMV()
{
	struct addrinfo hintsUmv;
	struct addrinfo *umvInfo;

	memset(&hintsUmv, 0, sizeof(hintsUmv));
	hintsUmv.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hintsUmv.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(IPUMV, PUERTOUMV, &hintsUmv, &umvInfo);	// Carga en serverInfo los datos de la conexion
	umvSocket = socket(umvInfo->ai_family, umvInfo->ai_socktype, umvInfo->ai_protocol);

	connect(umvSocket, umvInfo->ai_addr, umvInfo->ai_addrlen);
	freeaddrinfo(umvInfo);	// No lo necesitamos mas
}

void dejarDeDarServicio()
{
	matarCPU = 1;
}

void* UMV_solicitarBytes(int pid, int base, int offset, int tamanio)
{
	return 0;
}


