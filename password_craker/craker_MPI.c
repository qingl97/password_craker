#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include "mpi.h"
//#include "utils.h"

#define MAX_ALPHABET_LENGTH   128
//#define INTERVAL_LENGTH		  100000
#define TAG_PASSWORD          0
#define TAG_START_PERMUTATION 2
#define TAG_NUM_PERMUTATIONS  3
#define TAG_INTERVAL_REQUEST  4
#define TAG_NO_MORE_INTERVAL  5
#define TAG_PASSWORD_NOT_FOUND		  6

int getCharIndex(char* alphabet, char* chr){
  	int i = 0;
  	while(alphabet[i] != *chr)
    	i++;
  	return i+1;
}

int getStrLen(unsigned long long n, int alphab_len){
  	int i = 0, rem;
  	while(n>0){
    	++i;
    	rem = n%alphab_len;
    	if(rem == 0)
    		rem = alphab_len;
    	n = (n - rem)/alphab_len;
  	}
  	return i+1; // append '0' to the end of string
}

unsigned long long str2int(char* str, char* alphabet, int alphab_len){
  	int i = strlen(str);
  	int tmp = i;
  	unsigned long long sum = 0;
  	while(i>0){
    	sum += getCharIndex(alphabet, &str[i-1]) * 
      			(unsigned long long)pow((double)alphab_len, (double)(tmp-i));
    	--i;
  	}
  	return sum;
}

void int2str(unsigned long long n, char* chr, char* alphabet, int alphab_len){
	int len = getStrLen(n, alphab_len);
	int rem;
	int i=len-2;;
	while(i>=0){
		rem = n%alphab_len;
    	chr[i] = alphabet[(rem+alphab_len-1)%alphab_len];
    	if(rem == 0)
    		rem = alphab_len;
    	n = (n - rem)/alphab_len;
    	i--;
	}
}

void usage(){
  	printf("Run the program in this format:\n");
  	printf("mpirun -np numberOfProcs ./craker num_threadsPerProc PathToFileContainsAlphabet maxLengthToTest password\n");
}

