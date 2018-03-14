#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define min(x, y) ((x)<(y)?(x):(y))

/** 
	John Hyland - CIS 3238 - Lab 5 

    Program to multiply a matrix times a vector using both
    mpi to distribute the computation among nodes and omp
    to distribute the computation among threads.
*/

int main(int argc, char* argv[]) {
  int nrows, ncols;
  double *aa, *bb, *cc1, *cc2;
  double *buffer, *ans;
  double *times;
  double total_times;
  int run_index;
  int nruns;
  int myid, master, numprocs;
  double starttime, endtime;
  MPI_Status status;
  int i, j, numsent, sender;
  int anstype, row;
  srand(time(0));
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  if (argc > 1) {
   


    //read FIRST file
    FILE * fp;
    fp = fopen(argv[1], "r");
    fscanf(fp, "%d", &aRows);
    fscanf(fp, "%d", &aCols);
    printf("%d, %d", aRows, aCols);

    //allocate and fill first matrix, row major orderingg
    aa = malloc(sizeof(double)*aRows*aCols);
    for(int i=0; i<aRows; i++){
            for(int j =0; j<aCols; j++){
                    fscanf(fp, "%lf", &aa[i*aCols+j]);
            }
    }


    //read SECOND file
    fp = fopen(argv[2], "r");
    fscanf(fp, "%d", &bRows);
    fscanf(fp, "%d", &bCols);
    printf("%d, %d", bRows, bCols);

    //allocate and fill second matrix, row major orderingg
    bb = malloc(sizeof(double)*bRows*bCols);
    for(int i=0; i<bRows; i++){
            for(int j =0; j<bCols; j++){
                    fscanf(fp, "%lf", &bb[i*bCols+j]);
            }
    }

    for(int x = 0; x < bRows*bCols; x++){
            printf("element %d: %f\n",x+1, bb[x]);
    }


    //CHECK to see if matrices can be multipled. Exit if they cannot
    if(aCols!=bRows){
            printf("Dimension error. Matrices cannot be multipled. Exiting\n");
            exit(0);
    }


    //set dimensions of cc1
    ncols = bCols;
    nrows = aRows;


    cc1 = (double*)malloc(sizeof(double) * ncols * nrows)

    master = 0;
    if (myid == master) { //MASTER CODE

		starttime = MPI_Wtime();
		numsent = 0;

		//send all nodes a copy of bb
		MPI_Bcast(bb, bRows*bCols, MPI_DOUBLE, master, MPI_COMM_WORLD);

      	/* fill the buffer with a row of aa*/
        for (j = 0; j < aCols; j++) {
          buffer[j] = aa[ aCols*which_row_to_send+ j]; 
    	}

		/* send to the node with id of current row + 1 */
		MPI_Send(buffer, ncols, MPI_DOUBLE, which_row_to_send+1, which_row_to_send+1, MPI_COMM_WORLD);
		numsent++;


		/* for all rows in aa*/
		for (i = 0; i < nrows; i++) {

			/*receive an answer from any node */
			MPI_Recv(&ans, ncols, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	        sender = status.MPI_SOURCE; //get which node sent the answer
	        anstype = status.MPI_TAG;   //get the position in the resulting matrix

	     	/* put the answer into cc1 */
	        for(int x = 0; x < ncols; x++) {    
	        	cc1[ (anstype-1)*ncols + x] = ans[x];
	        }

			if (numsent < nrows) { // if there are still rows left to fill

				for (j = 0; j < aCols; j++) { // for all columns of bb
					buffer[j] = aa[aCols*numsent + j]; //put next row of aa in buffer
				}

	            // send the buffer back to the sender
	            MPI_Send(buffer, ncols, MPI_DOUBLE, sender, numsent+1 /*tag it numsent + 1*/ , MPI_COMM_WORLD);
	          	numsent++;

	        } 
	        else 
	        {
	        	/* if numsent exceeds number of rows, send nothing back */
	          MPI_Send(MPI_BOTTOM, 0, MPI_DOUBLE, sender, 0, MPI_COMM_WORLD);

	        }
      }
      endtime = MPI_Wtime();
      printf("%f\n",(endtime - starttime));
    } 
    else 
    { 
	    //SLAVE CODE
	    /* job: take a row of aa and use all of bb to find a row of cc */
	      
	    /*broadcast bb as the master */
		MPI_Bcast(bb, ncols, MPI_DOUBLE, master, MPI_COMM_WORLD);

	    if (myid <= nrows) { //if node is needed for work

	        while(1) { // in a loop

	        	//receive the buffer from the master	
	        	MPI_Recv(buffer, ncols, MPI_DOUBLE, master, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	            if (status.MPI_TAG == 0) { // if the master sent 0, break
	            	break;
	            }

	            row = status.MPI_TAG; // get which row from master, always the next one to work on
	            
	            // re-initialize ans to 0(row to send back)
	            for(int i = 0; i < ncols; i++) {
	            	ans[i] = 0;
	            }


#pragma omp parallel default(none) 
#pragma omp shared(ans) for reduction(+:ans)
	            for(int x = 0; x < ncols; x++) {
	            	/* do the multiplying*/
		            for(int j = 0; j < aCols; j++) {
		            	ans[x] = buffer[j] * /*all cols in b*/ bb[ x+(j*ncols) ];
		            }

	            }
			
			}

	        MPI_Send(&ans, ncols, MPI_DOUBLE, master, row, MPI_COMM_WORLD);
	    }
    }
  
	else { fprintf(stderr, "Usage matrix_times_matrix <size>\n"); }

	MPI_Finalize();
	return 0;
}
	                                                                                                                                                           102,1         Bot
