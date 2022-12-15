#include "fstream"
#include <iostream>
#include "unistd.h"
#include "random"
#include <ctime>
#include <mpi/mpi.h>
#include "stack"


void critical_section(){
    std::ifstream my_file;
    my_file.open("critical.txt");
    if(my_file.is_open()){
        std::cout << "file exists";
        my_file.close();
        return;
    } else{
        std::fopen("critical.txt", "w");
        int time_to_sleep = 1 + random() % 10;
        sleep(time_to_sleep);
        std::remove("critical.txt");
        std::cout << "file does not exists";
        return;
    }
}
//#include <mpi.h>
void incoming_message_checker(int size, int rank, std::stack<int> stack_of_requests, int time){
    MPI_Request request;
    MPI_Status status;
    int flag, message;
    int tmp_time;
    for (int i = 0; i < size; ++i) {
        if (i != rank){
            flag = 0;
            MPI_Irecv( &tmp_time, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
            MPI_Test(&request, &flag, &status);
            if (flag){
                printf("Rank: %d | %d received message from %d\n",rank, rank, i);
                if (tmp_time > time){
                    printf("Rank: %d | %d pushed to stack\n", rank, i);
                    stack_of_requests.push(i);
                }
                else{
                    message = 1;
                    printf("Rank: %d | send reply to %d\n", rank, i);
                    MPI_Isend(&message, 1, MPI_INT, i, 1, MPI_COMM_WORLD,&request);
                }
            }
            else{
                message = -1;
                printf("Rank: %d | %d did not received message from %d\n",rank, rank, i);
            }
        }
    }
}
int count_access_token(int *RD, int size){
    int result = 0;
    for (int i = 0; i < size; ++i) {
        result += RD[i];
    }
    return result;
}
int main(int argc, char *argv[]){
    MPI_Init( &argc, &argv );
    int rank, size, time;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int* RD = (int *) malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
        RD[i] = 0;
    }
    std::stack<int> stack_of_requests;
//    MPI_Barrier(MPI_COMM_WORLD);
    time = rank;
    printf("rank %d will sleep %d\n",rank, time);
    sleep(time);
    MPI_Request request;

    //section with check before asking access to critical section
    MPI_Status status;
    int flag, message = -1;
    int tmp_time;
    for (int i = 0; i < size; ++i) {
        if (i != rank) {
            flag = 0;
            MPI_Irecv( &tmp_time, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
            MPI_Test(&request, &flag, &status);
            if (flag){
                printf("Rank: %d | %d received message from %d\n",rank,rank, i);
                message = 1;
                MPI_Isend(&message, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &request);
            }
            else{
                message = -1;
                printf("Rank: %d | %d did not received message from %d\n",rank, rank, i);
            }
        }
    }

    printf("\nRank: %d | ASKING ACCESS TO CRITICAL SECTION\n", rank);

    // section with asking access to critical section
    // sending ask of access
    for (int i = 0; i < size; ++i) {
        if (i != rank) {
            incoming_message_checker(size, rank, stack_of_requests, time);
            MPI_Isend(&time,1,MPI_INT,i,0,MPI_COMM_WORLD,&request);
            printf("Rank: %d | %d ask access from %d\n" ,rank, rank, i);
        }
    }
    //check if some incoming messages
    incoming_message_checker(size, rank, stack_of_requests, time);
    while (count_access_token(RD, size)!=4) {
        printf("Rank: %d | count_access_token = %d\n",rank, count_access_token(RD, size));
        for (int i = 0; i < size - 1; ++i) {
            if (rank != i) {
                flag = 0;
                int tmp_result;
//                printf("Rank: %d| Waiting answer from somebody\n", rank);
                MPI_Irecv(&tmp_result, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
                MPI_Test(&request, &flag, &status);
                if (flag) {
                    RD[i] = tmp_result;
//                    printf("Rank: %d| Received answer from %d\n", rank, i);
                } else {
//                    printf("Rank: %d | Did not receive answer from %d\n", rank, i);
                    continue;
                }
            }
        }
    }
    for (int i = 0; i < size; ++i) {
        printf("Rank: %d | RD[%d] = %d\n", rank, i, RD[i]);
    }

    //critical_section//
    printf("\nRank: %d | ENTERED IN CRITICAL SECTION\n", rank);

    //end of critical_section
    printf("\nRank: %d | EXIT FROM CRITICAL SECTION\n", rank);
    for (int i = 0; i < size; ++i) {
        if (i != rank) {
            MPI_Irecv( &tmp_time, 1, MPI_INT, MPI_ANY_SOURCE,
                       0, MPI_COMM_WORLD, &request);
            printf("Rank: %d | WAITING request\n",rank);
            MPI_Wait(&request, &status);

            printf("%d received message from %d\n",rank, status.MPI_SOURCE);
            message = 1;
            MPI_Send(&message, 1, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
        }
    }
    int i;
    while (!stack_of_requests.empty()){
        i = stack_of_requests.top();
        printf("%d received message from %d\n",rank, i);
        message = 1;
        MPI_Send(&message, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
        stack_of_requests.pop();
    }
    MPI_Finalize();

    return 1;
}