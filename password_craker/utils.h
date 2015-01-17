#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

int getCharIndex(char* alphabet, char* chr);

int getStrLen(unsigned long long int n, int alphab_len);

unsigned long long int str2int(char* str, char* alphabet, int alphab_len);

void int2str(unsigned long long int n, char* chr, char* alphabet, int alphab_len);

#endif