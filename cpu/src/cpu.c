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

typedef struct {
	/*
	 * DEFINIR VARIABLE
	 *
	 * Reserva en el Contexto de Ejecución Actual el espacio necesario para una variable llamada identificador_variable y la registra tanto en el Stack como en el Diccionario de Variables. Retornando la posición del valor de esta nueva variable del stack
	 * El valor de la variable queda indefinido: no deberá inicializarlo con ningún valor default.
	 * Esta función se invoca una vez por variable, a pesar que este varias veces en una línea.
	 * Ej: Evaluar "variables a, b, c" llamará tres veces a esta función con los parámetros "a", "b" y "c"
	 *
	 * @sintax	TEXT_VARIABLE (variables identificador[,identificador]*)
	 * @param	identificador_variable	Nombre de variable a definir
	 * @return	Puntero a la variable recien asignada
	 */
	t_puntero (*AnSISOP_definirVariable)(t_nombre_variable identificador_variable);

	/*
	 * OBTENER POSICION de una VARIABLE
	 *
	 * Devuelve el desplazamiento respecto al inicio del segmento Stacken que se encuentra el valor de la variable identificador_variable del contexto actual.
	 * En caso de error, retorna -1.
	 *
	 * @sintax	TEXT_REFERENCE_OP (&)
	 * @param	identificador_variable 		Nombre de la variable a buscar (De ser un parametro, se invocara sin el '$')
	 * @return	Donde se encuentre la variable buscada
	 */
	t_puntero (*AnSISOP_obtenerPosicionVariable)(t_nombre_variable identificador_variable);

	/*
	 * DEREFERENCIAR
	 *
	 * Obtiene el valor resultante de leer a partir de direccion_variable, sin importar cual fuera el contexto actual
	 *
	 * @sintax	TEXT_DEREFERENCE_OP (*)
	 * @param	direccion_variable	Lugar donde buscar
	 * @return	Valor que se encuentra en esa posicion
	 */
	t_valor_variable (*AnSISOP_dereferenciar)(t_puntero direccion_variable);

	/*
	 * ASIGNAR
	 *
	 * Inserta una copia del valor en la variable ubicada en direccion_variable.
	 *
	 * @sintax	TEXT_ASSIGNATION (=)
	 * @param	direccion_variable	lugar donde insertar el valor
	 * @param	valor	Valor a insertar
	 * @return	void
	 */
	void (*AnSISOP_asignar)(t_puntero direccion_variable, t_valor_variable valor);

	/*
	 * OBTENER VALOR de una variable COMPARTIDA
	 *
	 * Pide al kernel el valor (copia, no puntero) de la variable compartida por parametro.
	 *
	 * @sintax	TEXT_VAR_START_GLOBAL (!)
	 * @param	variable	Nombre de la variable compartida a buscar
	 * @return	El valor de la variable compartida
	 */
	t_valor_variable (*AnSISOP_obtenerValorCompartida)(t_nombre_compartida variable);

	/*
	 * ASIGNAR VALOR a variable COMPARTIDA
	 *
	 * Pide al kernel asignar el valor a la variable compartida.
	 * Devuelve el valor asignado.
	 *
	 * @sintax	TEXT_VAR_START_GLOBAL (!) IDENTIFICADOR TEXT_ASSIGNATION (=) EXPRESION
	 * @param	variable	Nombre (sin el '!') de la variable a pedir
	 * @param	valor	Valor que se le quire asignar
	 * @return	Valor que se asigno
	 */
	t_valor_variable (*AnSISOP_asignarValorCompartida)(t_nombre_compartida variable, t_valor_variable valor);


	/*
	 * IR a la ETIQUETA
	 *
	 * Cambia la linea de ejecucion a la correspondiente de la etiqueta buscada.
	 *
	 * @sintax	TEXT_GOTO (goto )
	 * @param	t_nombre_etiqueta	Nombre de la etiqueta
	 * @return	void
	 */
	void (*AnSISOP_irAlLabel)(t_nombre_etiqueta t_nombre_etiqueta);

	/*
	 * LLAMAR SIN RETORNO
	 *
	 * Preserva el contexto de ejecución actual para poder retornar luego al mismo.
	 * Modifica las estructuras correspondientes para mostrar un nuevo contexto vacío.
	 *
	 * Los parámetros serán definidos luego de esta instrucción de la misma manera que una variable local, con identificadores numéricos empezando por el 0.
	 *
	 * @sintax	Sin sintaxis particular, se invoca cuando no coresponde a ninguna de las otras reglas sintacticas
	 * @param	etiqueta	Nombre de la funcion
	 * @return	void
	 */
	void (*AnSISOP_llamarSinRetorno)(t_nombre_etiqueta etiqueta);

	/*
	 * LLAMAR CON RETORNO
	 *
	 * Preserva el contexto de ejecución actual para poder retornar luego al mismo, junto con la posicion de la variable entregada por donde_retornar.
	 * Modifica las estructuras correspondientes para mostrar un nuevo contexto vacío.
	 *
	 * Los parámetros serán definidos luego de esta instrucción de la misma manera que una variable local, con identificadores numéricos empezando por el 0.
	 *
	 * @sintax	TEXT_CALL (<-)
	 * @param	etiqueta	Nombre de la funcion
	 * @param	donde_retornar	Posicion donde insertar el valor de retorno
	 * @return	void
	 */
	void (*AnSISOP_llamarConRetorno)(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);


	/*
	 * FINALIZAR
	 *
	 * Cambia el Contexto de Ejecución Actual para volver al Contexto anterior al que se está ejecutando, recuperando el Cursor de Contexto Actual y el Program Counter previamente apilados en el Stack.
	 * En caso de estar finalizando el Contexto principal (el ubicado al inicio del Stack), deberá finalizar la ejecución del programa.
	 *
	 * @sintax	TEXT_END (end )
	 * @param	void
	 * @return	void
	 */
	void (*AnSISOP_finalizar)(void);

	/*
	 * RETORNAR
	 *
	 * Cambia el Contexto de Ejecución Actual para volver al Contexto anterior al que se está ejecutando, recuperando el Cursor de Contexto Actual, el Program Counter y la direccion donde retornar, asignando el valor de retorno en esta, previamente apilados en el Stack.
	 *
	 * @sintax	TEXT_RETURN (return )
	 * @param	retorno	Valor a ingresar en la posicion corespondiente
	 * @return	void
	 */
	void (*AnSISOP_retornar)(t_valor_variable retorno);

	/*
	 * IMPRIMIR
	 *
	 * Envía valor_mostrar al Kernel, para que termine siendo mostrado en la consola del Programa en ejecución.
	 *
	 * @sintax	TEXT_PRINT (print )
	 * @param	valor_mostrar	Dato que se quiere imprimir
	 * @return	void
	 */
	void (*AnSISOP_imprimir)(t_valor_variable valor_mostrar);

	/*
	 * IMPRIMIR TEXTO
	 *
	 * Envía mensaje al Kernel, para que termine siendo mostrado en la consola del Programa en ejecución. mensaje no posee parámetros, secuencias de escape, variables ni nada.
	 *
	 * @sintax TEXT_PRINT_TEXT (textPrint )
	 * @param	texto	Texto a imprimir
	 * @return void
	 */
	void (*AnSISOP_imprimirTexto)(char* texto);

	/*
	 *	ENTRADA y SALIDA
	 *
	 *
	 *	@sintax TEXT_IO (io )
	 *	@param	dispositivo	Nombre del dispositivo a pedir
	 *	@param	tiempo	Tiempo que se necesitara el dispositivo
	 *	@return	void
	 */
	void (*AnSISOP_entradaSalida)(t_nombre_dispositivo dispositivo, int tiempo);
} misfunciones;

