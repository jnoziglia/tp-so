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
#define PUERTOKERNEL "6680"
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
void UMV_enviarBytes(int pid, int base, int offset, int tamanio, void* buffer);
char* serializarEnvioBytes(int pid, int base, int offset, int tamanio, void* buffer);
void dejarDeDarServicio();
t_pcb* deserializarPcb(void* package);

/* Primitivas*/
t_puntero AnSISOP_definirVariable(t_nombre_variable identificador_variable);
t_puntero AnSISOP_obtenerPosicionVariable(t_nombre_variable identificador_variable);
t_valor_variable AnSISOP_dereferenciar(t_puntero direccion_variable);
void AnSISOP_asignar(t_puntero direccion_variable, t_valor_variable valor);
//todo:ObtenerValorCompartida, AsignarValorCompartida,
t_valor_variable AnSISOP_obtenerValorCompartida(t_nombre_compartida variable);
t_valor_variable AnSISOP_asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor);
void AnSISOP_irAlLabel(t_nombre_etiqueta nombre_etiqueta);
void AnSISOP_llamarSinRetorno(t_nombre_etiqueta etiqueta);
void AnSISOP_llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
void AnSISOP_finalizar(void);
void AnSISOP_retornar(t_valor_variable retorno);
void AnSISOP_imprimir(t_valor_variable valor_mostrar);
void AnSISOP_imprimirTexto(char* texto);
void AnSISOP_entradaSalida(t_nombre_dispositivo dispositivo, int tiempo);
void AnSISOP_wait(t_nombre_semaforo identificador_semaforo);
void AnSISOP_signal(t_nombre_semaforo identificador_semaforo);

AnSISOP_funciones funciones = {
		.AnSISOP_asignar				= AnSISOP_asignar,
		.AnSISOP_asignarValorCompartida = AnSISOP_asignarValorCompartida,
		.AnSISOP_definirVariable		= AnSISOP_definirVariable,
		.AnSISOP_dereferenciar			= AnSISOP_dereferenciar,
		.AnSISOP_entradaSalida 			= AnSISOP_entradaSalida,
		.AnSISOP_finalizar				= AnSISOP_finalizar,
		.AnSISOP_imprimir				= AnSISOP_imprimir,
		.AnSISOP_imprimirTexto			= AnSISOP_imprimirTexto,
		.AnSISOP_irAlLabel 				= AnSISOP_irAlLabel,
		.AnSISOP_llamarConRetorno 		= AnSISOP_llamarConRetorno,
		.AnSISOP_llamarSinRetorno 		= AnSISOP_llamarSinRetorno,
		.AnSISOP_obtenerPosicionVariable= AnSISOP_obtenerPosicionVariable,
		.AnSISOP_obtenerValorCompartida = AnSISOP_obtenerValorCompartida,
		.AnSISOP_retornar 				= AnSISOP_retornar,

};
AnSISOP_kernel kernel_functions = {
		.AnSISOP_signal 				= AnSISOP_signal,
		.AnSISOP_wait					= AnSISOP_wait,
};

/* Variables Globales */
int kernelSocket;
int socketUMV;
int quantum = 10; //todo:quantum que lee de archivo de configuración
int estadoCPU;
bool matarCPU = 0;
bool terminarPrograma = 0;
t_pcb* pcb;

//todo:Primitivas, Hot Plug.

