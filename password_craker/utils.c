#include "utils.h"

int getCharIndex(char* alphabet, char* chr){
  	int i = 0;
  	while(alphabet[i] != *chr)
    	i++;
  	return i+1;
}

int getStrLen(unsigned long long int n, int alphab_len){
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

unsigned long long int str2int(char* str, char* alphabet, int alphab_len){
  	int i = strlen(str);
  	int tmp = i;
  	unsigned long long int sum = 0;
  	while(i>0){
    	sum += getCharIndex(alphabet, &str[i-1]) * 
      			(unsigned long long int)pow((double)alphab_len, (double)(tmp-i));
    	--i;
  	}
  	return sum;
}

void int2str(unsigned long long int n, char* chr, char* alphabet, int alphab_len){
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