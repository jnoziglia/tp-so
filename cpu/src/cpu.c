/*
 ============================================================================
 Name        : cpu.c
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
#include <parser/metadata_program.h>
#include <parser/parser.h>
#include <signal.h>

/* Definiciones y variables para la conexión por Sockets */
#define PUERTOUMV "6668"
#define IPUMV "127.0.0.1"
#define PUERTOKERNEL "6669"
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

typedef struct misFunciones
{
	t_puntero (*AnSISOP_definirVariable)(t_nombre_variable identificador_variable);
	t_puntero (*AnSISOP_obtenerPosicionVariable)(t_nombre_variable identificador_variable);
	t_valor_variable (*AnSISOP_dereferenciar)(t_puntero direccion_variable);
	void (*AnSISOP_asignar)(t_puntero direccion_variable, t_valor_variable valor);
	t_valor_variable (*AnSISOP_obtenerValorCompartida)(t_nombre_compartida variable);
	t_valor_variable (*AnSISOP_asignarValorCompartida)(t_nombre_compartida variable, t_valor_variable valor);
	void (*AnSISOP_irAlLabel)(t_nombre_etiqueta t_nombre_etiqueta);
	void (*AnSISOP_llamarSinRetorno)(t_nombre_etiqueta etiqueta);
	void (*AnSISOP_llamarConRetorno)(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
	void (*AnSISOP_finalizar)(void);
	void (*AnSISOP_retornar)(t_valor_variable retorno);
	void (*primitiva_signal)(char* nombre_semaforo);
	void (*wait) (char* nombre_semaforo);
}t_misFunciones;

typedef struct funcionesKernel
{
	void (*AnSISOP_wait)(t_nombre_semaforo identificador_semaforo);
	void (*AnSISOP_signal)(t_nombre_semaforo identificador_semaforo);
}t_funcionesKernel;

/* Funciones */
void conectarConKernel();
void conectarConUMV();
void* UMV_solicitarBytes(int pid, int base, int offset, int tamanio);
void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer);
char* serializarEnvioBytes(int pid, int base, int offset, int tamanio, void* buffer);
void dejarDeDarServicio();
/* Primitivas*/
t_puntero AnSISOP_definirVariable(t_nombre_variable identificador_variable);
t_puntero AnSISOP_obtenerPosicionVariable(t_nombre_variable identificador_variable);
t_valor_variable AnSISOP_dereferenciar(t_puntero direccion_variable);
void AnSISOP_asignar(t_puntero direccion_variable, t_valor_variable valor);
//todo:ObtenerValorCompartida, AsignarValorCompartida,
t_valor_variable AnSISOP_obtenerValorCompartida(t_nombre_compartida variable);
t_valor_variable AnSISOP_asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor);
void AnSISOP_irAlLabel(t_nombre_etiqueta t_nombre_etiqueta);
void AnSISOP_llamarSinRetorno(t_nombre_etiqueta etiqueta);
void AnSISOP_llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
void AnSISOP_finalizar(void);
void AnSISOP_retornar(t_valor_variable retorno);
void AnSISOP_signal(t_nombre_semaforo identificador_semaforo);
void AnSISOP_wait(t_nombre_semaforo identificador_semaforo);

/* Variables Globales */
int kernelSocket;
int socketUMV;
int quantum = 10; //todo:quantum que lee de archivo de configuración
int estadoCPU;
bool matarCPU = 0;
t_pcb* pcb;

//todo:Primitivas, Hot Plug.

int main(){
	t_misFunciones* funciones;
	t_funcionesKernel* kernel;

	pcb = malloc(sizeof(pcb));
	t_intructions* indiceCodigo;
	t_intructions instruccionABuscar;
	int quantumUtilizado = 1;
	signal(SIGUSR1,dejarDeDarServicio);
	conectarConKernel();
	conectarConUMV();
	printf("Conexiones establecidas.\n");
	while(1)
	{
		recv(kernelSocket,pcb,sizeof(pcb),0);
		printf("PCB recibido.\n");
		printf("pid: %d\n", pcb->pid);

		while(quantumUtilizado<=quantum)
		{

			indiceCodigo = UMV_solicitarBytes(pcb->pid,pcb->indiceCodigo,0,pcb->tamanioIndiceCodigo);
			instruccionABuscar = indiceCodigo[pcb->programCounter];
			char* instruccionAEjecutar = malloc(instruccionABuscar.offset);
			instruccionAEjecutar = UMV_solicitarBytes(pcb->pid,pcb->segmentoCodigo,instruccionABuscar.start,instruccionABuscar.offset);
			if(instruccionAEjecutar == NULL)
			{
				printf("Error en la lectura de memoria. Finalizando la ejecución del programa");
				AnSISOP_finalizar(); // Es una primitiva.
				return 0;
			}
			printf("%s\n", instruccionAEjecutar);
			analizadorLinea(instruccionAEjecutar,funciones,kernel); //Todo: fijarse el \0 al final del STRING. Faltan 2 argumentos
			pcb->programCounter++;
			quantumUtilizado++;
		}
		if(matarCPU == 1)
		{
			int estadoCPU = 0;
			send(kernelSocket,(int*)estadoCPU,sizeof(int),0); //avisar que se muere
			send(socketUMV,(int*)estadoCPU,sizeof(int),0); //avisar que se muere
			close(kernelSocket);
			close(socketUMV);
			free(pcb);
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
	char id = 1;
	char conf = 0;

	memset(&hintsUmv, 0, sizeof(hintsUmv));
	hintsUmv.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hintsUmv.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(IPUMV, PUERTOUMV, &hintsUmv, &umvInfo);	// Carga en serverInfo los datos de la conexion
	socketUMV = socket(umvInfo->ai_family, umvInfo->ai_socktype, umvInfo->ai_protocol);

	connect(socketUMV, umvInfo->ai_addr, umvInfo->ai_addrlen);
	freeaddrinfo(umvInfo);	// No lo necesitamos mas

	send(socketUMV, &id, sizeof(char), 0);
	recv(socketUMV, &conf, sizeof(char), 0);
}

void dejarDeDarServicio()
{
	matarCPU = 1;
}

void* UMV_solicitarBytes(int pid, int base, int offset, int tamanio)
{
	char operacion = 0;
	char confirmacion;
	void* buffer = malloc(tamanio);
	int mensaje[4];
	mensaje[0] = pid;
	mensaje[1] = base;
	mensaje[2] = offset;
	mensaje[3] = tamanio;
	send(socketUMV, &operacion, sizeof(char), 0);
	recv(socketUMV, &confirmacion, sizeof(char), 0);
	if(confirmacion == 0)
	{
		send(socketUMV, mensaje, 4*sizeof(int), 0);
		recv(socketUMV, buffer, tamanio, 0);
	}
	return buffer;
}

void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer)
{
	//int status = 1;
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


/*
 * PRIMITVAS
 */

t_puntero AnSISOP_definirVariable(t_nombre_variable identificador_variable)
{
	void* buffer = malloc(5);
	memcpy(buffer,&identificador_variable,1);
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack + pcb->tamanioContextoActual * 5),5,buffer);
	pcb->tamanioContextoActual++;
	//todo:Diccionario de variables. ??? ???
	free(buffer);
	return (pcb->cursorStack + pcb->tamanioContextoActual * 5)+1;
}