int main(int argc, char** argv){
	if(argc != 5){
		usage();
		return EXIT_FAILURE;
	}

	/*----------------- Commmon variables ----------------*/
	char* alphabet;
	int alphabet_len;
	int max_len_test;
	char* pw;
	int num_threadsPerProc;

	/*--------------- Initialize variables --------------*/
	num_threadsPerProc = atoi(argv[1]);
	max_len_test = atoi(argv[3]);
	pw = argv[4]; /* memory is allocated already! share by all the processus */
	alphabet = (char*)malloc(MAX_ALPHABET_LENGTH * sizeof(char)); 

	int rank;
	int num_procs;
	MPI_Init (&argc, &argv);
	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	MPI_Comm_size (MPI_COMM_WORLD, &num_procs);

	/*------------------- Master process ----------------*/
	if(rank == 0){
		/* read from alphabet file then initialize the alphabet array */
		FILE* file_alphabet;
		file_alphabet = fopen(argv[2], "r");
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
	}

	MPI_Bcast(&alphabet_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(alphabet, alphabet_len, MPI_CHAR, 0, MPI_COMM_WORLD);

	/*------------------- Master Working ----------------*/
	// All permutations correspond to an integer value in a numeral system with radix of the length
	// of the alphabet list. 
	// The Once the characters in the alphabet list is fixed, the order of characters in the list is 
	// the fixed, as the order in the file alphabet.list by default.

	if(rank == 0){           
		printf("alphabet_len:= %d\n", alphabet_len);

		MPI_Status status;
		MPI_Request req;
		int flag_pw = 0;
		int flag = 0;
		int flag_end = 0;
		int isComplete = 0;
		int moreInterval = 1;
		unsigned long long num_AllPerms, start, num_perms;
		int count = 0;
		unsigned long long INTERVAL_LENGTH;

		num_AllPerms = (unsigned long long)pow((double)alphabet_len, (double)max_len_test);
		start = 1;
		num_perms = INTERVAL_LENGTH = num_AllPerms / (num_procs*2);
		printf("Total number of password permutations to test:= %llu\n", num_AllPerms);
		printf("num_perms := %llu\n", num_perms);

		/*                    MAIN LOOP                     */
		while(!isComplete){

		  	/**
		  	 *	non-blocking check if the message of TAG_PASSWORD has arrived or not
		  	 * 	if arrived, means the password is found succussfully! 
		  	 * 	Then master terminate all slaves
		  	 **/
			MPI_Iprobe(MPI_ANY_SOURCE, TAG_PASSWORD, MPI_COMM_WORLD, &flag_pw, &status);
			if(flag_pw){
				unsigned long long val_pw, tmp;
				MPI_Recv(&val_pw, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_PASSWORD, MPI_COMM_WORLD, &status);

				int pw_length = getStrLen(val_pw, alphabet_len);
				char* str_pw = malloc(pw_length * sizeof(char));
				int2str(val_pw, str_pw, alphabet, alphabet_len);
				printf("\n************************  SUCCESS   ************************ \n");
				printf("Password is [%s], found by slave [%d]\n", str_pw, status.MPI_SOURCE);
				printf("**************************  SUCCESS   ************************ \n");
				isComplete = 1;
				free(str_pw);
				MPI_Abort(MPI_COMM_WORLD, 1); 
			}

			

			// respond to slaves' requests for intervals
			MPI_Iprobe(MPI_ANY_SOURCE, TAG_INTERVAL_REQUEST, MPI_COMM_WORLD, &flag, &status);

			if(flag){
				if(moreInterval){
					if(start + INTERVAL_LENGTH > num_AllPerms){
		  				num_perms = num_AllPerms - start + 1;
		  				moreInterval = 0;
					}
					MPI_Send(&start, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_START_PERMUTATION, MPI_COMM_WORLD);
					MPI_Send(&num_perms, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_NUM_PERMUTATIONS, MPI_COMM_WORLD);
					//printf("Master send interval begins at [%llu], ends at [%llu] to slave [%d]\n", start, start+num_perms-1, status.MPI_SOURCE);
					// update the start point for the next interval, if no more intervals, then do nothing
					if(moreInterval)
		  				start += num_perms; 
				}
				else{
					//printf("Master respond to slave %d, NO MORE INTERVAL !!! \n", status.MPI_SOURCE);
					MPI_Send(&moreInterval, 1, MPI_INT, status.MPI_SOURCE, TAG_NO_MORE_INTERVAL, MPI_COMM_WORLD);
					//MPI_Abort(MPI_COMM_WORLD, 2); 
				}
			}

			/**
			 * check if some slave had stopped without finding the password
			 */
			MPI_Iprobe(MPI_ANY_SOURCE, TAG_PASSWORD_NOT_FOUND, MPI_COMM_WORLD, &flag_end, &status);
			if(flag_end){
				count++;
				if(count == num_procs-1){
					// password not found!
					printf("PASSWORD NOT FOUND !!! :( \n");
					//MPI_Abort(MPI_COMM_WORLD, 0); 
						MPI_Finalize();
					  		exit(EXIT_SUCCESS);
				}
			}
		}
	}

	if(rank != 0){ // slave
		printf("Slave [%d] starts \n", rank);
		unsigned long long start, num_perms;
		int wantInterval = 1;
		MPI_Status status;
		MPI_Request req;
		int isDone = 0;
		int moreInterval = 1;
		int flag = 0;
		int flag_no_interval = 0;
		//unsigned long long count = 0;				/* for recording try times by this slave */

		while(!isDone){   /* Slave loop */
		  	// request interval from master
				MPI_Send(&wantInterval, 1, MPI_INT, 0, TAG_INTERVAL_REQUEST, MPI_COMM_WORLD);
				//printf("slave [%d] request for interval\n", rank);
				MPI_Iprobe(0, TAG_NO_MORE_INTERVAL, MPI_COMM_WORLD, &flag_no_interval, &status);
				//MPI_Wait(&req, &status);
				if(!flag_no_interval){
					//MPI_Iprobe(0, TAG_NUM_PERMUTATIONS, MPI_COMM_WORLD, &flag, &status);
					//if(flag){ // new interval arrived
					MPI_Recv(&start, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_START_PERMUTATION, MPI_COMM_WORLD, &status);
					MPI_Recv(&num_perms, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_NUM_PERMUTATIONS, MPI_COMM_WORLD, &status);
					//printf("[slave %d] received start:=[%llu]\tnum_perms:=[%llu]\n", rank, start, num_perms);

					unsigned long long i;
					for(i=start; i<start+num_perms; i++){
						//count++;
						int len = getStrLen(i, alphabet_len);
						char* str_perm = malloc(len*sizeof(char));
						int2str(i, str_perm, alphabet, alphabet_len);
						//printf("slave[%d] try [%s, %llu], length:= %d\n", rank,str_perm, i, len-1);

						if(strcmp(str_perm, pw) == 0){
					  		//isDone = 1; // password find!
					  		free(str_perm);
					  		printf("\nSlave [%d] has found the PASSWORD !!! Send to Master!\n", rank);
					  		MPI_Send(&i, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_PASSWORD, MPI_COMM_WORLD); // send pw
					  		//MPI_Abort(MPI_COMM_WORLD, 3); 
					  		//break;
					  		MPI_Finalize();
					  		exit(EXIT_SUCCESS);
						}
						free(str_perm);
					}
				}
				else{
					/** 
					  *	no more interval, this means this slave didn't find the password in its previous work
					  *	then send count to master
					**/
					printf("\nslave[%d] requested for interval, but master respond no more interval to distribute!\n", rank);
					//isDone = 1;
					MPI_Send(0, 0, MPI_INT, 0, TAG_PASSWORD_NOT_FOUND, MPI_COMM_WORLD);
					MPI_Finalize();
					exit(EXIT_SUCCESS);
				}
			//}	
		}
		printf("Slave[%d] quit !!!!!!!!!!!!!\n", rank);
	}

	free(alphabet);

	MPI_Finalize();

	return EXIT_SUCCESS;
}