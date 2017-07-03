/*

* Contar cuerpos celestes
*
* Asignatura Computación Paralela (Grado Ingeniería Informática)
* Código secuencial base
*
* @author Ana Moretón Fernández
* @author Eduardo Rodríguez Gutiez
*
* @author Luis Higuero Casado
* @author Esther Cuervo Fernández
* @version v1.3
*
* (c) 2017, Grupo Trasgo, Universidad de Valladolid
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "cputils.h"
#include <mpi.h>


/* Substituir min por el operador */
#define min(x,y)    ((x) < (y)? (x) : (y))

/**
* Funcion secuencial para la busqueda de mi bloque
*/
int computation(int i, int y, int columns, int *matrixData, int *matrixResult, int *matrixResultCopy, int rango){
  int result,x;
  // si estamos en el proceso de rango 0, la matriz no tiene una fila extra por encima
  if(rango == 0){x = i;}
  // si estamos en cualquier otro proceso, existe una fila extra por encima de los datos que estamos recorriendo
  else{x=i+1;}
  result=matrixResultCopy[x*columns+y];
  if( result!=-1 ){
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
    if(matrixResult[i*columns+y] == result){ return 0; }
    // Si el indice cambia, actualizo matrix de resultados con el indice adecuado y retorno 1
    else { matrixResult[i*columns+y]=result; return 1;}

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
	int world_rank = -1;
	int world_size = -1;
	double t_ini;
	int i,j;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Comm_size (MPI_COMM_WORLD, &world_size);

	if ( world_rank == 0 ) {

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
		int valor;
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
		for(i=0;i<rows-0;i++){
			matrixData[i*(columns)+0]=0;
			matrixData[i*(columns)+columns-1]=0;
		}
		for(i=0;i<columns-0;i++){
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
		t_ini = cp_Wtime();
	}

	//
	// EL CODIGO A PARALELIZAR COMIENZA AQUI
	//

  int num_filas_por_proceso;
  int data[2];
  int faltan;
  int *vector_reparto;
  int *vector_offset;
  int *matrixResultTroceada;
  int *matrixCopyTroceada;
  int *matrixDataTroceada;

/*----------------------
|	ENVIO DE DATOS    		|
-----------------------*/
	if ( world_rank == 0 ) {
		matrixResult= (int *)malloc( (rows)*(columns) * sizeof(int) );
		matrixResultCopy= (int *)malloc( (rows)*(columns) * sizeof(int) );
		if ( (matrixResult == NULL)  || (matrixResultCopy == NULL)  ) {
			perror ("Error reservando memoria");
			return -1;
		}

    data[0]=columns;
    data[1]=rows;
  }

	//Mandamos el número de columnas y de filas que tiene la matriz
  MPI_Bcast(data,2,MPI_INT,0,MPI_COMM_WORLD);

  if( world_rank != 0) {
    columns = data[0];
    rows = data[1];
  }

/*----------------------
|	DEFINICIÓN DE COMM USADA |
-----------------------*/
  //la comm que vamos a usar es PROCESOS_USADOS que no se llenara con todos los procesos solo si hay menos filas que procesos en la COMM_WORLD
  MPI_Comm PROCESOS_USADOS;

  if(world_rank < rows){
    MPI_Comm_split(MPI_COMM_WORLD,0,world_rank,&PROCESOS_USADOS);
  }
  else{MPI_Comm_split(MPI_COMM_WORLD,MPI_UNDEFINED,world_rank,&PROCESOS_USADOS);}

  //si no estan en la COMM PROCESOS_USADOS, los procesos terminan
  if(PROCESOS_USADOS == MPI_COMM_NULL){MPI_Finalize(); return(0);}
  MPI_Comm_size(PROCESOS_USADOS, &world_size);
  MPI_Comm_rank(PROCESOS_USADOS, &world_rank);

  num_filas_por_proceso = rows/world_size;
  faltan = rows%(world_size);

/*----------------------------
|	REPARTO DE FILAS SOBRANTES |
---------------------------*/

  int num_filas_por_proceso_old = num_filas_por_proceso;
  int fila_inicial;
  //repartimos una fila sobrante a cada proceso
  if(world_rank < faltan){
    num_filas_por_proceso++;
  }

/*----------------------
|	REPARTO DE MATRIXDATA |
-----------------------*/

  //Definicion de vectores para repartir las filas de matrixData y matrixResultCopy
  vector_reparto = (int *)malloc(sizeof(int)*world_size);
  vector_offset = (int *)malloc(sizeof(int)*world_size);
  if( vector_reparto == NULL || vector_offset == NULL){
    perror ("Error reservando memoria");
    return -1;
  }

//El proceso 0 define los vectores de reparto y offset, teniendo en cuenta las filas sobrantes que pueden haber sido asignadas
//y una fila extra para el proceso 0 y dos filas extras para los demas.
//Estas filas extras se usaran para guardar la ultima fila de la que se encarga el proceso anterior y la primera del proceso siguiente, respectivamente
//esto se usa en matrixData y matrixCopy, en la funcion computation. Esta solapacion se refleja en el vector_offset
  if(world_rank == 0){
    vector_reparto[0]=(num_filas_por_proceso+1)*columns;
    vector_offset[0]=0;
    for(i=1;i<world_size;i++){
      vector_reparto[i] = (num_filas_por_proceso_old+2)*columns;
      vector_offset[i] = (num_filas_por_proceso_old*i-1)*columns;
      if(i<faltan){vector_reparto[i] += columns; vector_offset[i] += i*columns;}
      else{vector_offset[i]+=faltan*columns;}
    }
  }

  //Los Bcast se utilizan para que todos los procesos tengan el mismo vector de reparto y offset
  MPI_Bcast(vector_reparto,world_size,MPI_INT,0,MPI_COMM_WORLD);
  MPI_Bcast(vector_offset,world_size,MPI_INT,0,MPI_COMM_WORLD);

  //Se reserva memoria para la copia local de cada parte de matrixData, matrixResult, y matrixResultCopy que guarda cada proceso
  matrixDataTroceada = (int *)malloc(sizeof(int)*vector_reparto[world_rank]);
  matrixResultTroceada = (int *)malloc(sizeof(int)*num_filas_por_proceso*columns);
  matrixCopyTroceada = (int *)malloc(sizeof(int)*vector_reparto[world_rank]);
  if ( (matrixDataTroceada == NULL) || (matrixResultTroceada == NULL) || (matrixCopyTroceada == NULL) ) {
    perror ("Error reservando memoria");
    return -1;
  }

  MPI_Scatterv(matrixData, vector_reparto, vector_offset, MPI_INT, matrixDataTroceada, vector_reparto[world_rank], MPI_INT, 0, PROCESOS_USADOS);

  if(world_rank==0){fila_inicial=vector_offset[world_rank]/columns;}
  else{fila_inicial=(vector_offset[world_rank]/columns)+1;}

/*----------------------
|	ETIQUETADO INICIAL    |
-----------------------*/
   //El primer proceso tiene una distribucion de filas ligeramente distinta, le hacemos otro bucle especial para su caso
  if(world_rank == 0){
     for(i=0;i<num_filas_por_proceso;i++){
       for(j=0;j<columns;j++){
         matrixResultTroceada[i*columns+j] = -1;
         if(matrixDataTroceada[(i)*columns+j]!=0){
           matrixResultTroceada[i*columns+j]= i*columns+j;
         }
       }
     }
  }

  //El resto de procesos tienen el mismo esquema. Usamos la fila_inicial calculada anteriormente para calcular el indice correcto
  if(world_rank != 0){
      for(i=0;i<num_filas_por_proceso;i++){
	      for(j=0;j<columns;j++){
	        matrixResultTroceada[i*columns+j] = -1;
	        if(matrixDataTroceada[(i+1)*columns+j]!=0){
	          matrixResultTroceada[i*(columns)+j]= (fila_inicial+i)*(columns)+j;
	       }
	      }
      }
   }

  /* 4. Computacion */
	int t=0;
	/* 4.1 Flag para ver si ha habido cambios y si se continua la ejecucion */
	int flagCambio=1;
  int flagCambioReduce;

	/* 4.2 Busqueda de los bloques similiares */
	for(t=0; flagCambio !=0; t++){
	  flagCambio=0;
    flagCambioReduce=0;

    /*-----------------------------
    |  ACTUALIZACIÓN DE MATRIXCOPY |
    -----------------------------*/

    //Si es el primer proceso matrixCopy empieza en la fila 0, en los demas empieza en la fila 1. matrixResult empieza en ambos en la fila 0
    if(world_rank == 0){
        for(i=0;i<num_filas_por_proceso;i++){
          for(j=0;j<columns;j++){
	          matrixCopyTroceada[i*columns+j] = -1;
            if(matrixResultTroceada[i*(columns)+j]!=-1){
              matrixCopyTroceada[i*(columns)+j]=matrixResultTroceada[i*(columns)+j];
            }
	        }
        }
    }

    if(world_rank != 0){
        for(i=0;i<num_filas_por_proceso;i++){
          for(j=0;j<columns;j++){
            matrixCopyTroceada[(i+1)*columns+j] = -1;
            if(matrixResultTroceada[i*(columns)+j]!=-1){
              matrixCopyTroceada[(i+1)*(columns)+j]=matrixResultTroceada[i*(columns)+j];
            }
          }
        }
    }

   /*----------------------------------
   |  ACTUALIZACIÓN DE FILAS SOLAPADAS |
   ----------------------------------*/
  //Otros procesos modifican las 2 (o 1 en el caso del proceso 0) filas extras en matrixCopy, por lo que tenemos que enviarlas:
  //proceso 0->envia su ultima fila a proceso 1. Recibe la primera fila del proceso 1
  //..
  //proceso i-> envia su primera fila al proceso i-1. envia su ultima fila al proceso i+1. recibe la ultima fila del proceso i-1. recibe la primera fila del proceso i+1
  //...
  //proceso n-> envia su primera fila al proceso n-1. Recibe la ultima fila del proceso n-1.

  //Para evitar problemas, pareamos los sends y recv con etiquetas (0 para la primera fila del proceso receptor, 1 para la ultima fila del proceso receptor)

    MPI_Request req;
    if(world_rank == 0){
      MPI_Isend(&matrixCopyTroceada[(num_filas_por_proceso-1)*columns],columns,MPI_INT,1,0,PROCESOS_USADOS,&req);

      MPI_Recv(&matrixCopyTroceada[(num_filas_por_proceso)*columns],columns,MPI_INT,1,1,PROCESOS_USADOS,MPI_STATUS_IGNORE);
    }

   else if(world_rank == world_size-1){
      //matrixCopyTroceada del ultimo proceso tiene una fila de arriba, por lo que la primera fila suya esta en la posicion columns
      MPI_Isend(&matrixCopyTroceada[columns],columns,MPI_INT,world_rank-1,1,PROCESOS_USADOS,&req);

      MPI_Recv(&matrixCopyTroceada[0],columns,MPI_INT,world_rank-1,0,PROCESOS_USADOS,MPI_STATUS_IGNORE);
   }
   else{
     MPI_Isend(&matrixCopyTroceada[columns],columns,MPI_INT,world_rank-1,1,PROCESOS_USADOS,&req);
     MPI_Isend(&matrixCopyTroceada[num_filas_por_proceso*columns],columns,MPI_INT,world_rank+1,0,PROCESOS_USADOS,&req);

     MPI_Recv(&matrixCopyTroceada[0],columns,MPI_INT,world_rank-1,0,PROCESOS_USADOS,MPI_STATUS_IGNORE);
     MPI_Recv(&matrixCopyTroceada[(num_filas_por_proceso+1)*columns],columns,MPI_INT,world_rank+1,1,PROCESOS_USADOS,MPI_STATUS_IGNORE);
   }

   /*--------------
   |  COMPUTACIÓN  |
   --------------*/

   for(i=0;i<num_filas_por_proceso;i++){
      for(j=1;j<columns-1;j++){
        flagCambioReduce=flagCambioReduce+computation(i,j,columns, matrixDataTroceada, matrixResultTroceada, matrixCopyTroceada,world_rank);
      }
    }

	flagCambio=0;

  //todos los procesos reciben la suma de los flagCambio de cada proceso
  MPI_Allreduce(&flagCambioReduce,&flagCambio, 1, MPI_INT, MPI_SUM, PROCESOS_USADOS);
  }

  free(vector_offset);
  free(vector_reparto);
  free(matrixDataTroceada);
  free(matrixCopyTroceada);


	/* 4.3 Inicio cuenta del numero de bloques */

  /*--------------------
  |  CUENTA DE BLOQUES  |
  --------------------*/

  int numBlocksReduce = 0;
  for(i=0;i<num_filas_por_proceso;i++){
    for(j=0;j<columns;j++){
      if(matrixResultTroceada[i*columns+j] == (fila_inicial+i)*columns+j){
        numBlocksReduce++;
      }
    }
  }

  numBlocks = 0;
  //Se suman todas las cuentas del numero de bloques de cada proceso, y se guarda el resultado en el proceso 0, que lo imprimira en la solucion
  MPI_Reduce(&numBlocksReduce,&numBlocks,1, MPI_INT,MPI_SUM,0,PROCESOS_USADOS);

  free(matrixResultTroceada);
	//
	// EL CODIGO A PARALELIZAR TERMINA AQUI
	//
	if ( world_rank == 0 ) {

		/* PUNTO DE FINAL DE MEDIDA DE TIEMPO */
		double t_fin = cp_Wtime();

		/* 5. Comprobación de resultados */
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

	MPI_Finalize();
	return 0;
}
