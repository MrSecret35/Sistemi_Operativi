#define _GNU_SOURCE/* Per poter compilare con -std=c89 -pedantic*/
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


/*
* Funzione per il posizionamento delle pedine sulla mappa di gioco
* Deprecated
*/
void initPosix(int idshm,int idshmM, int config[], struct pce* piece, char L);

/*
* Funzione per il posizionamento delle pedine sulla mappa di gioco
*/
void PosixPiece(int idshm,int config[], struct pce* piece, char L, int indice);

/*
* Funzione per indicare un obiettivo a tutte le pedine(-1 se obiettivo nullo)
*/
void findHunter(int idshm, int idmsnPP, int config[], struct pce* pceA, char L);


int main(int argv, char* args[], char* env_args[]){
	int i, j;	/*variabili contatore*/
	int config[CONF];	/* array che conterrà tutte le config del gioco*/
	struct pce* pceA;	/*puntatore ad una sequenza di struct per dati delle pedine*/
	int idmsnPP;	/*id coda di messaggi con pedine*/
	/*struct msgbuf utilizzate per la comunicazione su message queque*/
	struct msgbuf_Player turn,risp;	/*msg buf utilizzate per la comune comunicazione con il master*/
	struct msgbuf_Piece data;	/*msg buf utilizzate per la comunicazione dei dati con le pedine*/
	struct msgbufConq conq;		/*msg buf utilizzate per la comunicazione di conquista dalle pedine*/

	int idshm,idmsn, idshmM, idshmMP;
	struct map* mappa;	/*struct per l'attach in SM*/
	pid_t f;	/*variabile che conterrà i pid delle pedine*/
	int mom_obj[2];	/*array utilizzato per il controllo dell'obiettivo*/
	int f_conq,f_term;	/*conterranno i valori restituiti dalle msgrcv*/
	struct timespec tim;	/*struct utilizzata per la nanosleep*/
	struct msgbufTR t_round;	/*msg buf per comunicazione dati di fine round*/
	int mosse;	/*somma delle mosse utilizzate fino al n-esimo round*/
	char L;		/*identificatore assegnato al player*/
	char* args_out[5];
	char* env_args_out[2];
	char buffer[100], buffer1[100], buffer2[100];
	/*buffer utilizzati per l'uso di execve e sprintf*/
	int* busy;/*array di interi per segnare chi ha un obiettivo*/
	sigset_t my_mask;	/*maschera per SIGINT*/

	/*CODE*/
	srand(time(0));

	/* Blocking SIGINT */
	sigemptyset(&my_mask);                  /* set an empty mask */
	sigaddset(&my_mask, SIGINT);            /* add SIGINT to the mask */
	sigprocmask(SIG_BLOCK, &my_mask, NULL); /* mask SIGINT in the process */

	initConf(config);
	idshm=atoi(args[1]);
	idmsn=atoi(args[2]);
	L=*args[3];

	risp.tag='F';
	risp.id=getpid();
	pceA=malloc(sizeof(struct pce)*config[SO_NUM_P]);

	/*creo la coda di messaggi con le pedine PP= player to Piece*/
	idmsnPP=msgget(IPC_PRIVATE, IPC_CREAT|0666);


	/*creazione e piazzamento pedine iniziale*/
	for (i = 0; i <config[SO_NUM_P]; ++i){
		/*aspetto il turno*/
		msgrcv(idmsn, &turn, sizeof(turn), getpid(), 0);
		if(turn.tag=='G'){	
			/*piazzo(creo) a pedina*/
			
			args_out[0]="piece";
			
			sprintf(buffer,"%d",idshm);
			args_out[1]=buffer;

			sprintf(buffer1,"%d",idmsnPP);
			args_out[2]=buffer1;
			
			sprintf(buffer2,"%c",L);
			args_out[3]=buffer2;

			args_out[4]= (char*)0;

			env_args_out[0]="/piece";
			env_args_out[1]=(char*)0;

			f=fork();
			switch(f){
				case -1:
					printf("Error 11: Impossibili creare il processo pedina\n");
					/*gestire errore*/
					for (j = 0; j < i; j++){
						kill((pceA+j)->PID,SIGKILL);
					}
					exit(0);
				break;

				case 0:
					execve(args_out[0],args_out,env_args_out);
					exit(0);
				break;

				default:
					(pceA+i)->PID=f;
					(pceA+i)->obj[0]=-1;
					(pceA+i)->obj[1]=-1;
					(pceA+i)->moves=config[SO_N_MOVES];
					/*piazzamento pedina*/
					PosixPiece(idshm,config,(pceA+i), L, i);
			}
		}
		risp.type=turn.id;
		risp.tag='F';
		risp.id=getpid();
		msgsnd(idmsn, &risp, sizeof(risp),0);
		/*cedo il turno*/
	}

	/*invio i primi dati alle pedine*/
	for (i = 0; i <config[SO_NUM_P]; ++i){
		data.type=(pceA+i)->PID;
		data.pos[0]=(pceA+i)->pos[0];
		data.pos[1]=(pceA+i)->pos[1];
		data.obj[0]=(pceA+i)->obj[0];
		data.obj[1]=(pceA+i)->obj[1];
					
		msgsnd(idmsnPP, &data, sizeof(data), 0);
	}
		

	/*round*/
	msgrcv(idmsn, &turn, sizeof(turn), getpid(), 0);
	while(TRUE){
		switch(turn.tag){

			case 'F':/*first indications*/
				/*stabilisco gli obiettivi (findHunter)*/
				
				findHunter(idshm, idmsnPP, config, pceA, L);
				/*avverto il master sul completamento fase preliminare*/
				risp.tag='F';
				risp.type=turn.id;
				msgsnd(idmsn, &risp, sizeof(risp), 0);

				/*aspetto il segnale per cominciare a muovere le pedine*/
				msgrcv(idmsn, &turn, sizeof(turn), getpid(), 0);
			break;

			case 'R':/*restart round*/
				/*avvio il movimento delle pedine*/
				for (i = 0; i < config[SO_NUM_P]; ++i){
					data.tag='R';
					data.type=(pceA+i)->PID;
					msgsnd(idmsnPP, &data, sizeof(data), 0);
				}
				msgrcv(idmsn, &turn, sizeof(turn),getpid(), 0);
			break;

			case 'P':/*Termine round*/
				mosse=0;
				/*invio messaggi di fine round e richiedo io dati*/
				for (i = 0; i < config[SO_NUM_P]; ++i){
					kill((pceA+i)->PID,SIGUSR1);
					data.type=(pceA+i)->PID;
					data.tag='P';
					msgsnd(idmsnPP, &data, sizeof(data),0);

				}
				/*ricevo risposta con dati dalle pedine*/
				for (i = 0; i < config[SO_NUM_P]; i++){
					msgrcv(idmsnPP, &t_round, sizeof(t_round),getpid(), 0);
					if(t_round.tag=='C'){
						conq.type=getppid();
						conq.id=getpid();
						conq.pos[0]=t_round.pos[0];
						conq.pos[1]=t_round.pos[1];
						conq.tag='C';
						msgsnd(idmsn, &conq, sizeof(conq), 0);
					}
					/* gestisco risposta delle pedine*/
					for(j = 0; j < config[SO_NUM_P]; ++j){

						if((pceA+j)->PID==t_round.id){
							(pceA+j)->pos[0]=t_round.pos[0];
							(pceA+j)->pos[1]=t_round.pos[1];
							(pceA+j)->moves=t_round.mox;
							mosse+=t_round.mox;
						}
					}
				}
				msgrcv(idmsn, &turn, sizeof(turn), getpid(),0);

				t_round.type=getppid();
				t_round.mox=mosse;
				t_round.id=getpid();
				
				/*invio messaggio con dati al Master*/
				msgsnd(idmsn, &t_round, sizeof(t_round), 0);
				/*ricevo messaggio di re-inizio round (F)*/
				msgrcv(idmsn, &turn, sizeof(turn), getpid(),0);
			break;

			case'T':/*termine Game*/
				/*invio messaggio alle pedine di fine game*/
				for (i = 0; i < config[SO_NUM_P]; ++i){
					data.type=(pceA+i)->PID;
					data.tag='T';
					msgsnd(idmsnPP, &data, sizeof(data),0);
				}
				
				while(wait(0)!=-1){}
				msgctl(idmsnPP,IPC_RMID,NULL);	/*chiudo coda di messaggi*/
				exit(0); /*bye bye*/
			break;
		}
	}
}

