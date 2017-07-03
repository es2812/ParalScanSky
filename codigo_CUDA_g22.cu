/*
* Contar cuerpos celestes
*
* Asignatura Computación Paralela (Grado Ingeniería Informática)
* Código secuencial base
*
* @author Ana Moretón Fernández, Arturo Gonzalez-Escribano
* @author Luis Higuero Casado, Esther Cuervo Fernández
* @version v1.3
*
* (c) 2017, Grupo Trasgo, Universidad de Valladolid
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <cuda.h>
#include "cputils.h"

/* Substituir min por el operador */
#define min(x,y)    ((x) < (y)? (x) : (y))
#define THREADSPORBLOQUE 128

/**
* Funcion secuencial para la busqueda de mi bloque
*/
__device__ int computation(int x, int y, int columns, int* matrixData, int *matrixResult, int *matrixResultCopy){
	// Inicialmente cojo mi indice
	int result=matrixResultCopy[x*columns+y];
	if( result!= -1){
		//Si es de mi mismo grupo, entonces actualizo
		if(matrixData[(x-1)*columns+y] == matrixData[x*columns+y])
		{
			result = min (result, matrixResultCopy[(x-1)*columns+y]);
		}
		if(matrixData[(x+1)*columns+y] == matrixData[x*columns+y])
		{
			result = min (result, matrixResultCopy[(x+1)*columns+y]);
		}
		if(matrixData[x*columns+y-1] == matrixData[x*columns+y])
		{
			result = min (result, matrixResultCopy[x*columns+y-1]);
		}
		if(matrixData[x*columns+y+1] == matrixData[x*columns+y])
		{
			result = min (result, matrixResultCopy[x*columns+y+1]);
		}

		// Si el indice no ha cambiado retorna 0
		if(matrixResult[x*columns+y] == result){ return 0; }
		// Si el indice cambia, actualizo matrix de resultados con el indice adecuado y retorno 1
		else { matrixResult[x*columns+y]=result; return 1;}

	}
	return 0;
}


/*Definicion de kernels*/
__global__ void etiquetadoInicial(int *matrixResult, int *matrixResultCopy, int *matrixData, int tamMatriz){
	int indiceThread = (blockIdx.x*blockDim.x)+(threadIdx.x);

	if(indiceThread < tamMatriz){
		matrixResultCopy[indiceThread] = -1;
		matrixResult[indiceThread] = -1;

		if(matrixData[indiceThread] != 0){
			matrixResult[indiceThread] = indiceThread;
		}
	}

}

__global__ void actualizacionCopia(int *matrixResult, int *matrixResultCopy, int tamMatriz){
		int indiceThread = (blockIdx.x*blockDim.x)+(threadIdx.x);
		if(indiceThread < tamMatriz){
			if(matrixResult[indiceThread] != -1){
				matrixResultCopy[indiceThread] = matrixResult[indiceThread];
			}
		}
}

__global__ void computo(int *matrixResult, int *matrixResultCopy, int *matrixData, int filas, int columnas, int *arrayCambio){
		int indiceThread = (blockIdx.x*blockDim.x)+(threadIdx.x);
		if(indiceThread < columnas*filas){
			int x,y;
			x = indiceThread/columnas; //la fila en la que está la posicion es el entero resultante de dividir el indice por el tamaño de la fila...
			y = indiceThread%columnas; //y la columna es el resto
			arrayCambio[indiceThread] = computation(x, y, columnas, matrixData, matrixResult, matrixResultCopy);
		}
}
//Esta funcion suma todo su bloque en su posicion de auxiliar
__global__ void recuento(int *arrayCambio, int *arrayAux, int rows , int columns){
		int numbloque = blockIdx.x;
		int indiceThreadGlobal = (blockIdx.x*blockDim.x)+(threadIdx.x);

		int i;

		for (i=2;i<=blockDim.x;i=i*2){
			arrayCambio[indiceThreadGlobal]=arrayCambio[indiceThreadGlobal]+arrayCambio[indiceThreadGlobal+i/2];
			__syncthreads();
		}

		if((indiceThreadGlobal%blockDim.x)==0){
			arrayAux[numbloque]=arrayCambio[indiceThreadGlobal];
		}
}

