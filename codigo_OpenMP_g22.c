/*
* Contar cuerpos celestes
*
* Asignatura ComputaciÃ³n Paralela (Grado IngenierÃ­a InformÃ¡tica)
* CÃ³digo secuencial base
*
* @author Ana MoretÃ³n FernÃ¡ndez
* @version v1.2
*
* (c) 2017, Grupo Trasgo, Universidad de Valladolid
*/

/*Necesitamos datos del servidor para poder optimizar bien (TamaÃ±o caches, numero de hilos, etc)
*Creo que eran 32 o 16, pero casualmente es multiplo del tamaÃ±o de la imagen y creo que tambien del numero de elementos del a encontrar en la imagen mas grande, a tener en cuenta
*/



#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "cputils.h"
#include <omp.h>


/* Substituir min por el operador */
#define min(x,y)    ((x) < (y)? (x) : (y))

/**
* Funcion secuencial para la busqueda de mi bloque 
*/
int computation(int x, int y, int columns, int* matrixData, int *matrixResult, int *matrixResultCopy){
	// Inicialmente cojo mi indice
	int result=matrixResultCopy[x*columns+y];
	int dato=matrixData[x*columns+y];
    //Los -1 no se transmiten a la Matriz copia, por lo que el valor result correcto del vacio es 0
	if( result!= 0){
		//Si es de mi mismo grupo, entonces actualizo

		//Sacar aqui el matrix data a una variable, se accede cuatro veces, mejor leer una
		//Quiza un switch aumente rendimiento
		//Cambiar el orden puede aumentar el rendimiento
			if(matrixData[(x-1)*columns+y] == dato)
			{
				result = min (result, matrixResultCopy[(x-1)*columns+y]);
			}

			if(matrixData[(x+1)*columns+y] == dato)
			{
				result = min (result, matrixResultCopy[(x+1)*columns+y]);
			}
			
			if(matrixData[x*columns+y-1] == dato)
			{
				result = min (result, matrixResultCopy[x*columns+y-1]);
			}	
			
			if(matrixData[x*columns+y+1] == dato)
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
	int indice;



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
	int i,j,valor;
	fscanf (f, "%d\n", &rows);
	fscanf (f, "%d\n", &columns);
	// AÃ±ado dos filas y dos columnas mas para los bordes
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


	/* PUNTO DE INICIO MEDIDA DE TIEMPO */
	double t_ini = cp_Wtime();

//
// EL CODIGO A PARALELIZAR COMIENZA AQUI
//
   //CREAMOS YA LOS HILOS QUE SE MANTENDRAN DURANTE TODA LA PARTE PARALELA INICIAL reservado de memoria + etiquetado. 

   //Quitado, da peor resultado. y hay que usar exit en lugar de return
	/* 3. Etiquetado inicial */
	matrixResult= (int *)malloc( (rows)*(columns) * sizeof(int) );
	matrixResultCopy= (int *)malloc( (rows)*(columns) * sizeof(int) );
	if ( (matrixResult == NULL)  || (matrixResultCopy == NULL)  ) {
 		perror ("Error reservando memoria");
	   	return -1;
	}
	//Aqui es el primer etiquetado,solo detecta que no es fondo, facilmente paralelizable
  //collapse da tiempo peor
    #pragma omp parallel for firstprivate(matrixResult, matrixData,rows,columns) private(i,j,indice) default (none)
	//NO CAMBIAR EL ORDEN. CAMBIA EL RESULTADO
  for(i=0;i< rows; i++){
		for(j=0;j< columns; j++){
			indice=i*columns+j;
			matrixResult[indice]=-1;
			if(matrixData[indice]!=0){
				matrixResult[indice]=indice;
			}
		}
	}



	/* 4. Computacion */
	int t=0;
	/* 4.1 Flag para ver si ha habido cambios y si se continua la ejecucion */
	int flagCambio=1; 

	/* 4.2 Busqueda de los bloques similiares */
	//Mientras se hagan modificaciones en las pasadas
	for(t=0; flagCambio !=0; t++){
		flagCambio=0; 

		/* 4.2.1 Actualizacion copia */
		//Una copia de la matriz,facilmente paralelizable
		//Bucle i j parece correcto
	#pragma omp parallel firstprivate(columns,rows,matrixResult,matrixResultCopy,matrixData) private(i,j,indice) reduction(+:flagCambio) default (none)
	{
    //NO TOCAR EL ORDEN DE BUCLE EMPEORA MUCHO
    //collapse correcto
    #pragma omp for collapse(2)
		for(i=1;i<rows-1;i++){
			for(j=1;j<columns-1;j++){
				indice=i*(columns)+j;
				if(matrixResult[indice]!=-1){
					matrixResultCopy[indice]=matrixResult[indice];
				}
			}
		}

		/* 4.2.2 Computo y detecto si ha habido cambios */
		//Aqui es cuando se modifica la matriz, a la vez que se comprueba si ha habido cambios, en el flag cambio quedaria el nÂº de operaciones hechas

        //La funcion modifica la matriz pero al ser una operacion de busqueda del minimo no importa que distintos hilos escriban y lean las mismas posiciones, el minimo del bloque se mantiene.
        #pragma omp for  
    //NO TOCAR EL ORDEN DE BUCLE EMPEORA MUCHO
		for(i=1;i<rows-1;i++){
			for(j=1;j<columns-1;j++){
				flagCambio= flagCambio+ computation(i,j,columns, matrixData, matrixResult, matrixResultCopy);
			}
		}
		}


		#ifdef DEBUG
			printf("\nResultados iter %d: \n", t);
			for(i=1;i<rows-1;i++){
				for(j=1;j<columns-1;j++){
					printf ("%d\t", matrixResult[i*columns+j]);
				}
				printf("\n");
			}
		#endif

	}

	/* 4.3 Inicio cuenta del numero de bloques */
	//Esto puede ser paralelizable, problema de dividir en bloques y no contar dos o mas veces el mismo
	numBlocks=0;
  //collapse correcto
	#pragma omp parallel for private(i,j,indice) firstprivate(matrixResult,rows,columns) reduction(+:numBlocks) collapse(2) default (none)
  for(i=1;i<rows-1;i++){
    for(j=1;j<columns-1;j++){	
			indice=i*columns+j;
			if(matrixResult[indice] == indice){
				numBlocks++;
			} 
				
		}
	}

//
// EL CODIGO A PARALELIZAR TERMINA AQUI
//

	/* PUNTO DE FINAL DE MEDIDA DE TIEMPO */
 	double t_fin = cp_Wtime();



	/* 5. ComprobaciÃ³n de resultados */
  	double t_total = (double)(t_fin - t_ini);

	printf("Result: %d\n", numBlocks);
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

}