t_puntero AnSISOP_obtenerPosicionVariable(t_nombre_variable identificador_variable)
{
	int offset = -1;
	int aux = 0;
	char* buffer = malloc(pcb->tamanioContextoActual * 5);
	buffer = UMV_solicitarBytes(pcb->pid,pcb->segmentoStack,pcb->cursorStack,(pcb->tamanioContextoActual * 5));
	while(aux <= (pcb->tamanioContextoActual * 5))
	{
		if(*(buffer + aux) == identificador_variable)
		{
			free(buffer);
			offset = pcb->cursorStack + aux + 1;
			return (offset);
		}
		aux = aux + 5;
	}
	free(buffer);
	return offset;
}

t_valor_variable AnSISOP_dereferenciar(t_puntero direccion_variable)
{
	printf("dereferenciar\n");
	int* valor;
	valor = UMV_solicitarBytes(pcb->pid,pcb->segmentoStack,direccion_variable,4);
	return *valor;
}

void AnSISOP_asignar(t_puntero direccion_variable, t_valor_variable valor)
{
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,direccion_variable,4,&valor);
}

//TODO:ObtenerValorCompartida, asignarVariableCompartida
t_valor_variable AnSISOP_obtenerValorCompartida(t_nombre_compartida variable)
{
	return 0;
}

t_valor_variable AnSISOP_asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor)
{
	return 0;
}

void AnSISOP_irAlLabel(t_nombre_etiqueta t_nombre_etiqueta)
{
	printf("irallabel\n");
	void* etiquetas = malloc(pcb->tamanioIndiceEtiquetas);
	etiquetas = UMV_solicitarBytes(pcb->pid,pcb->indiceEtiquetas,0,pcb->tamanioIndiceEtiquetas);
	metadata_buscar_etiqueta(t_nombre_etiqueta,etiquetas,pcb->tamanioIndiceEtiquetas);
	free(etiquetas);
	return;
}

void AnSISOP_llamarSinRetorno(t_nombre_etiqueta etiqueta)
{
	printf("llamarsinretorno\n");
	void* buffer = malloc(8);
	memcpy(buffer,&(pcb->cursorStack),4);
	memcpy((buffer+4),&(pcb->programCounter),4);
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack + (pcb->tamanioContextoActual*5)),8,buffer);
	pcb->cursorStack = pcb->cursorStack + 8;
	free(buffer);
	return;
}


void AnSISOP_llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar)
{
	printf("llamarconretorno\n");
	void* buffer = malloc(12);
	memcpy(buffer,&(pcb->cursorStack),4);
	memcpy((buffer+4),&donde_retornar,4);
	memcpy((buffer+8),&(pcb->programCounter),4);
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack + (pcb->tamanioContextoActual*5)),12,buffer);
	pcb->cursorStack = pcb->cursorStack + 12;
	free(buffer);
	return;
}

void AnSISOP_finalizar(void)
{
	printf("finalizar\n");
	if(pcb->cursorStack == 0)
	{
		return;
	}
	else
	{
		int contexto_anterior, instruccion_a_ejecutar;
		void* buffer = malloc(8);
		buffer = UMV_solicitarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack - 8),8);
		memcpy(&instruccion_a_ejecutar,(buffer+4),4);
		memcpy(&contexto_anterior,buffer,4);
		pcb->cursorStack = contexto_anterior;
		free(buffer);
		return;
	}
}

void AnSISOP_retornar(t_valor_variable retorno)
{
	int contexto_anterior, instruccion_a_ejecutar, direccion_a_retornar;
	void* buffer = malloc(12);
	buffer = UMV_solicitarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack - 12),12);
	memcpy(&instruccion_a_ejecutar,(buffer+8),4);
	memcpy(&direccion_a_retornar,(buffer+4),4);
	memcpy(&contexto_anterior,buffer,4);
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,direccion_a_retornar,4,&retorno);
	pcb->cursorStack = contexto_anterior;
	free(buffer);
	return;
}

void AnSISOP_wait(t_nombre_semaforo identificador_semaforo)
{
	return;
}
void AnSISOP_signal(t_nombre_semaforo identificador_semaforo)
{
	return;
}
