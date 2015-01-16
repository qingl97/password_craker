#include<stdio.h>
#include"pvm3.h"
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include"intervalle.h"
#include<semaphore.h>


//tuer tous les esclaves
void kill_fils(int* tids, int nb_fils){
  int i;
  for(i=0; i<nb_fils; i++){
    pvm_kill(tids[i]);
  }
}
//envoyer un string
void send_str(int tid, int tag, char* index){
  pvm_initsend(PvmDataDefault);
  pvm_pkstr(index);
  pvm_send(tid,tag);
}
//envoyer un chiffre
void send_num(int tid, int tag, int num){
  pvm_initsend(PvmDataDefault);
  pvm_pkint(&num, 1,1);
  pvm_send(tid,tag);
}

int main(int argc, char** argv){
  int nlr_pros= atoi(argv[1]);
  int nb_r=atoi(argv[2]);
  char code[100];
  strcpy(code,argv[3]);
  int i;
  int filstids[1000];
  int parattid=pvm_mytid();
  char index[100]; //pour sauvegarder on est où
  strcpy(index, "a");//commencer par "a"
  char* fin=(char*)malloc(sizeof(char)*(nb_r+1)); // le string maximal
  for(i=0; i<nb_r; i++){
    fin[i]='o';
  }
  fin[nb_r]='\0';
  printf("intervalle is from a to %s\n",fin);

  int pas= pow( 2,nlr_pros+nb_r);
  //int pas=1000;
  printf("la taille de l'intervalle est %d\n", pas);

  pvm_catchout(stderr);
  int cc = pvm_spawn("/home/pg305-03/TPvendredi/CRAGAGE/non_multi/esclave", (char**)0, 0, "", nlr_pros, filstids);
  assert(cc == nlr_pros);
  
  for(i=0; i<nlr_pros; i++)
    {
      //distribuer à chaque esclave un intervalle
      send_str(filstids[i], 1, index);
      //calculer l'intervalle suivant
      char* str=intervalle(index,pas);
      strcpy(index, str);
      free(str);
      //envoyer la taille de l'intervalle
      send_num(filstids[i], 2, pas);
      //envoyer le mot de passe aux esclaves      
      send_str(filstids[i],3, code);
    }
  
  int bufid,bytes,type, source, info;
  int lasttour=0; // si tous les intervalles sont distribués
  char* str1;
  sem_t sem;
  char msg[20];
  char msg1[]="demand";
  char msg2[]="find";
  sem_init(&sem, 1,1);

    while(1){

      bufid = pvm_recv( -1, -1 ); // recevoir une demande quand un esclave calcul au milieu de l'intervalle
      pvm_upkstr(msg);      
      info = pvm_bufinfo( bufid, &bytes, &type, &source ); 
      sem_wait(&sem);
      if(strcmp(msg, msg1)==0){
	str1=intervalle(index, pas);
	send_str(source, 5, index);
	strcpy(index, str1);
	free(str1);
      }
      else if(strcmp(msg, msg2)==0){
	printf("mot de passe trouvé!\n");
	kill_fils(filstids, nlr_pros);
	break;	
      }

      if(lasttour){
        printf("mot de passe non trouvé...\n");
	break;
      }

      sem_post(&sem);

      // si l'index de string a dépassé la possibilité maximale, on reçoit la dernière fois les résultats
      if(strlen(index)>strlen(fin))
	{
	  lasttour=1;
	}
      else if(strlen(index)==strlen(fin)&&strcmp(index, fin)>0){
	lasttour=1;
      }
    }
    kill_fils(filstids, nlr_pros);
    free(fin);
    sem_destroy(&sem);
    pvm_exit();
    return 0;
}
