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

void check_incoming_request(){
    int time, flag = 0;
    MPI_Request request;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, SEND_REQUEST, MPI_COMM_WORLD, &flag, &status);
    int reply = 1;
    while(flag){
        flag = 0;
        MPI_Irecv(&time, 1, MPI_INT,status.MPI_SOURCE,
                  SEND_REQUEST,MPI_COMM_WORLD,&request);
        printf("Rank: %d | Receive request from %d\n", rank, status.MPI_SOURCE);
        MPI_Isend(&reply,1,MPI_INT,status.MPI_SOURCE,REPLY_MESSAGE,
                  MPI_COMM_WORLD,&request);
        printf("Rank: %d | Send reply to %d\n", rank, status.MPI_SOURCE);
        MPI_Iprobe(MPI_ANY_SOURCE, SEND_REQUEST, MPI_COMM_WORLD, &flag, &status);
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
    MPI_Iprobe(MPI_ANY_SOURCE, REPLY_MESSAGE, MPI_COMM_WORLD, &flag, &status);
    int reply = 1;
    while(flag){
        flag = 0;
        MPI_Irecv(&time, 1, MPI_INT,status.MPI_SOURCE,REPLY_MESSAGE,
                  MPI_COMM_WORLD,&request);
        printf("Rank: %d | Receive reply from %d\n", rank, status.MPI_SOURCE);
        answers++;
        MPI_Iprobe(MPI_ANY_SOURCE, REPLY_MESSAGE, MPI_COMM_WORLD, &flag, &status);
    }
    return answers;
}

void critical_section(){
    std::ifstream my_file;
    my_file.open("critical.txt");
    if(my_file.is_open()){
        std::cout << "file exists\n";
        my_file.close();
        return;
    } else{
        std::fopen("critical.txt", "w");
        int time_to_sleep = 1 + random() % 10;
        sleep(time_to_sleep);
        std::remove("critical.txt");
        std::cout << "file does not exists\n";
        return;
    }
}

std::stack<int> check_incoming_request_while_requesting_access(int time, std::stack<int> stack_of_requests){
    int tmp_time, flag = 0;
    MPI_Request request;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, SEND_REQUEST, MPI_COMM_WORLD, &flag, &status);

    int reply = 1;
    while(flag){
        flag = 0;
        MPI_Irecv(&tmp_time, 1, MPI_INT,status.MPI_SOURCE,
                  SEND_REQUEST,MPI_COMM_WORLD,&request);
        printf("Rank: %d | Receive request from %d\n", rank, status.MPI_SOURCE);
        if (tmp_time > time){
            printf("Rank: %d | Save request from %d\n", rank, status.MPI_SOURCE);
            stack_of_requests.push(status.MPI_SOURCE);
        }else{
            MPI_Isend(&reply,1,MPI_INT,status.MPI_SOURCE,REPLY_MESSAGE,
                      MPI_COMM_WORLD,&request);
            printf("Rank: %d | Send reply to %d\n", rank, status.MPI_SOURCE);
        }
        MPI_Iprobe(MPI_ANY_SOURCE, SEND_REQUEST, MPI_COMM_WORLD, &flag, &status);
    }
    return stack_of_requests;
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
    //request access to critical section
    send_request(time);
    //waiting answer from other threads
    while(answers != size-1){
        printf("Rank: %d | Receive answers %d\n", rank, answers);
        stack_of_requests = check_incoming_request_while_requesting_access(time, stack_of_requests);
        answers += check_incoming_reply();
        sleep(1);
    }
    printf("Rank: %d | ENTERING critical section \n", rank);
    critical_section();
    printf("Rank: %d | EXIT critical section \n", rank);

    check_incoming_request();
    while (!stack_of_requests.empty()){
        printf("%d received message from %d\n",rank, stack_of_requests.top());
        int message = 1;
        MPI_Isend(&message, 1, MPI_INT, stack_of_requests.top(),
                  REPLY_MESSAGE, MPI_COMM_WORLD, &request);
        stack_of_requests.pop();
    }
    check_incoming_request();

    MPI_Finalize();

    return 1;
}