typedef struct {
	/*
	 * WAIT
	 *
	 * Informa al kernel que ejecute la función wait para el semáforo con el nombre identificador_semaforo.
	 * El kernel deberá decidir si bloquearlo o no.
	 *
	 * @sintax	TEXT_WAIT (wait )
	 * @param	identificador_semaforo	Semaforo a aplicar WAIT
	 * @return	void
	 */
	void (*AnSISOP_wait)(t_nombre_semaforo identificador_semaforo);

	/*
	 * SIGNAL
	 *
	 * Informa al kernel que ejecute la función signal para el semáforo con el nombre identificador_semaforo.
	 * El kernel deberá decidir si desbloquear otros procesos o no.
	 *
	 * @sintax	TEXT_SIGNAL (signal )
	 * @param	identificador_semaforo	Semaforo a aplicar SIGNAL
	 * @return	void
	 */
	void (*AnSISOP_signal)(t_nombre_semaforo identificador_semaforo);
} t_func_kernel;


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
void AnSISOP_irAlLabel(t_nombre_etiqueta t_nombre_etiqueta);
void AnSISOP_llamarSinRetorno(t_nombre_etiqueta etiqueta);
void AnSISOP_llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
void AnSISOP_finalizar(void);
void AnSISOP_retornar(t_valor_variable retorno);
void AnSISOP_imprimir(t_valor_variable valor_mostrar);
void AnSISOP_imprimirTexto(char* texto);
void AnSISOP_entradaSalida(t_nombre_dispositivo dispositivo, int tiempo);
void AnSISOP_wait(t_nombre_semaforo identificador_semaforo);
void AnSISOP_signal(t_nombre_semaforo identificador_semaforo);