/*
* idshm= id memoria condivisa
* idmsnPP= id coda dei messaggi tra giocatore e pedine(Player to Piece || Piece to Player)
* config= array che contiene le config del gioco
* pceA=array contenente i dati delle pedine
* L=lettera, identificatore assegnato al giocatore
*/
void findHunter(int idshm, int idmsnPP, int config[], struct pce* pceA, char L){
	int i;	/*variabile contatore*/
	int x,y;	/*coordinate della scacchiera*/
	int num_f;	/*variabile utilizzata per il posizionamento sulla scacchiera*/
	struct map* mappa;	/*struttura in memoria condivisa*/
	char* map;	/*schacchiera char in memria condivisa*/
	int dist, dist_mom;
	int pid;
	boolean* find;
	int scelta;
	struct msgbuf_Piece data;
	int obj[2];
	int mpop;	/*max mosse per ogni pedina*/
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);
	find=(boolean*)malloc(sizeof(boolean)*config[SO_NUM_P]);

	/*setto tutte le pedine come libere*/
	for(i=0; i<config[SO_NUM_P]; i++){
		*(find+i)=FALSE;
	}
	mpop=(config[SO_BASE]*config[SO_ALTEZZA])/(config[SO_NUM_P]);

	/*controllo la scacchiera in cerca di bandierine*/
	for (x = 0; x < config[SO_BASE]; ++x){
	for(y=0; y<config[SO_ALTEZZA]; y++){
		dist=config[SO_BASE]*config[SO_ALTEZZA]-1;
		num_f=(y*config[SO_BASE])+x;
		scelta=-1;
		if(*(map+num_f)==FLAG){
			/*cerco tra le mie pedine quella piu' vicina*/
			for(i=0; i<config[SO_NUM_P]; i++){
				dist_mom=abs((pceA+i)->pos[0]-x)+abs((pceA+i)->pos[1]-y);
				if(dist_mom<dist && dist_mom<=(pceA+i)->moves && *(find+i)==FALSE){
					scelta=i;
					dist=dist_mom;
				}
			}
			/*invio il messaggio di obiettivo*/
			if(scelta>=0){
				*(find+scelta)=TRUE;
				data.type=(pceA+scelta)->PID;
				data.pos[0]=(pceA+scelta)->pos[0];
				data.pos[1]=(pceA+scelta)->pos[1];
				data.obj[0]=x;
				(pceA+scelta)->obj[0]=x;
				data.obj[1]=y;
				(pceA+scelta)->obj[1]=y;

				data.tag='F';
				msgsnd(idmsnPP, &data, sizeof(data),0);
			}
		}
	}
	}

	for(i=0; i<config[SO_NUM_P]; i++){
		if(*(find+i)==FALSE){
			dist_mom=findGoal((pceA+i)->pos,obj,idshm,config);
			if(dist_mom<=(pceA+i)->moves && dist_mom<=mpop){
				data.obj[0]=obj[0];
				(pceA+i)->obj[0]=obj[1];
				data.obj[1]=obj[0];
				(pceA+i)->obj[1]=obj[1];
			}else{
				data.obj[0]=-1;
				(pceA+i)->obj[0]=-1;
				data.obj[1]=-1;
				(pceA+i)->obj[1]=-1;
			}
			data.type=(pceA+i)->PID;
			data.pos[0]=(pceA+i)->pos[0];
			data.pos[1]=(pceA+i)->pos[1];
			data.tag='F';
			msgsnd(idmsnPP, &data, sizeof(data),0);
		}
	}

	shmdt(map);
	shmdt(mappa);
}

