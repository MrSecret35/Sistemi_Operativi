#define _GNU_SOURCE/* Per poter compilare con -std=c89 -pedantic */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>/*for fork*/
#include<sys/shm.h>/*for SM(shared Memory)*/
#include<signal.h>
#include<sys/types.h>/*for portability (vari flag)*/
#include<sys/sem.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<string.h>
#include<errno.h>
#include<time.h>
#include<sys/wait.h>
#include<math.h>
#include "function.h"
	
/*variabile utilizzata per il termine del round*/
boolean ROUND=TRUE;

int move(char* map, int idsem, int pos[], int obj[],int config[]);

int findMove(int pos[], int obj[]);

void term_round(){
	ROUND=FALSE;
}

int main(int argv, char* args[], char* env_args[]){
	struct msgbuf_Piece data;
	struct msgbufConq conq;
	struct msgbufTR t_round;
	int goal;	/*esito del movimento*/
	int n_moves;	/*numero mosse rimanenti*/
	int config[CONF];	/* array che conterrà tutte le config del gioco*/
	int posix[2];	/*array per la posizione*/
	int object[2];	/*array pet rla posizione obiettivo*/
	int pos_obj;	/*posizione obiettivo*/
	boolean arrivo, imp;
	int idshm, idshmM, idshmMP, idmsnPP;
	char L;
	struct sigaction sa_TR;	/*struct per il set dei vari handler*/
	struct map* mappa;
	char* map;
	int rcv_round;	/*esito di una message recieve*/
	sigset_t my_mask;	/*maschera per SIGINT*/

	/*CODE*/

	srand(time(0));
	idshm=atoi(args[1]);
	idmsnPP=atoi(args[2]);
	L=*args[3];

	/*handler for SIGUSR1*/
	bzero(&sa_TR, sizeof(sa_TR));
	sa_TR.sa_handler= term_round;
	sigaction(SIGUSR1,&sa_TR,NULL);
	
	/* Blocking SIGINT */
	sigemptyset(&my_mask);                  /* set an empty mask */
	sigaddset(&my_mask, SIGINT);            /* add SIGINT to the mask */
	sigprocmask(SIG_BLOCK, &my_mask, NULL); /* mask SIGINT in the process */


	initConf(config);
	n_moves=config[SO_N_MOVES];

	msgrcv(idmsnPP, &data, sizeof(data),getpid(),0);
	posix[0]=data.pos[0];
	posix[1]=data.pos[1];

	msgrcv(idmsnPP, &data, sizeof(data),getpid(),0);
	while(TRUE){
		switch(data.tag){
			case 'F':/*leggiere i dati del round*/
				/*prendo il mio obiettivo*/
				object[0]=data.obj[0];
				object[1]=data.obj[1];
				msgrcv(idmsnPP, &data, sizeof(data),getpid(), 0);
				pos_obj=(object[1]*config[SO_BASE])+object[0];

				mappa=(struct map*)shmat(idshm,NULL,0);
				map=(char*)shmat(mappa->idMap,NULL,0);
			break;

			case 'R':/*Round*/
				arrivo=FALSE;
				imp=FALSE;
				ROUND=TRUE;
				if(object[0]==-1 ||  object[1]==-1 || n_moves==0){
					imp=TRUE;
				}
				while(arrivo==FALSE && imp==FALSE && ROUND==TRUE){
					/*x=findGoal(posix,object,idshm,config);*/
					goal=move(map, mappa->idSam, posix, object, config);
					switch(goal){
						case 1:
							n_moves--;
						break;

						case 2:
							arrivo=TRUE;
						break;
					}
					if(*(map+pos_obj)!=FLAG || n_moves==0){
						/*obiettivo già catturato*/
						imp=TRUE;
					}
				}
				rcv_round=msgrcv(idmsnPP, &data, sizeof(data),getpid(),0);	
				/*bug di crash msgrcv dovuta ad un segnale*/
				if(rcv_round == -1){
					msgrcv(idmsnPP, &data, sizeof(data),getpid(),0);
				}			
			break;

			case'P':/*Termine round*/
				shmdt(map);
				shmdt(mappa);
				if(arrivo==TRUE){
					t_round.tag='C';
				}else{
					t_round.tag='L';
				}
				/*calcolo dati*/
				t_round.type=getppid();
				t_round.id=getpid();
				t_round.pos[0]=posix[0];
				t_round.pos[1]=posix[1];
				t_round.mox=n_moves;
				/*invio dati al Player*/
				msgsnd(idmsnPP, &t_round, sizeof(t_round), 0);

				/*aspetto segnale per continuale l'esecuzione || terminare*/
				msgrcv(idmsnPP, &data, sizeof(data),getpid(),0);


			break;
			
			case 'T':/*Termine Game*/
				exit(0);
			break;
		}
	}
}

