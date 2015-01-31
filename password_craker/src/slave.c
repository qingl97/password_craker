#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <omp.h>
#include "mpi.h"

#include "wordHandler.h"
#include "mpi_utils.h"

#define TASK_SIZE	1000

typedef struct Task {
	unsigned long long start;
	unsigned long long end;
	struct Task* next;
} Task;

char* alphabet;
int alphabet_len;
int num_threads_slave;
int num_threads_finished;
unsigned long long num_perms_per_thead;
unsigned long long pwd_found;
Task* listHeader, *listTail;
unsigned long long num_tasks_left;
int isFound, terminateComm, stopSearching;
int k, rank;
char* pw;
int requested;
int hasInterval;

// allocate memory for every task in the list
void updateTaskList(Interval* itv){
	unsigned long long start, end;
	start = itv->start;
	end = itv->start + itv->num_perms - 1;
	while(start){
		++num_tasks_left;
		Task* slice = malloc(sizeof(Task));
		slice->start = start;
		if(start+TASK_SIZE-1 >= end){ // the last slice
			slice->end = end;
			start = 0;
		}
		else{
			slice->end = start + TASK_SIZE - 1;
			start += TASK_SIZE;
		}
		slice->next = NULL;

		if(listHeader == NULL){
			listHeader = listTail = slice;
		}
		else{
			listTail->next = slice;
			listTail = slice;
		}
	}
}	

Task* nextTask(){
	Task* ret;
	ret = listHeader;
	if(listHeader != NULL){
		listHeader = listHeader->next;
                --num_tasks_left;
        }
	return ret;
}

void clearTaskList(){
	while(listHeader != NULL){
		Task* tmp = listHeader;
		listHeader = listHeader->next;
		free(tmp);
	}
}

/**
 * thread responsible for communicating with the slave threads and the parent process
 */
void thread_comm(MPI_Comm inter){
	int send, send1, send2; // flags to make sure some messages must be send only once
	send = send1 = send2 = 0;

	MPI_Datatype mpi_type_interval;
	createMPIDatatype_Interval(&mpi_type_interval);
  
	MPI_Status status;
	MPI_Request req;
	int flag;
	while(!terminateComm){
		Interval task;
                MPI_Iprobe(0, MPI_ANY_TAG, inter, &flag, &status);
                if(flag){
                        switch(status.MPI_TAG){
                        case TAG_INTERVAL: // an interval arrived
				send = 0;
                                MPI_Recv(&task, 1, mpi_type_interval, 0, TAG_INTERVAL, inter, &status);
                                // fprintf(stderr, "[SLAVE %d] received interval [%llu - %llu] from Master\n", rank, task.start, task.start+task.num_perms-1);
#pragma omp critical 
                                {
                                        updateTaskList(&task);
                                }
                                break;
                                
                        case TAG_NO_MORE_INTERVAL:
                                // fprintf(stderr, "[SLAVE %d] received from Master NO_MORE_INTERVAL\n", rank);
				send = 0;
                                MPI_Recv(0,0,MPI_INT, 0, TAG_NO_MORE_INTERVAL,inter, &status);
				hasInterval = 0;
                                break;
                               
                        case TAG_KILL_SELF:
                                MPI_Recv(0,0,MPI_INT, 0, TAG_KILL_SELF,inter, &status);
				MPI_Send(0,0,MPI_INT, 0, TAG_CONFIRM_EXIT, inter);
                                // fprintf(stderr, "[SLAVE %d] to exit.\n\n\n", rank);
                                stopSearching = 1;
                                terminateComm = 1;
                                break;
                        }
                }
                else{
			if(isFound){ // one thread of this process found password
                                if(send2 == 0) {
					MPI_Send(&pwd_found, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_PASSWORD, inter);
					send2 = 1;
				}
			}
                        else if(num_threads_finished == num_threads_slave){ // all threads didn't find the password
                                if(send1 == 0){ // send only once
					MPI_Send(0, 0, MPI_INT, 0, TAG_PWD_NOT_FOUND, inter);
					// fprintf(stderr, "[SLAVE %d]: NOT FOUND PWD, DONE!\n", rank);
					send1 = 1;
			        } 
                        }
                        else if(num_tasks_left < num_threads_slave && hasInterval){ // tasks not sufficient, ask for a new interval
				if(send == 0) {
					MPI_Send(0, 0, MPI_INT, 0, TAG_INTERVAL_REQUEST, inter);
					send = 1;
				}
                        }
                }
        }
}

/**
 * thread responsible for searching the password
 */
void thread_slave(){
	unsigned long long i;
	
	while(!stopSearching){
		Task* slice;

                #pragma omp critical
		{
			slice = nextTask();
		}
                
		if(slice != NULL){ // task available
			for(i=slice->start; i<=slice->end; i++){
				if(isFound || stopSearching){ // found by other thread of this process
					//fprintf(stderr, "[SLAVE %d]: PWD found by other thread or other slave processes, terminate thread self\n", rank);
					return;
                                }
				int len = getStrLen(i, alphabet_len);
				char* str_perm = malloc(len*sizeof(char));
				int2str(i, str_perm, alphabet, alphabet_len);
				if(strcmp(str_perm, pw) == 0){
					isFound = 1; // password find!
					pwd_found = i; 
					stopSearching = 1; // all threads stop searching
					free(str_perm);
					free(slice);
					return;
				}
				free(str_perm);
			}
			free(slice);
		}
		else if(!hasInterval) { // no slice available, so terminates
			++num_threads_finished;
			return;
		}
	}
}

int main(int argc, char** argv){
	
	/* initialize global variables */
	alphabet = malloc(MAX_ALPHABET_LENGTH * sizeof(char)); 
	num_threads_slave = atoi(argv[3]);
	pw = argv[5];

	isFound = 0;
	terminateComm = 0;
	stopSearching = 0;
	num_threads_finished = 0;
	num_tasks_left = 0;
	hasInterval = 1;
	listHeader = listTail = NULL;

	/* MPI Init */
	int provided;
	MPI_Comm inter;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
	MPI_Comm_get_parent(&inter);
	MPI_Comm_rank(inter, &rank);
	
	MPI_Bcast(&alphabet_len, 1, MPI_INT, 0, inter);
	MPI_Bcast(alphabet, alphabet_len, MPI_CHAR, 0, inter);

	/* parallel section */
	int id; 
	omp_set_num_threads(num_threads_slave+1);
        #pragma omp parallel private(id)
	{
		id = omp_get_thread_num();
		if(id == 0)
			thread_comm(inter);
		else
			thread_slave();
	}

	MPI_Finalize();
	free(alphabet);
	clearTaskList();
	// fprintf(stderr, "[SLAVE %d]: TERMINATED, DONE \n", rank);
	return EXIT_SUCCESS;
}
