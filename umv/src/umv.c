/*
 ============================================================================
 Name        : umv.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


void* solicitarBytes(void* inicio, int offset, int tamanio);
void enviarBytes(void* inicio, int offset, int tamanio, void* buffer);
int retornarBytesSolicitados(void* inicio, int offset, int tamanio);


int main(void)
{
	scanf();
	return 0;
}


int retornarBytesSolicitados(void* inicio,int offset, int tamanio) {
	void* bufferLleno;
	int* aux;
	int i;
	bufferLleno = solicitarBytes(inicio,3,tamanio);
	aux = bufferLleno;
	printf("\n buffer:\n");
	for (i=0;i<tamanio;i++)
	{
		printf("%p - ", aux+i);
		printf("%d\n", *(aux+i));
	}

	printf("\n Valor de aux: %d", *aux);
	return 0;
}



void* solicitarBytes(void* inicio, int offset, int tamanio)
{
	void* pComienzo;
	pComienzo = inicio + offset;
	void* buffer= malloc(tamanio);
	memcpy(buffer,pComienzo,tamanio);
	return buffer;
}

void enviarBytes(void* inicio, int offset, int tamanio, void* buffer)
{
	void* pComienzo;
	pComienzo = inicio + offset;
	memcpy(pComienzo,buffer,tamanio);
}
