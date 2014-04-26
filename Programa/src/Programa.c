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


char *ANSISOP_CONFIG;

int main(int cantArgs, char **args) {
	FILE *archivoConfig;
	FILE *script;
	char caracter;

	//Muestro cuantos argumentos se pasan
	printf("%d\n", cantArgs);

	//Muestro el primer argumento (ruta del script)
	printf("%s\n", args[1]);

	//Abro archivo de config y defino variable global
	ANSISOP_CONFIG = "./config";
	archivoConfig = fopen(ANSISOP_CONFIG,"r");

	//Abro script, leo el contenido y lo imprimo en pantalla
	script = fopen(args[1],"r");

	if (script == NULL)
	{
		printf("\nError de apertura del archivo. \n\n");
    }
	else
	{
	    printf("\nEl contenido del archivo de prueba es \n\n");
	    while (feof(script) == 0)
	    {
	    	caracter = fgetc(script);
	    	printf("%c",caracter);
	    }
    }
	fclose(script);


	return 0;
}

