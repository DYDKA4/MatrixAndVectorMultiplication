#include <iostream>
#include "mpi/mpi.h"

using namespace std;

void show_arr(int* arr, int size)
{
    for(int i=0; i < size; i++) cout << arr[i] << " ";
    cout << endl;
}

int main(int argc, char **argv)
{
    int size, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(rank == 0)
    {
        int* arr = new int[size];
        for(int i=0; i < size; i++) arr[i] = i;
        for(int i=1; i < size; i++) MPI_Send(arr, i, MPI_INT, i, 5, MPI_COMM_WORLD);
    }
    else
    {
        int count;
        MPI_Status status;

        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &count);
        int* buf = new int[count];

        MPI_Recv(buf, count, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        cout << "Process:" << rank << " || Count: " << count << " || Array: ";
        show_arr(buf, count);
    }
    MPI_Finalize();
    return 0;
}