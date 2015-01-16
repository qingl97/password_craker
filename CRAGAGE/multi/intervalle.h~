#include<stdio.h>
#include<string.h>
#include<stdlib.h>
int pow(int a, int b){
  int ret=1;
  int i;
  for(i=0;i<b;i++){
    ret*=a;
  }
  return ret;
}
//convertir un string à un chiffre
int char2num(char* a){
  int length= strlen(a);
  int i;
  int* tmp=(int*) malloc(length*sizeof(int));
  for(i=0; i< length; i++){
    tmp[i] = a[i]-96;
  }
  int sum= 0;
  i=0;
  while(i<length){
    sum =sum*16+tmp[i];
    i++;
  }
  free(tmp);
  return sum;
}

//a = 1, o= 15
char* intervalle(unsigned char* a, int b){
  int length= strlen(a);
  int i;
  int* tmp=(int*) malloc(length*sizeof(int));
  for(i=0; i< length; i++){
    tmp[i] =a[i]-96;
  }
  int sum= 0;
  i=0;
  while(i<length){
    sum =sum*16+tmp[i];
    i++;
  }
  sum += b;

  int num[100];
  int index=0;
  int tmp1=sum/16;
  while(tmp1>=1){
    num[index]=sum%16;
    index++;
    sum = (sum-sum%16)/16;
    tmp1=sum/16;
  }
  num[index]= sum;

  for(i=0; i<=index;i++ ){
    num[i]+= 96;
  }
  char* new= (char*) malloc(sizeof(char)*(index+2));
  for(i=0; i< index+1; i++){
    new[i]= num[index-i];
  }
  new[index+1]='\0';
  free(tmp);
  return new;
}
