#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <semaphore.h>
#include "mpi.h"

#include "wordHandler.h"
#include "mpi_utils.h"

#define TASK_SIZE	100

typedef struct {
	unsigned long long start;
	unsigned long long end;
	Task* next;
} Task;

char* alphabet;
int alphabet_len;
int num_threads_slave;
int num_threads_finished;
unsigned long long num_perms_per_thead;
unsigned long long pwd_found;
Task* listHeader, *listTail;
unsigned long long num_tasks;
int isFound, terminate;
int k;
// sem_t computers;

// allocate memory for every task in the list
void updateTaskList(Interval* itv){
	unsigned long long start, end;
	start = itv->start;
	end = itv->start + itv->num_perms - 1;
	while(start){
		++num_tasks;
		Task* slice = malloc(sizeof(Task));
		slice->start = start;
		if(start+TASK_SIZE+1 > end){ // the last slice
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
	if(listHeader != NULL)
		listHeader = listHeader->next;
	return ret;
}

void clearTaskList(){
	while(listHeader != NULL){
		Task* tmp = listHeader;
		listHeader = listHeader->next;
		free(tmp);
	}
}

// thread responsible for communicating with the slave threads and the parent process
void thread_comm(MPI_Comm inter){
	int isOver = 0;
	int hasInterval = 1;
	int responded, reqSent;
	listHeader = listTail = NULL;
	MPI_Datatype mpi_type_interval;
    createMPIDatatype_Interval(&mpi_type_interval);

    MPI_Status status;
    MPI_Request req;
    // begin by sending a request for interval
    MPI_Send(0, 0, MPI_INT, 0, TAG_INTERVAL_REQUEST, inter);
    reqSent = 1;
    responded = 0;

	while(!isOver){
		Interval task;
        MPI_Iprobe(0, MPI_ANY_TAG, inter, &flag, &status);
        if(flag){
        	switch(status.MPI_TAG){
        		case TAG_INTERVAL: // an interval arrived
        		responded = 1;
        		reqSent = 0;
                MPI_Recv(&task, 1, mpi_type_interval, 0, TAG_INTERVAL, inter, &status);
            	#pragma omp critical 
            	{
            		updateTaskList(&task);
            		//sem_post(&computers);
            	}
                break;

                case TAG_NO_MORE_INTERVAL:
                reqSent = 0;
                responded = 1;
                hasInterval = 0;
                break;

                // only when pwd is found or // all tasks finished without finding pwd
                case TAG_KILL_SELF:
                isOver = 1;
                terminate = 1;
                break;
        	}
        }
        else{
        	if(isFound){ // password found
        		MPI_Send(&pwd_found, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_PASSWORD, inter);

        		//return;
        	}
        	else if(num_threads_finished == num_threads_slave){ // all threads didn't find the password
        		MPI_Send(0, 0, MPI_INT, 0, TAG_PWD_NOT_FOUND, inter);
        		//for(k = 0; k<num_threads_slave; ++k)
				//	sem_post(&computers);
        		//return;
        	}
        	else if(num_tasks < num_threads_slave && hasInterval && responded == 0 && reqSent == 0){ // tasks not sufficient, ask for a new interval
        		MPI_Send(0, 0, MPI_INT, 0, TAG_INTERVAL_REQUEST, inter);
        		reqSent = 1;
        	}
        }
	}
}

// thread responsible for searching the password
void thread_slave(){
	unsigned long long i, start, end;

	while(1){
		Task* slice;
		// sem_wait(&computers);
		#pragma omp critical
		{
			slice = nextTask();
		}

		if(slice != NULL){ // task available
			for(i=slice->start; i<=slice->end; i++){
				if(terminate)
					return;
                int len = getStrLen(i, alphabet_len);
                char* str_perm = malloc(len*sizeof(char));
                int2str(i, str_perm, alphabet, alphabet_len);
                //printf("slave[%d] try [%s, %llu], length:= %lu\n", rank,str_perm, str2int(str_perm, alphabet, alphabet_len), strlen(str_perm));
                if(strcmp(str_perm, pw) == 0){
                    isFound = 1; // password find!
                    pwd_found = i; // write to this global variable, since only one password is possible, no need to protect its write
                    free(str_perm);
                    return;
                }
                free(str_perm);
            }
		}
		else{ // no slice available, means this thread doesn't find the password, so terminates
			++num_threads_finished;
			return;
		}	
	}
}

int main(int argc, char** argv){

	alphabet = malloc(MAX_ALPHABET_LENGTH * sizeof(char)); 
	num_threads_slave = argv[1]
	isFound = 0;
	terminate = 0;
	num_threads_finished = 0;

	/* MPI Initialization */
    int rank;
    int num_procs;
    MPI_Comm inter;

    MPI_Init (&argc, &argv);
    MPI_Comm_get_parent(&inter); // get intercommunicator
    MPI_Comm_rank (inter, &rank);
    MPI_Comm_size (inter, &num_procs);
    
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

	//sem_destroy(&computers);
	MPI_Finalize();
	free(alphabet);
	clearTaskList();
	return EXIT_SUCCESS;
}