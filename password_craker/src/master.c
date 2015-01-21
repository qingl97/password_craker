#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include "mpi.h"

#include "wordHandler.h"
#include "mpi_utils.h"

/* Commmon variables */
char* alphabet;
int alphabet_len;
int max_len_test;
char* pw;

void usage(){
    printf("Run the program in this format:\n");
    printf("mpirun ./master [PathToFileContainsAlphabet] [num_slaveThreadsPerProc] [maxLengthToTest] [passwordToTest]\n");
}

int main(int argc, char** argv){
    if(argc != 5){
        usage();
        return EXIT_FAILURE;
    }

    /* Initialize variables */
    max_len_test = atoi(argv[3]);
    pw = argv[4];
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

    /* MPI Initialization */
    int rank;
    int num_procs;
    MPI_Comm inter;

    MPI_Init (&argc, &argv);
    /* Create num_procs-1 slaves */
    MPI_Comm_spawn("slave", argv+1, num_procs-1, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &inter, MPI_ERRCODES_IGNORE);
    MPI_Comm_rank (inter, &rank);
    //MPI_Comm_size (inter, &num_procs);

    /* Master sends the alphabet information to slaves */
    MPI_Bcast(&alphabet_len, 1, MPI_INT, 0, inter);
    MPI_Bcast(alphabet, alphabet_len, MPI_CHAR, 0, inter);

    /*--------------------------------------- Master Working ---------------------------------------
     * All permutations correspond to an integer value in a numeral system with radix of the length
     * of the alphabet list. 
     * The Once the characters in the alphabet list is fixed, the order of characters in the list is 
     * the fixed, as the order in the file alphabet.list by default.
     *--------------------------------------------------------------------------------------------*/ 
    MPI_Status status;
    MPI_Request req;
    int flag = 0;
    int isOver = 0;
    int isFound = 0;
    int moreInterval = 1;
    unsigned long long num_AllPerms, start, num_perms;
    unsigned long long val_pw;
    int recorder[num_procs-1];
    unsigned long long INTERVAL_LENGTH;
    int i;
    int numProcsFailed = 0;
    for(i=0; i<num_procs-1; i++) recorder[i] = 0;

    num_AllPerms = (unsigned long long)pow((double)alphabet_len, (double)max_len_test);
    start = 1;
    INTERVAL_LENGTH = num_AllPerms / (num_procs*2);
    if(INTERVAL_LENGTH < 1)
        INTERVAL_LENGTH = 1;
    num_perms = INTERVAL_LENGTH;
    printf("Total number of password permutations to test:= %llu\n", num_AllPerms);
    printf("num_perms := %llu\n", num_perms);
    printf("the real PASSWORD is [%s]\n", pw);

    MPI_Datatype mpi_type_interval;
    createMPIDatatype_Interval(&mpi_type_interval);
    
    /*--------------------- Master MAIN LOOP --------------------*/
    while(!isOver){   
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, inter, &flag, &status);
        if(flag){
            switch(status.MPI_TAG){
                case TAG_PASSWORD: // password found, message from slave
                MPI_Recv(&val_pw, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_PASSWORD, inter, &status);
                printf("\nSlave [%d] has found the PASSWORD [%llu]!!! Send to Master!\n", status.MPI_SOURCE, val_pw);
                for(i=1; i<num_procs; i++){
                    MPI_Send(0,0, MPI_INT, i, TAG_KILL_SELF, inter);
                    isOver = 1; isFound = 1;
                }
                break;

                case TAG_PWD_NOT_FOUND: // tell the process to kill itself
                ++numProcsFailed;
                MPI_Send(0,0, MPI_INT, status.MPI_SOURCE, TAG_KILL_SELF, inter);
                break;

                case TAG_INTERVAL_REQUEST:
                printf("slave [%d] is requesting interval.................\n", status.MPI_SOURCE);
                if(moreInterval){
                    Interval task;
                    task.start = start;
                    task.num_perms = num_perms;
                    //MPI_Send(&moreInterval, 1, MPI_INT, status.MPI_SOURCE, TAG_INTERVAL_REQUEST, MPI_COMM_WORLD);
                    MPI_Send(&task, 1, mpi_type_interval, status.MPI_SOURCE, TAG_INTERVAL, inter);
                    printf("Master send interval start at [%llu] end at [%llu] to slave [%d]\n", start, start+num_perms-1, status.MPI_SOURCE);
                    /* Update start index and number of permutations to try for next request */
                    if(start + num_perms - 1 < num_AllPerms){
                        start += num_perms; 
                        if(start + INTERVAL_LENGTH > num_AllPerms)
                            num_perms = num_AllPerms - start + 1;
                    }
                    else
                        moreInterval = 0;
                }
                else{
                    MPI_Send(0, 0, MPI_INT, status.MPI_SOURCE, TAG_NO_MORE_INTERVAL, inter);
                }
                break;
                
            }
        }
        else{
            if(numProcsFailed == num_procs-1){
                isOver = 1;
                isFound = 0;
            }
        }
    }

    if(isFound){
        int pw_length = getStrLen(val_pw, alphabet_len);
        char* str_pw = malloc(pw_length * sizeof(char));
        int2str(val_pw, str_pw, alphabet, alphabet_len);
        printf("\n************************  SUCCESS   ************************ \n");
        printf("Password is [%s], found by slave [%d]\n", str_pw, status.MPI_SOURCE);
        printf("**************************  SUCCESS   ************************ \n");
        free(str_pw);
    }
    else{
        printf("\n**************************  FAILED   ************************ \n");
        printf("All process don't find the right password! Program quit!!!\n");
        printf("**************************  FAILED   ************************ \n");
    }  

    free(alphabet);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return EXIT_SUCCESS;
}