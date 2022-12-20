#include "fstream"
#include <iostream>
#include "unistd.h"
#include "random"
#include <ctime>
#include <mpi/mpi.h>
#include "stack"
#define REPLY_MESSAGE 1
#define SEND_REQUEST 0

int size, rank;

int count_access_token(int *RD){
    int result = 0;
    for (int i = 0; i < size; ++i) {
        result += RD[i];
    }
    return result;
}

void check_incoming_request(){
    int time, flag = 0;
    MPI_Request request;
    MPI_Status status;
    for (int i = 0; i < size-1; ++i) {
//        if (i != rank) {
            MPI_Irecv(&time, 1, MPI_INT,MPI_ANY_SOURCE,
                      SEND_REQUEST,MPI_COMM_WORLD,&request);
            MPI_Test(&request,&flag,&status);
            if (flag){
                int reply = 1;
                printf("Rank: %d | Receive request from %d\n", rank, status.MPI_SOURCE);
                MPI_Isend(&reply,1,MPI_INT,status.MPI_SOURCE,REPLY_MESSAGE,
                          MPI_COMM_WORLD,&request);
                printf("Rank: %d | Send reply to %d\n", rank, status.MPI_SOURCE);
                flag = 0;
            }
//        }
    }

    return;
}

void send_request(int time){
    int time_to_send = time;
    MPI_Request request;
    MPI_Status status;
    for (int i = 0; i < size; ++i){
        if (i != rank){
            MPI_Isend(&time_to_send,1,MPI_INT,i,
                      SEND_REQUEST,MPI_COMM_WORLD,&request);
            printf("Rank: %d | Send request to %d\n", rank, i);
        }
    }
}

int check_incoming_reply(){
    int time, flag = 0, answers = 0;
    MPI_Request request;
    MPI_Status status;
//    for (int i = 0; i < size; ++i) {
//        if (i != rank) {
            MPI_Irecv(&time, 1, MPI_INT,MPI_ANY_SOURCE,REPLY_MESSAGE,MPI_COMM_WORLD,&request);
            MPI_Test(&request,&flag,&status);
            if (flag){
                printf("Rank: %d | Receive reply from %d\n", rank, status.MPI_SOURCE);
                flag = 0;
                answers++;
                do {
                    flag = 0;
                    MPI_Irecv(&time, 1, MPI_INT,MPI_ANY_SOURCE,REPLY_MESSAGE,MPI_COMM_WORLD,&request);
                    MPI_Test(&request,&flag,&status);
                    printf("Rank: %d | Receive reply from %d\n", rank, status.MPI_SOURCE);
                    answers++;
                } while (flag);
//            }
//        }
    }
    return answers;
}
int main(int argc, char *argv[]){
    MPI_Init( &argc, &argv );
    int time, answers = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int* RD = (int *) malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
        RD[i] = 0;
    }
    std::stack<int> stack_of_requests;

    time = rank;
    printf("rank %d will sleep %d\n",rank, time);
    sleep(time);
    MPI_Request request;

    //section with check before asking access to critical section
    check_incoming_request();
    printf("Rank: %d | Receive requests %d\n", rank, answers);
    //request access to critical section
    send_request(time);
    //waiting answer from other threads
    while(answers != size-1){
        printf("Rank: %d | Receive answers %d\n", rank, answers);
        check_incoming_request();
        answers += check_incoming_reply();
        check_incoming_request();
        sleep(3);
    }
    printf("Rank: %d | FINISH \n", rank);
    MPI_Finalize();

    return 1;
}