int move(char* map, int idsem, int pos[], int obj[],int config[]){
	int m;	/*carattere che conterra' lid del movimento*/
	int op[4][2];
	int num,new_num;
	int new_pos[2];
	struct sembuf opE,opR;	/*OPerazioni di Entrata e di Rilascio*/
	int j;	/*return dello operazione su semafori*/
	int i;	/*variabile contatore*/
	struct timespec tim;
	int result;
	/*CODE*/
	

	op[UP][0]=0;
	op[UP][1]=-1;

	op[RIGHT][0]=1;
	op[RIGHT][1]=0;

	op[DOWN][0]=0;
	op[DOWN][1]=1;

	op[LEFT][0]=-1;
	op[LEFT][1]=0;
	/*cerco la mossa*/
	m=findMove(pos, obj);

	/*mi muovo*/
	num=(pos[1]*config[SO_BASE])+pos[0];

	new_pos[0]=pos[0]+op[m][0];
	new_pos[1]=pos[1]+op[m][1];
	new_num=(new_pos[1]*config[SO_BASE])+new_pos[0];

	opE.sem_num=new_num;
	opE.sem_op=-1;
	opE.sem_flg=IPC_NOWAIT;
	
	result=0;
	j=semop(idsem,&opE,1);
	if(j==0){
		result=1;
		if(*(map+new_num)==FLAG){
			result=2;
		}
		*(map+new_num)=*(map+num);
		*(map+num)=VUOTO;
		pos[0]=new_pos[0];
		pos[1]=new_pos[1];

		opR.sem_num=num;
		opR.sem_op=1;
		opR.sem_flg=0;

		j=semop(idsem,&opR,1);

		tim.tv_sec=0;
		tim.tv_nsec=config[SO_MIN_HOLD_NSEC];
		nanosleep(&tim,NULL);

	}else{/*gestisco il problema*/

	}
	return result;
}

int findMove(int pos[], int obj[]){
	int op[4][2];	/*operazioni possibili sulla pedina*/
	int mom, min;	/*utilizzate per il controllo tra valore MOMentaneo e MINore*/
	int mini;	/*conterrà l'id della mosssa migliore*/
	int i;	/*variabile contatore*/
	int posM[2];	/*coordinate moomentanee*/
	int config[CONF]; /* array che conterrà tutte le config del gioco*/
	int nop[4]={RIGHT,LEFT,UP,DOWN};	/*indicatore della mossa(up,right,down,left)*/
	/*CODE*/
	initConf(config);
	op[2][0]=0;
	op[2][1]=-1;

	op[0][0]=1;
	op[0][1]=0;

	op[3][0]=0;
	op[3][1]=1;

	op[1][0]=-1;
	op[1][1]=0;

	min=config[SO_BASE]*config[SO_ALTEZZA];
	for (i = 0; i < 4; ++i){
		posM[0]=pos[0]+op[i][0];
		posM[1]=pos[1]+op[i][1];
		if(posM[1]>=0 && posM[1]<=config[SO_ALTEZZA] && posM[0]>=0 && posM[0]<=config[SO_BASE]){
			mom=fabs(posM[0]-obj[0])+fabs(posM[1]-obj[1]);
			if(mom<min){
				min=mom;
				mini=nop[i];
			}
		}
	}

	return mini;
}
