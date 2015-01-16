#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include "mpi.h"

#define MAX_ALPHABET          128
#define TAG_PASSWORD          0
#define TAG_PASSWORD_FOUND    1
#define TAG_START_PERMUTATION 2
#define TAG_NUM_PERMUTATIONS  3
#define TAG_INTERVAL          4

typedef struct{
  char* str;
  int len;
}Permutation;

typedef struct{
  unsigned long long int start;
  unsigned long long int end;
}Interval;

void usage(){
  printf("Run the program in this format:\n");
  printf("./craker num_threadsPerProc PathToFileContainsAlphabet maxLengthToTest password\n");
}

void int2Permutation(unsigned long long int n, char* chr, char* alpha, int alpha_len){
  int i = 0;
  unsigned long long int rem, tmp;
  i = 0;
  while(n!=0){
    rem = n%alpha_len;
    if(rem)
      chr[i] = alpha[rem-1];
    else
      chr[i] = alpha[alpha_len-1];
    ++i;
    n /= alpha_len;
  }
  chr[i] = '\0';
}

// void permutationIncrementOne(Permutation* perm, char* alpha, int alpha_len){
//   int i, more = 0;
//   for(i=perm->len-1; i>=0; i--){
//     if(perm->str[i] == alpha[alpha_len-1]){
//       char* buf;
//       strncpy(buf, perm->str, perm->len-1);
//       strcat(buf, &alpha[getCharIndex(alpha, alpha_len, &perm->str[perm->len-1])+1]);
//       more = 0;
//       break;
//     }
//     else{
//       perm->str[i] = alpha[0];
//       if(i == 0)  more = 1;
//     }
//   }

//   if(more){
//     free(perm->str);
//     perm->len++;
//     perm->str = malloc(perm->len * sizeof(char));
//     //TODO
//   }
// }

// index of each char in the alphabet starts from 1 to the N
// N is the lenth of the alphabet
int getCharIndex(char* alphabet, int alpha_len, char* chr){
  int i = 0;
  while(alphabet[i] != *chr)
    i++;
  return i+1;
}

// get the corresponding value of the permutaiton in a numeral system with a radix of alpha_len
unsigned long long int permutation2int(Permutation* perm, char* alphabet, int alpha_len){
  int i = perm->len;
  unsigned long long int sum = 0;
  while(i>0){
    sum += getCharIndex(alphabet, alpha_len, &perm->str[i-1]) * 
      (unsigned long long int)pow((double)alpha_len, (double)(i-1));
    --i;
  }
  return sum;
}

