#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/times.h>
#define min(x, y) ((x)<(y)?(x):(y))

/** 
  Program to multiply a matrix times a matrix using both
  mpi to distribute the computation among nodes and omp
  to distribute the computation among threads.
  */


int main(int argc, char* argv[])
{
	int master = 0;
    int numtasks,taskid,numworkers,source,dest,rows,offset,i,j,k;
    double starttime, endtime;
    MPI_Status status;
	
  double *buffer, *ans;

    FILE *fileA;
    FILE *fileB;
    FILE *fileC;

    int row,  which_row_to_send,  numprocs, myid, sender, anstype;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	
    int totalrows,  extra, arows,  acols, brows, bcols, ncols, nrows;

    if (argc == 3)
    {

        fileA = fopen(argv[1], "r");
        fileB = fopen(argv[2], "r");

         //read FIRST file, get # rows, cols
        fscanf(fileA, "%d", &arows);
        fscanf(fileA, "%d", &acols);

        //read SECOND file, get # rows, cols
        fscanf(fileB, "%d", &brows);
        fscanf(fileB, "%d", &bcols);


        if (acols == brows){
            nrows = arows;
            ncols = bcols;
        } else {
            fprintf(stderr, "These 2 matrices cannot be multiplied.\n");
            return 0;
        }

        double aa[arows][acols],bb[brows][bcols],cc1[nrows][ncols], cc2[nrows][ncols];


	for(i = 0; i < arows; i++){
		for(k = 0; k<acols; k++) {
			fscanf(fileA, "%lf", &aa[i][k]);
		}
	}

	for(i = 0; i < brows; i++){
                for(k = 0; k<bcols; k++) {
                        fscanf(fileB, "%lf", &bb[i][k]);
                }
        }

    
	for(int y = 0; y<arows; y++){
		for(int z = 0; z<acols; z++) {
			printf("aa[%d][%d]=%lf\n", y, z, aa[y][z]); 
		}
	}
        
	 for(int y = 0; y<brows; y++){
                for(int z = 0; z<bcols; z++) {
                        printf("bb[%d][%d]=%lf\n", y, z, bb[y][z]);
                }
        }

        if (myid == 0) {  //MASTER CODE

            starttime = MPI_Wtime();

            offset = 0;
            numworkers = numprocs-1;
     	    
	    rows = nrows/numworkers;

	    if(rows==0) rows=1;

	    int averow = nrows/numworkers;
	    extra = nrows %numworkers;
 
	
            for (dest=1; dest<=min(numworkers, nrows); dest++){
                rows = (dest <= extra) ? averow+1 : averow;
	              MPI_Send(&offset, 1, MPI_INT, dest, 1, MPI_COMM_WORLD);//send the 
                MPI_Send(&rows, 1, MPI_INT, dest, 1, MPI_COMM_WORLD);
                MPI_Send(&aa[offset][0], rows*nrows, MPI_DOUBLE, dest, 1, MPI_COMM_WORLD);
                MPI_Send(&bb, brows*bcols, MPI_DOUBLE, dest, 1, MPI_COMM_WORLD);
                offset = offset + rows;
  	          	totalrows -= rows;    
	         } 

            for (i=1; i<=min(numworkers, nrows); i++){
                source = i;
                MPI_Recv(&offset, 1, MPI_INT, source, 2, MPI_COMM_WORLD, &status);
                MPI_Recv(&rows, 1, MPI_INT, source, 2, MPI_COMM_WORLD, &status);
                MPI_Recv(&cc1[offset][0], rows*nrows, MPI_DOUBLE, source, 2, MPI_COMM_WORLD, &status);
            }

            endtime = MPI_Wtime();

            //do the mmult the old way and put result in cc2
            for (i=0; i<nrows; i++){
                for (j=0; j<ncols; j++){
                    cc2[i][j] = 0.0;
                    for (k=0; k<acols; k++){
                        cc2[i][j] += aa[i][k] * bb[k][j];
                    }
                }
            }
    		
            int isSame = 1; 

            for (i=0; i<nrows; i++){
                for (j=0; j<ncols; j++){
                    if (cc1[i][j] != cc2[i][j]) isSame = 0;
                    printf("cc1[%d][%d] = %lf cc2[%d][%d] = %lf\n", i, j, cc1[i][j], i, j, cc2[i][j]);
                }
            }

            if (isSame){
                printf("These matrices are the same.\n");
            } else {
                printf("These matrices are NOT the same.\n");
            }

            fileC = fopen("matrixC.txt", "w");
            char space[] = " ";
            char nline[] = "\n";
            char s[20];
            for (i=0; i<nrows; i++){
                for (j=0; j<ncols; j++){
                    sprintf(s, "%f", cc1[i][j]);
                    fwrite(s, 1, sizeof(double), fileC);
                    if (j != ncols-1) fwrite(space, 1, sizeof(char), fileC);
                }
                fwrite(nline, 1, sizeof(char), fileC); 
            }
            printf("MatrixC has been stored in file: matrixC.txt\n");
	

        } 
        else if(myid > 0) 
        { //SLAVE CODE
     

	  if(nrows>=myid){
            source = 0;
            MPI_Recv(&offset, 1, MPI_INT, source, 1, MPI_COMM_WORLD, &status);
            MPI_Recv(&rows, 1, MPI_INT, source, 1, MPI_COMM_WORLD, &status);	
            MPI_Recv(&aa, rows*nrows, MPI_DOUBLE, source, 1, MPI_COMM_WORLD, &status);
            MPI_Recv(&bb, brows*bcols, MPI_DOUBLE, source, 1, MPI_COMM_WORLD, &status);
             
#pragma omp parallel default(none)  \
    shared(aa, bb, cc1, ncols, rows, brows) private(i, k, j)
#pragma omp for  
                for (int k=0; k<ncols; k++){
                    for (int i=0; i<rows; i++){
                        cc1[i][k] = 0.0; 
                        for(int j=0; j<brows; j++){
                            cc1[i][k] += aa[i][j] * bb[j][k];
                        }
                    }
                }

                MPI_Send(&offset, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
                MPI_Send(&rows, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
                MPI_Send(&cc1, rows*ncols, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
           
        }
      }
    }
    else
    {
        fprintf(stderr, "Two files with MatrixA and MatrixB must be passed as arguments\n");
        return 0;
    }

    MPI_Finalize();
    return 0;
}