/*
* idshm= id memoria condivisa
* config=array che contiene le config del gioco
* piece= dati della pedina da posizionare
* L=lettera, identificatore assegnato al giocatore
* indice= indice che assume la pedina durante il posizionamento
*/
void PosixPiece(int idshm,int config[], struct pce* piece, char L, int indice){
	struct map* mappa;
	char* map;
	int BA[2];	/*array che conterrà in num pedine per base e per altezza*/
	int pB,pA;
	int nBB,nBA;	/*numero blocchiper base e altezza*/
	int randB,randH;	/*base e altezza casuali*/
	int num;	/*numero equivalente alle cordinate*/
	int k,g;	/*variebili per generare pA*/
	boolean b=FALSE;
	int j;	/*risultato della semop*/
	struct sembuf sops;	/*struct per l'operazione su semafori*/

	findBA(config[SO_NUM_P],BA);
	
	/*genero un num casuale per le colonne*/
	pB=indice%BA[0];
	nBB=config[SO_BASE]/BA[0];

	randB=(rand()%nBB)+(pB*nBB);


	/*genero un num casuale per le righe*/
	k=BA[0];
	for(g=0;k<=indice;g++){
		k+=BA[0];
	}
	pA=g;

	nBA=config[SO_ALTEZZA]/BA[1];
	randH=(rand()%nBA)+(pA*nBA);

	/*posiziono la pedina*/
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);

	while(!b){

		num=( ( randH * config[SO_BASE] ) + randB );
		
		sops.sem_num=num;
		sops.sem_op=-1;
		sops.sem_flg=IPC_NOWAIT;
		
		j=semop(mappa->idSam, &sops , 1);
		/*TEST_ERROR;*/
		if(j==0){
			piece->pos[0]=randB;
			piece->pos[1]=randH;
			*(map+num)=L;
			b=TRUE;

		}else{
			randB=(rand()%nBB)+(pB*nBB);
			randH=(rand()%nBA)+(pA*nBA);
		}
	}
	shmdt(map);
	shmdt(mappa);
}

/*
* idshm= id memoria condivisa
* idshmM= id memoria condivisa della Mappa
* config=array che contiene le config del gioco
* piece= dati della pedina da posizionare
* L=lettera, identificatore assegnato al giocatore
*/
void initPosix(int idshm, int idshmM, int config[], struct pce* piece, char L){
	int x, y;
	boolean b=FALSE;
	struct sembuf sops;
	int num;
	int j;
	struct map* mappa;
	char* map;

	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);
	while(!b){

		x=rand()%config[SO_BASE];
		y=rand()%config[SO_ALTEZZA];

		num=( ( y * config[SO_BASE] ) + x );
		
		sops.sem_num=num;
		sops.sem_op=-1;
		sops.sem_flg=IPC_NOWAIT;
		
		j=semop(mappa->idSam, &sops , 1);
		/*TEST_ERROR;*/
		if(j==0){
			piece->pos[0]=x;
			piece->pos[1]=y;
			*(map+num)=L;
			b=TRUE;

		}
	}
	shmdt(map);
	shmdt(mappa);
}