int main(){

	void* package = malloc(sizeof(t_pcb));
	void* indiceCodigo;
	t_intructions instruccionABuscar;
	int quantumUtilizado = 1;
	signal(SIGUSR1,dejarDeDarServicio);
	conectarConUMV();
	conectarConKernel();
	printf("socketK: %d\n", kernelSocket);
	printf("socketU: %d\n", socketUMV);
	printf("Conexiones establecidas.\n");
	pcb = malloc (sizeof(t_pcb));

	int i,a;
	while(1)
	{
		printf("Espero pcb\n");
		int superMensaje[11];

		int recibido = recv(kernelSocket,superMensaje,sizeof(t_pcb),0);
		pcb->pid = superMensaje[0];
		pcb->segmentoCodigo = superMensaje[1];
		pcb->segmentoStack=	superMensaje[2] ;
		pcb->cursorStack=superMensaje[3]  ;
		pcb->indiceCodigo=superMensaje[4] ;
		pcb->indiceEtiquetas=superMensaje[5]  ;
		pcb->programCounter=superMensaje[6] ;
		pcb->tamanioContextoActual=superMensaje[7] ;
		pcb->tamanioIndiceEtiquetas=superMensaje[8] ;
		pcb->tamanioIndiceCodigo=superMensaje[9] ;
		pcb->peso=superMensaje[10] ;

		for(i=0; i<11; i++){
			printf("pcb: %d\n", superMensaje[i]);
		}


		printf("%d",recibido);
		printf("PCB recibido.\n");
		//pcb = deserializarPcb(package);
		printf("prog counter: %d\n",pcb->programCounter);
		printf("indice codigo: %d\n",pcb->indiceCodigo);
		printf("pid: %d\n", pcb->pid);
		printf("peso: %d\n", pcb->peso);

		while(quantumUtilizado<=quantum)
		{
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
			printf("Program counter: %d\n", pcb->programCounter);
			indiceCodigo = UMV_solicitarBytes(pcb->pid,pcb->indiceCodigo,pcb->programCounter,sizeof(t_intructions));
			memcpy(&(instruccionABuscar.start), indiceCodigo, sizeof(int));
			memcpy(&(instruccionABuscar.offset), indiceCodigo+sizeof(int), sizeof(int));
			printf("instruccionABuscar: %d\n", instruccionABuscar.start);
			printf("offset: %d\n", instruccionABuscar.offset);
			char* instruccionAEjecutar = malloc(instruccionABuscar.offset);
			instruccionAEjecutar = UMV_solicitarBytes(pcb->pid,pcb->segmentoCodigo,instruccionABuscar.start,instruccionABuscar.offset);
			if(instruccionAEjecutar == NULL)
			{
				printf("Error en la lectura de memoria. Finalizando la ejecución del programa");
				AnSISOP_finalizar(); // Es una primitiva.
				return 0;
			}
			printf("%s\n", instruccionAEjecutar);
			analizadorLinea(instruccionAEjecutar,&funciones,&kernel_functions); //Todo: fijarse el \0 al final del STRING. Faltan 2 argumentos
			if(terminarPrograma)
			{
				printf("Se termina el programa \n");
				scanf("%d", &i);
				return 0;
			}
			pcb->programCounter += 8;
			quantumUtilizado++;
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

	int a = -1;

	memset(&hintsKernel, 0, sizeof(hintsKernel));
	hintsKernel.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hintsKernel.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(IPKERNEL, PUERTOKERNEL, &hintsKernel, &kernelInfo);	// Carga en serverInfo los datos de la conexion
	kernelSocket = socket(kernelInfo->ai_family, kernelInfo->ai_socktype, kernelInfo->ai_protocol);
while (a == -1){
	a = connect(kernelSocket, kernelInfo->ai_addr, kernelInfo->ai_addrlen);
}
	printf("A: %d\n", a);
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

t_pcb* deserializarPcb(void* package)
{
	t_pcb* pcb = malloc(sizeof(t_pcb));
	int offset = 0;
	memcpy(&(pcb->pid),package,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->programCounter),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->tamanioIndiceEtiquetas),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->cursorStack),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->tamanioContextoActual),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->siguiente),package+offset,sizeof(t_pcb*));
	offset += sizeof(t_pcb*);

	memcpy(&(pcb->tamanioIndiceCodigo),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->segmentoStack),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->segmentoCodigo),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->indiceCodigo),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->indiceEtiquetas),package+offset,sizeof(int));
	offset += sizeof(int);

	memcpy(&(pcb->peso),package+offset,sizeof(int));

	pcb->siguiente= NULL;
	//free(package);

	return pcb;
}

