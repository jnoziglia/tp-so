/*
 ============================================================================
 Name        : Programa.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Deargsion : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <parser/metadata_program.h>
#include <parser/parser.h>
#include <commons/config.h>
#include <commons/string.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define IP "127.0.0.1"//falta leer el archivo de configuracion y sacar estos datos de ahi
#define PUERTO "6667"


typedef struct {
	char ip[15];
	char puerto[4];
} t_regConfig;//definicion de la estructura del archivo de configuracion para q sea mas facil leerlo
char *ANSISOP_CONFIG;


int main(int cantArgs, char **args) {
	//FILE *archivoConfig;
	FILE *script;
	char caracter;
	char codigo[100000];
	int num=0;
	char* buffer = malloc(1000);

	//Muestro cuantos argumentos se pasan
	printf("%d\n", cantArgs);

	//Muestro el primer argumento (ruta del script)
	printf("%s\n", args[1]);

	//Abro archivo de config y defino variable global
	//ANSISOP_CONFIG = "./config";
	//archivoConfig = fopen(ANSISOP_CONFIG,"r");
     //fclose(archivoConfig);
	/*fread ( void * ptr, size_t size, size_t count, FILE * stream );//lee
	size_t fwrite(void *puntero, size_t tamano, size_t cantidad, FILE *archivo);//escribe
	*/

	//Abro script, leo el contenido
	script = fopen(args[1],"r");

	//empiezo con el socket
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(IP, PUERTO, &hints, &serverInfo);	// Carga en serverInfo los datos de la conexion

	int serverSocket;
	serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

	connect(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);	// No lo necesitamos mas


	if (script == NULL)
	{
		printf("\nError de apertura del archivo. \n\n");
    }
	else
	{
		fscanf(script,"%s", buffer);//borro el shellbag
		fgetc(script);
	    while (!feof(script))
	    {
	    	caracter = fgetc(script);
	    	codigo[num]=caracter;
	    	num++;
	    }

	    int tamanio=sizeof(char) * num-1;
	    printf("tamanio = %d", tamanio);
	    printf("tamanio-1 = %d", num-1);
	    printf("\nEl código es:");
	    printf("%s", codigo);
	    char mensaje[tamanio+1];
	    memcpy(mensaje, codigo, tamanio);
	    mensaje[tamanio+1] = '\0';
	    //printf("socket %d\n",serverSocket);
	    send(serverSocket, mensaje,tamanio, 0);
	    printf("Código enviado\n");
	    int status = recv(serverSocket, mensaje, tamanio+1, 0);
	    if (status == 0)
	    {
	    	close(serverSocket);
	    }
    }
	free(buffer);
	fclose(script);
	close(serverSocket);

	return 0;
}


