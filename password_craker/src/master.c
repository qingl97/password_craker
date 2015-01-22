#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include "mpi.h"

#include "wordHandler.h"
#include "mpi_utils.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* Commmon variables */
char* alphabet;
int alphabet_len;
int max_len_test;
char* pw;
int num_procs;

void usage(){
    printf("Run the program in this format:\n");
    printf("mpirun ./master [PathToFileContainsAlphabet] [num_procs] [num_slaveThreadsPerProc] [maxLengthToTest] [passwordToTest]\n");
}

int main(int argc, char** argv){
    if(argc != 6){
        usage();
        return EXIT_FAILURE;
    }
    
    /* Initialize variables */
    num_procs = atoi(argv[2]);
    max_len_test = atoi(argv[4]);
    pw = argv[5];
    alphabet = malloc(MAX_ALPHABET_LENGTH * sizeof(char)); 

    /* Initialize the alphabet array */
    FILE* file_alphabet;
    file_alphabet = fopen(argv[1], "r");
    if (file_alphabet == NULL) {
        fprintf(stderr, "Can't open alphabet file %s!\n", argv[2]);
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    int count = 0;
    while ((linelen = getline(&line, &linecap, file_alphabet)) > 0){
        alphabet[count] = *line;
        count++;
    }
    alphabet_len = count;
    fclose(file_alphabet);
    printf("alphabet_len:= %d\n", alphabet_len);
    printf("num_procs:= %d\n", num_procs);

    int stopComm = 0;
    int isFound = 0;
    int moreInterval = 1;
    unsigned long long num_AllPerms, start;
    unsigned long long val_pw;
    unsigned long long INTERVAL_LENGTH;
    int i;
    int numProcsFailed = 0;
    num_AllPerms = 0;
    for(i=1; i<= max_len_test; ++i)
            num_AllPerms += (unsigned long long)pow((double)alphabet_len, (double)i);
    start = 1;
    INTERVAL_LENGTH = num_AllPerms / (num_procs*2);
    printf("DEFAULT_INTERVAL_LENGTH:=%llu, num_procs_slave:= %d\n", INTERVAL_LENGTH, num_procs-1);
    if(INTERVAL_LENGTH < 1)
        INTERVAL_LENGTH = 1;
    
    printf("Total number of password permutations to test:= %llu\n", num_AllPerms);
    int tmp = getStrLen(456976, alphabet_len);
    char* str = malloc(sizeof(char) * tmp);
    int2str(456976, str, alphabet, alphabet_len);
    printf("PASSWORD to test [%s - %llu]\n", pw, str2int(pw, alphabet, alphabet_len));
    
    /* MPI Initialization */
    int rank;
    MPI_Comm inter;
    
    MPI_Init (&argc, &argv);    
    MPI_Comm_spawn("slave", argv+1, num_procs-1, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &inter, MPI_ERRCODES_IGNORE);
    MPI_Comm_rank (inter, &rank);
    
    /* Master sends the alphabet information to slaves */
    MPI_Bcast(&alphabet_len, 1, MPI_INT, MPI_ROOT, inter);
    MPI_Bcast(alphabet, alphabet_len, MPI_CHAR, MPI_ROOT, inter);
   
    
    MPI_Status status;
    MPI_Request req;
    MPI_Datatype mpi_type_interval;
    createMPIDatatype_Interval(&mpi_type_interval);
    
    /*--------------------- Master MAIN LOOP --------------------*/
    while(!stopComm){ 
            int flag;
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, inter, &flag, &status);
            if(flag){
                    switch(status.MPI_TAG){
                    case TAG_PASSWORD: // password found, message from slave
                            MPI_Recv(&val_pw, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_PASSWORD, inter, &status);
                            printf("[MASTER] Slave [%d] has found the PASSWORD [%llu]!!! \n", status.MPI_SOURCE, val_pw);
                            for(i=0; i<num_procs-1; i++){
                                    //if(i!=status.MPI_SOURCE)
                                            MPI_Send(0,0, MPI_INT, i, TAG_KILL_SELF, inter);
                            }
                            stopComm = 1; isFound = 1;
                            break;
                            
                    case TAG_PWD_NOT_FOUND: // tell the process to kill itself
                            MPI_Recv(0,0,MPI_INT, status.MPI_SOURCE, TAG_PWD_NOT_FOUND,inter, &status);
                            printf("[MASTER]: slave[%d] FAILED !!!\n", status.MPI_SOURCE);
                            ++numProcsFailed;
                            //MPI_Send(0,0, MPI_INT, status.MPI_SOURCE, TAG_KILL_SELF, inter);
                            break;
                            
                    case TAG_INTERVAL_REQUEST:
                            // printf("[MASTER]: slave [%d] is requesting interval.................\n", status.MPI_SOURCE);
                            MPI_Recv(0,0,MPI_INT, status.MPI_SOURCE, TAG_INTERVAL_REQUEST,inter,  &status);
                            if(moreInterval){
                                    Interval task;
                                    if(start+INTERVAL_LENGTH-1 > num_AllPerms){
                                            task.start = start;
                                            task.num_perms = num_AllPerms - start + 1;
                                            moreInterval = 0;
                                    }
                                    else{
                                            task.start = start;
                                            task.num_perms = INTERVAL_LENGTH;
                                            moreInterval = 1;
                                            start = start + INTERVAL_LENGTH;
                                    }
                                    MPI_Send(&task, 1, mpi_type_interval, status.MPI_SOURCE, TAG_INTERVAL, inter);
                                    //  printf("[MASTER]: Send interval start at [%llu] end at [%llu] to slave [%d]\n", task.start, task.start+task.num_perms-1, status.MPI_SOURCE);
                            }
                            else{
                                    //      printf("[MASTER]: respond to slave[%d] NO_MORE_INTERVAL\n", status.MPI_SOURCE);
                                    MPI_Send(0, 0, MPI_INT, status.MPI_SOURCE, TAG_NO_MORE_INTERVAL, inter);
                            }
                            break;
                    }
            }
            else{
                    if(numProcsFailed == num_procs-1){
                            stopComm = 1;
                            isFound = 0;
                    }
            }
    }
    
    if(isFound){
            int pw_length = getStrLen(val_pw, alphabet_len);
            char* str_pw = malloc(pw_length * sizeof(char));
            int2str(val_pw, str_pw, alphabet, alphabet_len);
            printf("\n************************************************************ \n");
            printf("<<< \x1b[32mSUCCESS \x1b[0m>>> Password is [%s], found by slave [%d]\n", str_pw, status.MPI_SOURCE);
            printf("************************************************************** \n");
            free(str_pw);
    }
    else{
            printf("\n************************************************************ \n");
            printf("<<< \x1b[31m FAILED\x1b[0m >>> vAll process don't find the right password! Program quit!!!\n");
            printf("************************************************************** \n");
    }  
    
    free(alphabet);
    //    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    printf("[MASTER]: exit program\n");
    return EXIT_SUCCESS;
}
