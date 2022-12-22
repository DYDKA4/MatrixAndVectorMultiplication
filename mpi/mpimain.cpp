#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi/mpi.h>
//#include "mpi-ext.h"
#include <signal.h>

int ProcNumbers;
int ProcRank;

void print_vector(int Size, int* Vector) {
    for (int i = 0; i < Size; ++i) {
        printf("Vector[%d] = %d \t", i, Vector[i]);
    }
}

void print_matrix(int Size, int* Matrix){
    for (int i = 0; i < Size; ++i) {
        printf("\n");
        for (int j = 0; j < Size; ++j) {
//            Matrix[i*Size+j] = rand();
            printf("Matrix[%d][%d] = %d \t", i,j, Matrix[i*Size+j]);
        }
    }
    printf("\n");
    return;
}
void RandomDataInitialization(int* Matrix, int* Vector, int Size){
    srand(1);
    for (int i = 0; i < Size; ++i) {
//        Vector[i] = rand();
        Vector[i] = i;
        for (int j = 0; j < Size; ++j) {
//            Matrix[i*Size+j] = rand();
            Matrix[i*Size+j] = i;
        }
    }
    print_vector(Size, Vector);
    print_matrix(Size, Matrix);
}

void ProcessInitialization (int* &Matrix, int* &Vector, int* &Result,
                            int* &ProcRows, int* &ProcResult, int &Size, int &RowNum){
    //Setting the size of the initial matrix and vector
    int RestRows;
    if (ProcRank == 0){
        do {
            printf("\nChosen objects size = %d\n", Size);
            if (Size < ProcNumbers) {
                printf("Wrong Size, size must be > ProcNumbers\n");
            }
        } while ((Size < ProcNumbers));
    }
    MPI_Bcast(&Size, 1, MPI_INT, 0, MPI_COMM_WORLD);

    RestRows = Size;
    for (int i=0; i<ProcRank; i++)
        RestRows = RestRows-RestRows/(ProcNumbers-i);
    RowNum = RestRows/(ProcNumbers-ProcRank);

    Vector = new int [Size];
    Result = new int [Size];
    ProcRows = new int [RowNum*Size];
    ProcResult = new int [RowNum];

    if(ProcRank == 0){
        Matrix = new int [Size*Size];
        RandomDataInitialization(Matrix, Vector, Size);
    }
}

void freeMemory(int* Matrix, int* Vector, int* Result,int* ProcRows,int* ProcResult){
    if (ProcRank == 0)
        delete [] Matrix;
    delete [] Vector;
    delete [] Result;
    delete [] ProcRows;
    delete [] ProcResult;
}

void dataSharing(int* Matrix, int* ProcRows, int* Vector,int Size, int RowNum){
    int *SendNum; // Number of elements sent to the process
    int *SendInd; // Index of the first data element sent to the process
    int RestRows=Size; // Number of rows, that haven’t been distributed yet
    MPI_Bcast(Vector,Size,MPI_INT,0,MPI_COMM_WORLD);

    // Alloc memory for temporary objects
    SendInd = new int [ProcNumbers];
    SendNum = new int [ProcNumbers];

    // Determine the disposition of the matrix rows for current process
    RowNum = (Size/ProcNumbers);
    SendNum[0] = RowNum*Size;
    SendInd[0] = 0;
    for (int i=1; i<ProcNumbers; i++) {
        RestRows -= RowNum;
        RowNum = RestRows/(ProcNumbers-i);
        SendNum[i] = RowNum*Size;
        SendInd[i] = SendInd[i-1]+SendNum[i-1];
    }

    // Scatter the rows
    MPI_Scatterv(Matrix , SendNum, SendInd, MPI_INT, ProcRows,SendNum[ProcRank], MPI_INT, 0, MPI_COMM_WORLD);
    //Free the memory
    delete [] SendNum;
    delete [] SendInd;
}

void ParallelResultCalcuation(int* ProcRows, int* Vector,int* ProcResult, int Size, int RowNum){
    if (ProcRank == (ProcNumbers-1)){
        raise(SIGKILL);
    }
    for (int i = 0; i < RowNum; ++i) {
        ProcResult[i] = 0;
        for (int j = 0; j < Size; ++j) {
            ProcResult[i] += ProcRows[i*Size+j]*Vector[j];
        }
    }
}

void ResultReplication(int* ProcResult, int* Result, int Size, int RowNum) {
    int *ReceiveNum;   // Number of elements, that current process sends
    int *ReceiveInd;  // Index of the first element from current process

    int RestRows=Size; // Number of rows, that haven’t been distributed yet

    // Alloc memory for temporary objects
    ReceiveNum = new int [ProcNumbers];
    ReceiveInd = new int [ProcNumbers];

    // Determine the disposition of the result vector block
    ReceiveInd[0] = 0;
    ReceiveNum[0] = Size/ProcNumbers;
    for (int i=1; i<ProcNumbers; i++) {
        RestRows -= ReceiveNum[i-1];
        ReceiveNum[i] = RestRows/(ProcNumbers-i);
        ReceiveInd[i] = ReceiveInd[i-1]+ReceiveNum[i-1];
    }

    // Gather the whole result vector on every processor
    MPI_Allgatherv(ProcResult, ReceiveNum[ProcRank], MPI_INT, Result, ReceiveNum, ReceiveInd, MPI_INT, MPI_COMM_WORLD);

    // Free the memory
    delete [] ReceiveNum;
    delete [] ReceiveInd;
}
int main(int argc, char* argv[]) {
    int* Matrix, *Vector, *Result, Size;
    int *ProcRows, *ProcResult, RowNum;
    double Start, Finish, Duration, totalDuration = 0;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &ProcNumbers);
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);

    if(ProcRank == 0){
        printf ("Parallel matrix-vector multiplication program\n");
    }
    for (int Size = 4; Size <= 20; Size+=2000) {
        totalDuration = 0;
        ProcessInitialization(Matrix, Vector, Result, ProcRows, ProcResult, Size, RowNum);

        for (int i = 0; i < 1; ++i) {

            Start = MPI_Wtime();
            dataSharing(Matrix, ProcRows, Vector, Size, RowNum);
            ParallelResultCalcuation(ProcRows, Vector, ProcResult, Size, RowNum);

            ResultReplication(ProcResult, Result, Size, RowNum);
            if (ProcRank == 0) {
                for (int j = 0; j < Size; ++j) {
                    printf("Result[%d] = %d \n", j, Result[j]);
                }
            }
            Finish = MPI_Wtime();
            Duration = Finish - Start;


            if (ProcRank == 0) {
                //printf("Time of execution = %f\n", Duration);
                totalDuration += Duration;
            }
        }
        if (ProcRank == 0) {
            printf("AVG time of execution = %f\n", totalDuration / 20);
        }
        freeMemory(Matrix, Vector, Result, ProcRows, ProcResult);
    }
    MPI_Finalize();

    return 0;
}