/*Fin de kernels*/

/**
* Funcion principal
*/
int main (int argc, char* argv[])
{

	/* 1. Leer argumento y declaraciones */
	if (argc < 2) 	{
		printf("Uso: %s <imagen_a_procesar>\n", argv[0]);
		return(EXIT_SUCCESS);
	}
	char* image_filename = argv[1];

	int rows=-1;
	int columns =-1;
	int *matrixData=NULL;
	int *matrixResult=NULL;
	int *matrixResultCopy=NULL;
	int numBlocks=-1;



	/* 2. Leer Fichero de entrada e inicializar datos */

	/* 2.1 Abrir fichero */
	FILE *f = cp_abrir_fichero(image_filename);

	// Compruebo que no ha habido errores
	if (f==NULL)
	{
	   perror ("Error al abrir fichero.txt");
	   return -1;
	}

	/* 2.2 Leo valores del fichero */
	int i,j;
	fscanf (f, "%d\n", &rows);
	fscanf (f, "%d\n", &columns);
	// Añado dos filas y dos columnas mas para los bordes
	rows=rows+2;
	columns = columns+2;

	/* 2.3 Reservo la memoria necesaria para la matriz de datos */
	matrixData= (int *)malloc( rows*(columns) * sizeof(int) );
	if ( (matrixData == NULL)   ) {
 		perror ("Error reservando memoria");
	   	return -1;
	}

	/* 2.4 Inicializo matrices */
	for(i=0;i< rows; i++){
		for(j=0;j< columns; j++){
			matrixData[i*(columns)+j]=-1;
		}
	}
	/* 2.5 Relleno bordes de la matriz */
	for(i=1;i<rows-1;i++){
		matrixData[i*(columns)+0]=0;
		matrixData[i*(columns)+columns-1]=0;
	}
	for(i=1;i<columns-1;i++){
		matrixData[0*(columns)+i]=0;
		matrixData[(rows-1)*(columns)+i]=0;
	}
	/* 2.6 Relleno la matriz con los datos del fichero */
	for(i=1;i<rows-1;i++){
		for(j=1;j<columns-1;j++){
			fscanf (f, "%d\n", &matrixData[i*(columns)+j]);
		}
	}
	fclose(f);

	#ifdef WRITE
		printf("Inicializacion \n");
		for(i=0;i<rows;i++){
			for(j=0;j<columns;j++){
				printf ("%d\t", matrixData[i*(columns)+j]);
			}
			printf("\n");
		}
	#endif

	cudaSetDevice(0);
	cudaDeviceSynchronize();

	/* PUNTO DE INICIO MEDIDA DE TIEMPO */
	double t_ini = cp_Wtime();

//
// EL CODIGO A PARALELIZAR COMIENZA AQUI
//

	cudaError_t err1, err2, err3, err4, err5; //variables para comprobación de errores.
	int tamMatriz = rows*columns;
	int numbloques=tamMatriz/THREADSPORBLOQUE + (tamMatriz%THREADSPORBLOQUE != 0);

	matrixResult= (int *)malloc( (rows)*(columns) * sizeof(int) );
	int *arrayCambio = (int *)malloc(numbloques*sizeof(int));

	if ( (matrixResult == NULL) || (arrayCambio==NULL) ) {
 		perror ("Error reservando memoria");
	   	return -1;
	}

	/*Envio de las matrices a la GPU*/
	//Inicializacion
	int *GPUmatrixResult;
	int *GPUmatrixResultCopy;
	int *GPUmatrixData;

	int *GPUArrayCambio;
	int *GPUArrayCambioAux;

	err1 = cudaMalloc(&GPUmatrixResult, rows*columns*sizeof(int));
	err2 = cudaMalloc(&GPUmatrixResultCopy, rows*columns*sizeof(int));
	err3 = cudaMalloc(&GPUmatrixData, rows*columns*sizeof(int));

	err4 = cudaMalloc(&GPUArrayCambio, rows*columns*sizeof(int));
	err5 = cudaMalloc(&GPUArrayCambioAux,numbloques*sizeof(int));



	if(err1 != cudaSuccess || err2 != cudaSuccess || err3 != cudaSuccess || err4 != cudaSuccess || err5!=cudaSuccess){
		printf("Error en el reservado de memoria GPU\n");
		return -1;
	}

	//Envio a GPU

	err1 = cudaMemcpy(GPUmatrixData, matrixData, rows*columns*sizeof(int), cudaMemcpyHostToDevice);

	if(err1 != cudaSuccess){
		printf("Error enviando las matrices a la GPU\n");
		return -1;
	}

	/*Definicion de grids*/


	dim3 bloque(THREADSPORBLOQUE,1);
	dim3 grid(numbloques,1);

	/* 3. Etiquetado inicial */
	etiquetadoInicial<<<grid,bloque>>>(GPUmatrixResult, GPUmatrixResultCopy, GPUmatrixData,tamMatriz);


	/* 4. Computacion */
	int t=0;
	/* 4.1 Flag para ver si ha habido cambios y si se continua la ejecucion */
	int flagCambio=1;

	/* 4.2 Busqueda de los bloques similiares */
	for(t=0; flagCambio !=0; t++){
		flagCambio=0;

		/* 4.2.1 Actualizacion copia */

		actualizacionCopia<<<grid,bloque>>>(GPUmatrixResult,GPUmatrixResultCopy,tamMatriz);

		/* 4.2.2 Computo y detecto si ha habido cambios */

		computo<<<grid,bloque>>>(GPUmatrixResult, GPUmatrixResultCopy, GPUmatrixData, rows, columns, GPUArrayCambio);

		recuento<<<grid,bloque>>>(GPUArrayCambio,GPUArrayCambioAux,rows,columns);

		//El resultado de flagCambio se guarda en un array, hacemos reduccion en el host

		err1 = cudaMemcpy(arrayCambio,GPUArrayCambioAux,numbloques*sizeof(int),cudaMemcpyDeviceToHost);

		if(err1 != cudaSuccess){
			printf("Error copiando memoria al host %s\n",err1);
			return -1;
		}

		for(i=0;i<numbloques;i++){
		  flagCambio = arrayCambio[i];
		  if(flagCambio != 0) break;
		}

		#ifdef DEBUG
			printf("\nResultados iter %d: \n", t);
			for(i=0;i<rows;i++){
				for(j=0;j<columns;j++){
					printf ("%d\t", matrixResult[i*columns+j]);
				}
				printf("\n");
			}
		#endif
		//printf("FlagCambio%d\n",flagCambio);

	}

  //Una vez terminada la computación, se habrá generado el matrixResult final en la GPU

	err1 = cudaMemcpy(matrixResult,GPUmatrixResult,rows*columns*sizeof(int),cudaMemcpyDeviceToHost);

	if(err1 != cudaSuccess){
		printf("Error copiando memoria al host %s\n",err1);
		return -1;
	}


	/* 4.3 Inicio cuenta del numero de bloques */
	numBlocks=0;
	for(i=1;i<rows-1;i++){
		for(j=1;j<columns-1;j++){
			if(matrixResult[i*columns+j] == i*columns+j) numBlocks++;
		}
	}

	/* Liberacion de memoria*/
	cudaFree(GPUmatrixResult);
	cudaFree(GPUmatrixData);
	cudaFree(GPUmatrixResultCopy);
	cudaFree(GPUArrayCambio);
	cudaFree(GPUArrayCambioAux);

//
// EL CODIGO A PARALELIZAR TERMINA AQUI
//

	/* PUNTO DE FINAL DE MEDIDA DE TIEMPO */
	cudaDeviceSynchronize();
 	double t_fin = cp_Wtime();


	/* 5. Comprobación de resultados */
  	double t_total = (double)(t_fin - t_ini);

	printf("Result: %d:%d\n", numBlocks, t);
	printf("Time: %lf\n", t_total);
	#ifdef WRITE
		printf("Resultado: \n");
		for(i=0;i<rows;i++){
			for(j=0;j<columns;j++){
				printf ("%d\t", matrixResult[i*columns+j]);
			}
			printf("\n");
		}
	#endif

	/* 6. Liberacion de memoria */
	free(matrixData);
	free(matrixResult);
	free(matrixResultCopy);

	return 0;
}