/* Variables Globales */
int kernelSocket;
int socketUMV;
int quantum = 10; //todo:quantum que lee de archivo de configuración
int estadoCPU;
bool matarCPU = 0;
t_pcb* pcb;

//todo:Primitivas, Hot Plug.

int main(){
	//t_puntero (*funciontest)(t_nombre_variable) = AnSISOP_definirVariable;

	AnSISOP_funciones funciones;
	funciones.AnSISOP_asignar = (*AnSISOP_asignar);
	funciones.AnSISOP_asignarValorCompartida = (*AnSISOP_asignarValorCompartida);
	funciones.AnSISOP_definirVariable = (*AnSISOP_definirVariable);
	funciones.AnSISOP_dereferenciar = (*AnSISOP_dereferenciar);
	funciones.AnSISOP_entradaSalida = (*AnSISOP_entradaSalida);
	funciones.AnSISOP_finalizar = (*AnSISOP_finalizar);
	funciones.AnSISOP_imprimir = (*AnSISOP_imprimir);
	funciones.AnSISOP_imprimirTexto = (*AnSISOP_imprimirTexto);
	funciones.AnSISOP_irAlLabel = (*AnSISOP_irAlLabel);
	funciones.AnSISOP_llamarConRetorno = (*AnSISOP_llamarConRetorno);
	funciones.AnSISOP_llamarSinRetorno = (*AnSISOP_llamarSinRetorno);
	funciones.AnSISOP_obtenerPosicionVariable = (*AnSISOP_obtenerPosicionVariable);
	funciones.AnSISOP_obtenerValorCompartida = (*AnSISOP_obtenerValorCompartida);
	funciones.AnSISOP_retornar = (*AnSISOP_retornar);

	AnSISOP_kernel fkernel;
//	fkernel.AnSISOP_signal = (*AnSISOP_signal);
//	fkernel.AnSISOP_wait = (*AnSISOP_wait);


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
	t_pcb* pcb = malloc (sizeof(t_pcb));

	int i;
	while(1)
	{
		printf("Espero pcb\n");
		int superMensaje[11];

		int recibido = recv(kernelSocket,superMensaje,sizeof(t_pcb),0);
		pcb->pid = superMensaje[0];
		pcb->segmentoCodigo = superMensaje[1];
		pcb->segmentoStack=	superMensaje[2] ;
		pcb->cursorStack	=superMensaje[3]  ;
		pcb->indiceCodigo=	superMensaje[4] ;
		pcb->indiceEtiquetas=	superMensaje[5]  ;
		pcb->programCounter= superMensaje[6] ;
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
			//sleep(30);
			analizadorLinea(instruccionAEjecutar,&funciones,&fkernel); //Todo: fijarse el \0 al final del STRING. Faltan 2 argumentos
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
	printf("Definir variable %c\n",identificador_variable);
	sleep(10);
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

