#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "pvm3.h"
#include "intervalle.h"

void send_str(int tid, int tag, char* index){
  pvm_initsend(PvmDataDefault);
  pvm_pkstr(index);
  pvm_send(tid,tag);
}

void test_violent(char* str, int longeur, char* code, int parent){
  // mot de passe trouvé, 1; sinon, 0
  int i;
  char msg1[]="demand";
  char msg2[]="find";
  for(i=0; i<longeur; i++){
    if(i==longeur/2){
      send_str(parent,6, msg1);
      printf("demander nouvel intervalle\n");
    }
    char* essai= intervalle(str,i);
    if(strcmp(essai, code)==0){
      printf("-------------------------------------------------------the code is %s\n", essai);
      fflush(stdout);
      send_str(parent, 4, msg2);
      free(essai);
      break;
    }
    else{
      free(essai);
    }
  }
}

int main(int argc, char** argv){
  printf("COMMENCER!!!!\n");
  int mytid, parenttid;
  char code[100];
  int pas;// intervalle
  char test_str[100];
  mytid=pvm_mytid();
  parenttid=pvm_parent();
  pvm_recv(parenttid,1);//tag=1, recevoir le 1er intervalle
  pvm_upkstr(test_str);
  printf("I'm a child, my id is %d .Recv msg %s\n",mytid, test_str);
  pvm_recv(parenttid,2);//tag=2, recevoir la taille de l'intervalle
  pvm_upkint(&pas, 1,1);
  pvm_recv(parenttid,3);//tag=3, recevoir le mot de passe
  pvm_upkstr(code);

  //tester le premier intervalle
  test_violent(test_str, pas, code, parenttid);
  while(1){
    pvm_recv(parenttid,5); //tag=5, recevoir le nouvel intervalle
    pvm_upkstr(test_str);
    printf("Recv msg %s\n",test_str);
    test_violent(test_str, pas, code, parenttid);
  }
  pvm_exit();
  return 0;
}
