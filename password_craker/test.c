#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils.h"

#define alphab_len	16
#define MAX_ALPHABET 128

char* alphabet;
int alphabet_len;

int main(){

  	alphabet = (char*)malloc(MAX_ALPHABET * sizeof(char)); 

	FILE* file_alphabet;
    file_alphabet = fopen("letters", "r");
    if (file_alphabet == NULL) {
      fprintf(stderr, "Can't open alphabet file!\n");
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

    unsigned long long int i;
    for(i=1;i<10000000;i++){
    	int length = getStrLen(i, alphabet_len);
    	char* str = malloc(length*sizeof(char));
    	int2str(i, str, alphabet, alphabet_len);
    	unsigned long long int dst = str2int(str, alphabet, alphabet_len);
    	free(str);
    	if(i != dst){
    		printf("strLenght = %d, str = %s\n", length, str);
    		printf("i = %llu, dst = %llu\n", i, dst);
    		exit(EXIT_FAILURE);
    	}
    		
    }
    printf("DONE\n" );

    free(alphabet);
    return 0;
}
