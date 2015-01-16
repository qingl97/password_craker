#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "pvm3.h"
#include "intervalle.h"
#include<pthread.h>
#include<semaphore.h>

char msg1[]="demand";
char msg2[]="find";

int parenttid;

void send_str(int tid, int tag, char* index){
  sem_t sem_send;
  sem_init(&sem_send,1,1);
  sem_wait(&sem_send);
  pvm_initsend(PvmDataDefault);
  pvm_pkstr(index);
  pvm_send(tid,tag);
  sem_post(&sem_send);
}

typedef struct Args{
  char code[100];
  char index[100];
  int pas;
} Para;

int find=1;
int not_find=0;

/*chaque thread lance cette fonction pour test son propre intervalle
  Une fois ce thread a trouvé le mot de passe, le thread retourne 1;
  sinon, il retourne 0
 */
void* test( void* arg){
  void* ret;
  printf("lancer un thread\n");
  Para* para= arg;
  int i;
  char code[100];
  strcpy(code, para->code);
  char index[100];
  strcpy(index, para->index);
  int pas=para->pas;
  for(i=0;i<pas;i++){
    char* test= intervalle(index, i);
    if(strcmp(code, test)==0){
      ret=&find;
      printf("-------------------------------code found!!!!!!!!!!!!!!\n");
      printf("-------------------------------mot de passe est %s \n", test);
      fflush(stdout);
      free(test);
      break;
    }
    ret=&not_find;
    free(test);
  }
  free(para);
  pthread_exit(ret);
}


void lancer_test(char* index, char* code, int pas, int pas_thread, int nb_thread , int parent){
  sem_t sem;
  sem_init(&sem,1,1);
  Para* args; 
  int cur_index=0;// l'index de l'intervalle distribué, pour savoir on est où
  int i;
  pthread_t fils_th[nb_thread];
  for(i=0; i<nb_thread; i++){
    sem_wait(&sem);
    args=malloc(sizeof(struct Args));
    strcpy(args->index, index);
    strcpy(args->code, code);
    args->pas=pas_thread;
    printf("intervalle commence %s\n", index);
    pthread_create(&fils_th[i],NULL, test,(void*) args);
    cur_index+=pas_thread;
    if(cur_index==pas/2){                  //si on est au milieu de l'intervalle, envoyer au maitre une demande
      send_str(parent,6, msg1);
    }
    char* tmp=intervalle(index, pas_thread);
    strcpy(index, tmp);
    free(tmp);
    sem_post(&sem);
  }
  void* res;
  while(cur_index<=pas){
    
    for(i=0; i<nb_thread; i++){
      pthread_join(fils_th[i],&res);
      int* x;
      x=(int*) res;
      if(*x==1){//si le mot de passe est trouvé, dire au maitre
	printf("code found\n");
	send_str(parent, 4, msg2);
	return;
      }
      else{
	sem_wait(&sem);
	char* tmp1=intervalle(index, pas_thread);
	strcpy(index, tmp1);
	args=malloc(sizeof(struct Args));
	strcpy(args->index, index);
	strcpy(args->code, code);
	args->pas=pas_thread;
	free(tmp1);
	printf("intervalle commence %s\n", index);
	pthread_create(&fils_th[i], NULL, test, (void*)args);
	cur_index+= pas_thread;
	if(cur_index==pas/2){
	  send_str(parent,6, msg1);
	  printf("au milieu\n");
	}
	sem_post(&sem);
      }
    }
  }
  printf("mot de passe non trouve\n");
}


int main(int argc, char** argv){
  printf("COMMENCER!!!!\n");
  // int mytid, parenttid;
  int mytid;
  char code[100];
  int pas;// intervalle
  char test_str[100];
  int nb_thread;
  mytid=pvm_mytid();
  parenttid=pvm_parent();
  pvm_recv(parenttid,1);//tag=1, recevoir le 1er intervalle distribué
  pvm_upkstr(test_str);
  printf("I'm a child, my id is %d .Recv msg %s\n",mytid, test_str);
  pvm_recv(parenttid,2);//tag=2, recevoir la taille de l'intervalle
  pvm_upkint(&pas, 1,1);
  pvm_recv(parenttid,3);//tag=3, recevoir le mot de passe
  pvm_upkstr(code);
  pvm_recv(parenttid,7);//tag=7, recevoir le nombre de thread
  pvm_upkint(&nb_thread,1,1);
  printf("Je vais lancer %d threads\n", nb_thread);


  //  int pas_thread=pas/(nb_thread*10); // la taille de l'intervalle pour chaque thread
  //  int pas_thread=50;
  int pas_thread=pow(2, nb_thread);
  //tester le premier intervalle de cet esclave
  lancer_test(test_str, code, pas, pas_thread, nb_thread, parenttid);

  while(1){
    pvm_recv(parenttid,5); //tag=5, recevoir le nouvel intervalle
    pvm_upkstr(test_str);
    printf("Recv msg %s\n",test_str);
    lancer_test(test_str, code, pas, pas_thread, nb_thread, parenttid);
  }
  pvm_exit();
  return 0;
}
