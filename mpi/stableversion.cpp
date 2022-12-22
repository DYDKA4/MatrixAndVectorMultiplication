#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mpi.h>
#include <mpi-ext.h>
#include <csignal>
#include <stack>
#include <err.h>
#include <climits>

int ProcNumbers, CurrentProcNumbers;
int ProcRank;

MPI_Comm main_comm = MPI_COMM_WORLD;
std::stack<int> failures{};
MPI_Errhandler errh;

void verbose_errhandler(MPI_Comm* pcomm, int* perr, ...) {
    MPI_Comm comm = *pcomm;
    int err = *perr;
    char errstr[MPI_MAX_ERROR_STRING];
    int i, rank, size, nf, len, eclass;
    MPI_Group group_c, group_f;
    int *ranks_gc, *ranks_gf;

    MPI_Error_class(err, &eclass);
    if( MPIX_ERR_PROC_FAILED != eclass ) {
        MPI_Abort(comm, err);
    }

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    /* We use a combination of 'ack/get_acked' to obtain the list of
     * failed processes (as seen by the local rank).
     */
    MPIX_Comm_failure_ack(comm);
    MPIX_Comm_failure_get_acked(comm, &group_f);
    MPI_Group_size(group_f, &nf);
    MPI_Error_string(err, errstr, &len);
    printf("Rank %d / %d: Notified of error %s. %d found dead: { ",
           rank, size, errstr, nf);

    /* We use 'translate_ranks' to obtain the ranks of failed procs
     * in the input communicator 'comm'.
     */
    ranks_gf = (int*)malloc(nf * sizeof(int));
    ranks_gc = (int*)malloc(nf * sizeof(int));
    MPI_Comm_group(comm, &group_c);
    for(i = 0; i < nf; i++)
        ranks_gf[i] = i;
    MPI_Group_translate_ranks(group_f, nf, ranks_gf,
                              group_c, ranks_gc);
    for(i = 0; i < nf; i++){
        printf("%d ", ranks_gc[i]);
        failures.push(ranks_gc[i]);
        CurrentProcNumbers--;
    }
    printf("}\n");
    free(ranks_gf); free(ranks_gc);
//    MPIX_Comm_shrink(*pcomm, &main_comm);
    MPI_Comm_rank(main_comm, &ProcRank);
    MPI_Comm_size(main_comm, &ProcNumbers);
}

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
    if (ProcRank == 2){
        raise(SIGKILL);
    }
    for (int i = 0; i < RowNum; ++i) {
        ProcResult[i] = 0;
        for (int j = 0; j < Size; ++j) {
            ProcResult[i] += ProcRows[i*Size+j]*Vector[j];
        }
    }
    printf("Rank %d/%d: Completed calculations\n", ProcRank, ProcNumbers);
}
void RecoveryResultCalculation(int *Result ,int* Matrix, int* Vector,int Size){
    int *ReceiveNum;   // Number of elements, that current process sends
    int *ReceiveInd;  // Index of the first element from current process
    int RestRows=Size; // Number of rows, that haven’t been distributed yet
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
    while (!failures.empty()){
        int number_of_proc = failures.top();
        for (int i = 0; i < ReceiveNum[i]; ++i) {
            Result[ReceiveInd[number_of_proc]+i] = 0;
            for (int j = 0; j < Size; ++j) {
                printf("Matrix element = %d, Vector element = %d\n",Matrix[(ReceiveInd[number_of_proc]+i)*Size+j], Vector[j]);
                Result[ReceiveInd[number_of_proc]+i] += Matrix[(ReceiveInd[number_of_proc]+i)*Size+j]*Vector[j];
            }
        }

        failures.pop();
    }
}
void ResultReplication(int* ProcResult, int* Result, int Size, int RowNum) {
    int *ReceiveNum;   // Number of elements, that current process sends
    int *ReceiveInd;  // Index of the first element from current process
    int *Tmp_Result;
    int RestRows=Size; // Number of rows, that haven’t been distributed yet

    // Alloc memory for temporary objects
    ReceiveNum = new int [ProcNumbers];
    ReceiveInd = new int [ProcNumbers];
    Tmp_Result = new int [sizeof(ProcResult)/4];
    // Determine the disposition of the result vector block
    ReceiveInd[0] = 0;
    ReceiveNum[0] = Size/ProcNumbers;

    for (int i=1; i<ProcNumbers; i++) {
        RestRows -= ReceiveNum[i-1];
        ReceiveNum[i] = RestRows/(ProcNumbers-i);
        ReceiveInd[i] = ReceiveInd[i-1]+ReceiveNum[i-1];
    }
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < ReceiveNum[ProcRank]; ++i) {
        printf("Rank %d/%d Result[%d] = %d\n", ProcRank, CurrentProcNumbers, i, ProcResult[i]);
    }
    // Gather the whole result vector on every processor
    if (ProcRank == 0){
        for (int i = 0; i < ReceiveNum[ProcRank]; ++i) {
            Result[i] = ProcResult[i];
        }
        for (int i = 0; i < CurrentProcNumbers-1; ++i) {
            MPI_Status status;
            MPI_Recv(Tmp_Result,sizeof(ProcResult)/4,MPI_INT,MPI_ANY_SOURCE,
                     0,MPI_COMM_WORLD, &status);
            for(int j = 0; j < ReceiveNum[status.MPI_SOURCE]; ++j) {
                printf("Proc 0 received message from %d, with %d, will start filling from %d\n",
                       status.MPI_SOURCE, Tmp_Result[j],ReceiveInd[status.MPI_SOURCE] + j);
                Result[ReceiveInd[status.MPI_SOURCE] + j] = Tmp_Result[j];
            }
        }
    }else{
        MPI_Send(ProcResult,sizeof(ProcResult)/4,MPI_INT,0,0,MPI_COMM_WORLD);
    }
    int err = MPI_Allgatherv(ProcResult, ReceiveNum[ProcRank], MPI_INT, Result, ReceiveNum, ReceiveInd, MPI_INT, MPI_COMM_WORLD);

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
    CurrentProcNumbers = ProcNumbers;
    MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);
    MPI_Comm_create_errhandler(verbose_errhandler, &errh);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD,
                            errh);
    MPI_Barrier(MPI_COMM_WORLD);
    if(ProcRank == 0){
        printf ("Parallel matrix-vector multiplication program\nCurrent size is %d\n", ProcNumbers);
    }
    for (int Size = 7; Size <= 20; Size+=2000) {
        totalDuration = 0;
        ProcessInitialization(Matrix, Vector, Result, ProcRows, ProcResult, Size, RowNum);

        for (int i = 0; i < 1; ++i) {

            Start = MPI_Wtime();
            dataSharing(Matrix, ProcRows, Vector, Size, RowNum);
//            MPI_Barrier(MPI_COMM_WORLD);
            ParallelResultCalcuation(ProcRows, Vector, ProcResult, Size, RowNum);
            ResultReplication(ProcResult, Result, Size, RowNum);
            if (ProcRank == 0) {
                RecoveryResultCalculation(Result, Matrix, Vector, Size);
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