int main(int argc, char** argv){
  int num_procs;
  int num_threadsPerProc;
  char* alphabet;
  int alphabet_len;
  int max_len_test;
  int pw_len;
  char* pw;
  Permutation* perm_pw;

  if(argc != 5){
    usage();
    return EXIT_FAILURE;
  }
  
  // initialize parameters
  num_threadsPerProc = atoi(argv[1]);
  max_len_test = atoi(argv[3]);
  pw = argv[4]; /* memory is allocated already! share by all the processus */
  pw_len = strlen(pw);
  alphabet = (char*)malloc(MAX_ALPHABET * sizeof(char)); 
  perm_pw = (Permutation*)malloc(sizeof(Permutation));
  perm_pw->str = pw;
  perm_pw->len = pw_len;

  int rank;
  MPI_Init (&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Comm_size (MPI_COMM_WORLD, &num_procs);
  
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

  /* Main processing part */
  // All permutations correspond to an integer value in a numeral system with radix of the length
  // of the alphabet list. 
  // The Once the characters in the alphabet list is fixed, the order of characters in the list is 
  // the fixed, as the order in the file alphabet.list by default.

  unsigned long long int val_pw;      // value of the password

  val_pw = permutation2int(perm_pw, alphabet, alphabet_len);

  if(rank == 0){ // master
    printf("password length:= %d\n", pw_len);
    printf("%llu\n", val_pw);

    int find = 0;
    int num_permsPerItval = 100;    // number of permuations distributed to each slave process

    unsigned long long int num_AllPerms, start, num_perms;
  
    num_AllPerms = (unsigned long long int)pow((double)alphabet_len, (double)max_len_test);
    start = 1;
    num_perms = num_permsPerItval;

    MPI_Status status;
    MPI_Request req;
    int flag = 0;
    int isComplete = 0;
    // if not find, continue working

    printf("Total number of perms to try:= %llu\n", num_AllPerms);
    //while(1){
      MPI_Irecv(&find, 1, MPI_INT, MPI_ANY_SOURCE, TAG_PASSWORD_FOUND, MPI_COMM_WORLD, &req);
      if(find)
        MPI_Abort(MPI_COMM_WORLD, 2); 
      else{ // not find yet, continue respond to slave's request for intervals
        //while(!flag){
          MPI_Iprobe(MPI_ANY_SOURCE, TAG_INTERVAL, MPI_COMM_WORLD, &flag, &status);
        
        //}
          while(flag){
            if(start + num_permsPerItval > num_AllPerms){
              num_perms = num_AllPerms - start + 1;
              isComplete = 1;
            }
            MPI_Send(&start, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_START_PERMUTATION, MPI_COMM_WORLD);
            MPI_Send(&num_perms, 1, MPI_UNSIGNED_LONG_LONG, status.MPI_SOURCE, TAG_NUM_PERMUTATIONS, MPI_COMM_WORLD);
            if(!isComplete)
              start += num_perms;
            else
              break;
          }
        // int i;
        // for(i=1; i<num_procs; i++){
        //   if(start + num_permsPerItval > num_AllPerms){
        //     num_perms = num_AllPerms - start + 1;
        //     isComplete = 1;
        //   }
        //   MPI_Send(&start, 1, MPI_UNSIGNED_LONG_LONG, i, START_PERMUTATION, MPI_COMM_WORLD);
        //   MPI_Send(&num_perms, 1, MPI_UNSIGNED_LONG_LONG, i, NUM_PERMUTATIONS, MPI_COMM_WORLD);
        //   MPI_Iprobe(...)
        //   if()// Tester que la communication est arrive
        //     MPI_Recv(...)// Reception d'une demande de travail
        //     MPI_Send(...)// Envoie d'un interval si possible
        //   // should update the start and end
        //   if(!isComplete)
        //     start += num_perms;
        //   else
        //     break;
        }
      //}
    }
  
  else{ // slave
    printf("begin slave\n");
    unsigned long long int start, num_perms;
    int wantInterval = 1;
    MPI_Status status;
    MPI_Request req;
    int find = 0;

    while(!find){   /* Main loop */
      // request interval from master
      MPI_Send(&wantInterval, 1, MPI_INT, 0, TAG_INTERVAL, MPI_COMM_WORLD);
      MPI_Recv(&start, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_START_PERMUTATION, MPI_COMM_WORLD, &status);
      MPI_Recv(&num_perms, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_NUM_PERMUTATIONS, MPI_COMM_WORLD, &status);
      printf("[slave %d] received start:=[%llu]\tnum_perms:=[%llu]\n", rank, start, num_perms);

      /* Search in the interval of start to end */
      //#pragma omp parallel 
      unsigned long long int i, tmp;

      for(i=start; i<=start+num_perms; i++){
        int j = 0;
        tmp = i;
        while(i!=0){
          j++;  i /= alphabet_len;
        }
        i = tmp;
        char str_perm[j+1];
        int2Permutation(i, str_perm, alphabet, alphabet_len);
        printf("----------------------------------------------try [%s], length:= %lu\n", str_perm, strlen(str_perm));

        if(strcmp(str_perm, pw) == 0){
          find = 1; // password find!
          printf("*****************  SUCCESS!!! password is %s\n", str_perm);
          MPI_Send(&i, 1, MPI_UNSIGNED_LONG_LONG, 0, TAG_PASSWORD, MPI_COMM_WORLD); // send pw
          MPI_Isend(&find, 1, MPI_INT, 0, TAG_PASSWORD_FOUND, MPI_COMM_WORLD, &req); // send signal
          break;
        }
      }
    }
  }

  free(perm_pw);
  free(alphabet);
  
  MPI_Finalize();

  return EXIT_SUCCESS;
}
