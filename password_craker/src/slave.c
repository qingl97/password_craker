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
sem_t computers;
int requested;

// allocate memory for every task in the list
void updateTaskList(Interval* itv){
	unsigned long long start, end;
	start = itv->start;
	end = itv->start + itv->num_perms - 1;
	while(start){
		++num_tasks_left;
		Task* slice = malloc(sizeof(Task));
		slice->start = start;
		if(start+TASK_SIZE-1 > end){ // the last slice
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
        //        fprintf(stderr, "[SLAVE %d]: slices UPDATED left to search = %llu\n", rank, num_tasks_left);
}	

Task* nextTask(){
	Task* ret;
	ret = listHeader;
	if(listHeader != NULL){
                Task* tmp = listHeader;
		listHeader = listHeader->next;
                free(tmp);
                --num_tasks_left;
        }
        else{
                //   fprintf(stderr, "[SLAVE %d]: NO MORE TASK_SLICE !!!\n", rank);
        }
        //  fprintf(stderr, "[SLAVE %d]: slices CONSUMED left to search = %llu\n", rank, num_tasks_left);
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
	int hasInterval = 1;
	//int responded, reqSent;
      	

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
                                // responded = 1;
                                // reqSent = 0;
                                MPI_Recv(&task, 1, mpi_type_interval, 0, TAG_INTERVAL, inter, &status);
                                //   fprintf(stderr, "[SLAVE %d] received interval [%llu - %llu] from Master ----------\n", rank, task.start, task.start+task.num_perms-1);
#pragma omp critical 
                                {
                                        updateTaskList(&task);
                                }
                                break;
                                
                        case TAG_NO_MORE_INTERVAL:
                                // fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Master said NO_MORE_INTERVAL~~~~~~~~~~~~~~~~~~~~~\n");
                                MPI_Recv(0,0,MPI_INT, 0, TAG_NO_MORE_INTERVAL,inter, &status);
                                //reqSent = 0;
                                // responded = 1;
                                hasInterval = 0;
                                break;
                                
                                // other process found password
                        case TAG_KILL_SELF:
                                MPI_Recv(0,0,MPI_INT, 0, TAG_KILL_SELF,inter, &status);
                                //  fprintf(stderr, "[SLAVE %d] Other process has found password______________\n", rank);
                                //isFound = 1;// by other process
                                stopSearching = 1;
                                terminateComm = 1;
                                break;
                        }
                }
                else{
                        if(isFound){ // one thread of this process found password
                                MPI_Send(&pwd_found, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_PASSWORD, inter);
                                //  fprintf(stderr, "[SLAVE %d] found PWD!!! Send to Master________________\n", rank);
                                //MPI_Send()
                                // terminateComm = 1;
                                //return;
                        }
                        else if(num_threads_finished == num_threads_slave){ // all threads didn't find the password
                                // fprintf(stderr, "[SLAVE %d] finished not finding _________________\n", rank);
                                MPI_Send(0, 0, MPI_INT, 0, TAG_PWD_NOT_FOUND, inter);
                                //for(k = 0; k<num_threads_slave; ++k)
				//	sem_post(&computers);
                                terminateComm = 1;
                                
                                //return;
                        }
                        else if(num_tasks_left < num_threads_slave && hasInterval){ // tasks not sufficient, ask for a new interval
                                // fprintf(stderr, "[SLAVE %d] requesting interval_________________\n", rank);
                                MPI_Send(0, 0, MPI_INT, 0, TAG_INTERVAL_REQUEST, inter);
                                //reqSent = 1;
                                //responded = 0;
                        }
                }
        }
}

// thread responsible for searching the password
void thread_slave(){
	unsigned long long i, start, end;
	
	while(!stopSearching){
		Task* slice;

		#pragma omp critical
		{
			slice = nextTask();
		}
                
		if(slice != NULL){ // task available
			for(i=slice->start; i<=slice->end; i++){
				if(isFound || stopSearching){ // found by other thread of this process
                                        //					fprintf(stderr, "[SLAVE %d]: PWD found by other thread or other slave\n", rank);
                                        return;
                                }
				int len = getStrLen(i, alphabet_len);
				char* str_perm = malloc(len*sizeof(char));
				int2str(i, str_perm, alphabet, alphabet_len);
                                //                                fprintf(stderr, "slave[%d] try [%s, %llu], length:= %lu\n", rank,str_perm, str2int(str_perm, alphabet, alphabet_len), strlen(str_perm));
				if(strcmp(str_perm, pw) == 0){
					fprintf(stderr, "[SLAVE %d] ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~PASSWORD FOUND~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", rank);
					isFound = 1; // password find!
					pwd_found = i; // write to this global variable, since only one password is possible, no need to protect its write
					free(str_perm);
					stopSearching = 1;
					return;
				}
				free(str_perm);
			}
		}
		else { // no slice available, means this thread doesn't find the password, so terminates
			++num_threads_finished;
			//stopSearching = 1;
                        //	fprintf(stderr, "[SLAVE %d] one thread finished\n", rank);
			return;
		}
	}
}



int main(int argc, char** argv){
    int provided;
    MPI_Comm inter;
    
    alphabet = malloc(MAX_ALPHABET_LENGTH * sizeof(char)); 
    num_threads_slave = atoi(argv[3]);
    pw = argv[5];

    isFound = 0;
    terminateComm = 0;
    stopSearching = 0;
    num_threads_finished = 0;
    num_tasks_left = 0;

    /* MPI Init */
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    // MPI_Init(&argc, &argv);
    MPI_Comm_get_parent(&inter);
    MPI_Comm_rank(inter, &rank);
    //fprintf(stderr, "[SLAVE %d] starts______________________\n", rank);
    
    MPI_Bcast(&alphabet_len, 1, MPI_INT, 0, inter);
    MPI_Bcast(alphabet, alphabet_len, MPI_CHAR, 0, inter);
   
    /* Initialize task list */
    listHeader = listTail = NULL;
    Interval task;
    MPI_Datatype mpi_type_interval;
    MPI_Status status;
    createMPIDatatype_Interval(&mpi_type_interval);
    MPI_Send(0, 0, MPI_INT, 0, TAG_INTERVAL_REQUEST, inter);
    MPI_Recv(&task, 1, mpi_type_interval, 0, TAG_INTERVAL, inter, &status);
    //    fprintf(stderr, "[SLAVE %d] received an interval [%llu - %llu] \n", rank, task.start, task.start+task.num_perms-1);
    updateTaskList(&task);

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
	fprintf(stderr, "[SLAVE %d]: exit************************ \n", rank);
	return EXIT_SUCCESS;
}