void* UMV_solicitarBytes(int pid, int base, int offset, int tamanio)
{
	char operacion = 0;
	char confirmacion;
	void* buffer = malloc(tamanio);
	int mensaje[4], status;
	mensaje[0] = pid;
	mensaje[1] = base;
	mensaje[2] = offset;
	mensaje[3] = tamanio;
	send(socketUMV, &operacion, sizeof(char), 0);
	status = recv(socketUMV, &confirmacion, sizeof(char), 0);
	if(confirmacion != 0)
	{
		send(socketUMV, mensaje, 4*sizeof(int), 0);
		status = recv(socketUMV, buffer, tamanio, 0);
		printf("recibo datos segmento\n");
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
	printf("Primitiva Definir Variable\n");
	int a;
	char variable = (char) identificador_variable;
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack + pcb->tamanioContextoActual * 5),1,&variable);
	pcb->tamanioContextoActual++;
	//todo:Diccionario de variables. ??? ???
	return (pcb->cursorStack + pcb->tamanioContextoActual * 5)+1;
}

t_puntero AnSISOP_obtenerPosicionVariable(t_nombre_variable identificador_variable)
{
	printf("Primitiva Obtener Posicion Variable\n");
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
	printf("Primitiva Dereferenciar Variable\n");
	int* valor;
	valor = UMV_solicitarBytes(pcb->pid,pcb->segmentoStack,direccion_variable,4);
	return *valor;
}

void AnSISOP_asignar(t_puntero direccion_variable, t_valor_variable valor)
{
	printf("Primitiva Asignar Variable\n");
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

void AnSISOP_irAlLabel(t_nombre_etiqueta nombre_etiqueta)
{
	printf("Primitiva Ir al Label\n");
	void* buffer = malloc(pcb->tamanioIndiceEtiquetas);
//	char* etiquetas = malloc(pcb->tamanioIndiceEtiquetas);
	printf("tamanio indice etiquetas: %d\n", pcb->tamanioIndiceEtiquetas);
	t_puntero_instruccion instruccion;
	memcpy(buffer, UMV_solicitarBytes(pcb->pid,pcb->indiceEtiquetas,0,pcb->tamanioIndiceEtiquetas), pcb->tamanioIndiceEtiquetas);
//	memcpy(etiquetas,buffer,pcb->tamanioIndiceEtiquetas);
	instruccion = metadata_buscar_etiqueta(nombre_etiqueta,buffer,pcb->tamanioIndiceEtiquetas);
	//char* n = etiquetas;
	//printf("etiquetas: \n");
	//printf("%s \n", etiquetas);
//	printf("largo etiquetas: %d\n", strlen(etiquetas));
	printf("etiqueta: %s\n", nombre_etiqueta);
	printf("instruccion: %d\n", instruccion);
	scanf("%d", &instruccion);

//	free(etiquetas);
	return;
}

void AnSISOP_llamarSinRetorno(t_nombre_etiqueta etiqueta)
{
	printf("Primitiva Llamar Sin Retorno\n");
	printf("Etiqueta: %s\n", etiqueta);
	void* buffer = malloc(8);
	memcpy(buffer,&(pcb->cursorStack),4);
	memcpy((buffer+4),&(pcb->programCounter),4);
	UMV_enviarBytes(pcb->pid,pcb->segmentoStack,(pcb->cursorStack + (pcb->tamanioContextoActual*5)),8,buffer);
	AnSISOP_irAlLabel(etiqueta);
	free(buffer);
	return;
}


void AnSISOP_llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar)
{
	printf("Primitiva Llamar Con Retorno\n");
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
	printf("Primitiva Finalizar. ");
	if(pcb->cursorStack == 0)
	{
		printf("El programa termina\n");
		terminarPrograma = 1;
		return;
	}
	else
	{
		printf("Termina la funcion\n");
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

void AnSISOP_imprimir(t_valor_variable valor_mostrar)
{
	return;
}

void AnSISOP_imprimirTexto(char* texto)
{
	return;
}

void AnSISOP_entradaSalida(t_nombre_dispositivo dispositivo, int tiempo)
{
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

