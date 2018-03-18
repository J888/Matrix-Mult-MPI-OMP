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


void fill_matrix(double *matrix, int n, int m, FILE *fp)



int main(int argc, char* argv[])
{
    double *aa;	/* the A matrix */
    double *bb;	/* the B matrix */
    double *cc1;	/* A x B computed using the omp-mpi code you write */
    double *cc2;	/* A x B computed using the conventional algorithm */
    int myid, numprocs, nrows, ncols;
    double starttime, endtime;
    MPI_Status status;
    /* insert other global variables here */ 
    int i, j, k, rows, offset, dest, source, 
        arows, acols, brows, bcols, numworkers,
        totalrows;   
    FILE *fileA;
    FILE *fileB;
    FILE *fileC;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    if (argc == 3){

        fileA = fopen(argv[1], "r");
        fileB = fopen(argv[2], "r");

         //read FIRST file, get # rows, cols
        fscanf(fileA, "%d", &arows);
        fscanf(fileA, "%d", &acols);


        //read SECOND file, get # rows, cols
        fscanf(fileB, "%d", &brows);
        fscanf(fileB, "%d", &bcols);
        printf("%d, %d", brows, bcols);


        if (acols == brows){
            nrows = arows;
            ncols = bcols;
        } else {
            fprintf(stderr, "These 2 matrices cannot be multiplied.\n");
            return 0;
        }

        aa = malloc(sizeof(double) * arows * acols);
        bb = malloc(sizeof(double) * brows * bcols);
        gen_matrix(aa, arows, acols, fileA);
        gen_matrix(bb, brows, bcols, fileB);
        cc1 = malloc(sizeof(double) * nrows * ncols);
        double a[arows][acols];
        double b[brows][bcols];
        double c[nrows][ncols];
        double c2[nrows][ncols];

        i = j = 0;
        for (i = 0; i < arows; i++){
            for (j = 0; j < acols; j++){
                if (!fscanf(fileA, "%lf", &a[i][j])) break;
            }
        }	
        rewind(fileA);

        for (i = 0; i < brows; i++){
            for (j = 0; j < bcols; j++){
                if (!fscanf(fileB, "%lf", &b[i][j])) break;
            }
        }	
        rewind(fileB);

        if (myid == 0) {  //MASTER CODE
            starttime = MPI_Wtime();
            offset = 0;
            numworkers = numprocs-1;
            rows = nrows/numworkers;
            if (rows == 0) rows = 1;
            int averow, extra;
            averow = nrows/numworkers;
            extra = nrows%numworkers;
            for (dest=1; dest<=min(numworkers, nrows); dest++){
                rows = (dest <= extra) ? averow+1 : averow;
                MPI_Send(&offset, 1, MPI_INT, dest, 1, MPI_COMM_WORLD);
                MPI_Send(&rows, 1, MPI_INT, dest, 1, MPI_COMM_WORLD);
                MPI_Send(&a[offset][0], rows*nrows, MPI_DOUBLE, dest, 1, MPI_COMM_WORLD);
                MPI_Send(&b, brows*bcols, MPI_DOUBLE, dest, 1, MPI_COMM_WORLD);
                offset = offset + rows;
                totalrows -= rows;
            } 

            for (i=1; i<=min(numworkers, nrows); i++){
                source = i;
                MPI_Recv(&offset, 1, MPI_INT, source, 2, MPI_COMM_WORLD, &status);
                MPI_Recv(&rows, 1, MPI_INT, source, 2, MPI_COMM_WORLD, &status);

                MPI_Recv(&c[offset][0], rows*nrows, MPI_DOUBLE, source, 2, MPI_COMM_WORLD, &status);
            }
            endtime = MPI_Wtime();
            printf("%f\n", (endtime - starttime));
            //      mmult(cc2, aa, arows, acols, bb, brows, bcols); 
            for (i=0; i<nrows; i++){
                for (j=0; j<ncols; j++){
                    c2[i][j] = 0;
                    for (k=0; k<nrows; k++){
                        c2[i][j] += a[i][k] * b[k][j];
                    }

                }
            }

            int isSame = 1; 

            for (i=0; i<nrows; i++){
                for (j=0; j<ncols; j++){
                    if (c[i][j] != c2[i][j]) isSame = 0;
                    printf("c[%d][%d] = %lf c2[%d][%d] = %lf\n", i, j, c[i][j], i, j, c2[i][j]);
                }
            }
            if (isSame){
                printf("These matrices are the same.\n");
            } else {
                printf("These matrices are NOT the same.\n");
            }

            fileC = fopen("matrixC", "w");
            char space[] = " ";
            char nline[] = "\n";
            char s[20];
            for (i=0; i<nrows; i++){
                for (j=0; j<ncols; j++){
                    sprintf(s, "%f", c[i][j]);
                    fwrite(s, 1, sizeof(double), fileC);
                    if (j != ncols-1) fwrite(space, 1, sizeof(char), fileC);
                }
                fwrite(nline, 1, sizeof(char), fileC); 
            }
            printf("MatrixC has been stored in file: matrixC.\n");

        }else { //SLAVE CODE
      
            if (nrows >= myid){
                source = 0;
                MPI_Recv(&offset, 1, MPI_INT, source, 1, MPI_COMM_WORLD, &status);
                MPI_Recv(&rows, 1, MPI_INT, source, 1, MPI_COMM_WORLD, &status);	
                MPI_Recv(&a, rows*nrows, MPI_DOUBLE, source, 1, MPI_COMM_WORLD, &status);
                MPI_Recv(&b, brows*bcols, MPI_DOUBLE, source, 1, MPI_COMM_WORLD, &status);
             
#pragma omp parallel default(none)  \
    shared(a, b, c, ncols, rows, brows) private(i, k, j)
#pragma omp for
                for (k=0; k<ncols; k++){
                    for (i=0; i<rows; i++){
                        c[i][k] = 0; 
                        for(j=0; j<brows; j++){
                            c[i][k] += a[i][j] * b[j][k];
                        }
                    }
                }
                printf("Multiplication from worker %d complete!\n", myid);
                MPI_Send(&offset, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
                MPI_Send(&rows, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
                MPI_Send(&c, rows*ncols, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
            }
        }
    } else {
        fprintf(stderr, "Two files with MatrixA and MatrixB must be passed as arguments\n");
        return 0;
    }
    MPI_Finalize();

    return 0;
}




void fill_matrix(double *matrix, int n, int m, FILE *fp) {    
    
    fscanf(config_file, "%*[^\n]\n", NULL);//skip first line of file

    int i, j;
    double c;
    i = j = 0;
    for (i = 0; i < n; i++){
        for (j = 0; j < m; j++){
            if (!fscanf(fp, "%lf", &matrix[n*i+j])) break;
        }
    }   
    rewind(fp);
}


