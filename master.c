#define _GNU_SOURCE/* Per poter compilare con -std=c89 -pedantic */
#include<stdio.h> /*standard input output*/
#include<stdlib.h>
#include<unistd.h>/*for fork*/
#include<sys/shm.h>/*for SM(shared Memory)*/
#include<signal.h>/*library for signal es(EINTR)*/
#include<sys/types.h>/*for portability (vari flag)*/
#include<sys/ipc.h>/*Inter Process Comunication*/
#include<sys/sem.h>/*Semaphores*/
#include<sys/msg.h>/*Message queqe*/
#include<string.h>
#include<errno.h>
#include<time.h>
#include<sys/wait.h>
#include<math.h>
#include "function.h"

/*
*		Processo MASTER
*il suo compito è di gestire il gioco con i vari round, 
*le varie stampe,i processi giocatore e il campo di gioco(scacchiera)
*/

boolean ALLARME=FALSE;/*variabile globale utilizzata per la gestione dell'alarm e del termine game*/
/*
*Funzione utilizzata dal masterper il posizionamento delle bandierine ogni round
*/
void posixFlag(int idshm, struct flag* flagA, int numF);

/*
*Funzione che effettua un reset sui punteggi delle bandierine sulla mappa a 0
*/
void resetPunt(int idshm);

/*
*Funzione che effettua un set della schacchiera con un '.'
*/
void stet(int idshm);

/*
*Funzione utilizzata per la stampa della mappa di gioco
*/
void PrintMap(int idshm);


/*
*Funzione che stampa lo stato del gioco
*/
void PrintStatus(int idshm, struct player* playerA);

/*
* Handler utilizzato per gestire l'ALARM
*/
void GameOver(){
	ALLARME=TRUE;
}


int main(){
	int config[CONF]; /* array che conterrà tutte le config del gioco*/
	int i,p,g,k,q;	/*variabili contatori*/
	pid_t f;	/*variabile che conterra il return di una fork()*/
	struct map* sch;/*creao la mappa da sistemare in mem. condivisa*/
	int idshm;	/*id della shared memory*/
	int idshmM;	/*id della seconda shared memory per la Matrice*/
	int idshmMP;	/*id della seconda shared memory per la Matrice dei punteggi*/
	int idsem;	/*id della i semafori*/
	int idmsn;	/*id della coda di messaggi*/
	struct msgbuf_Player turn,risp;
	/*struct msgbuf utilizzate per la comunicazione su message queque*/
	struct msgbufTR t_round;	/*messaggi di fine round*/
	struct player* playerA;	/*strutture per i dati dei giocatori*/
	struct flag* flagA;	/*strutture per i dati delle pedine*/
	int numF;	/*numero di bandierine variabile per ogni turno*/
	int dimM;	/*dimensione della matrice(base*altezza)*/
	int round;	/*contatore per il numero di round totali*/
	struct sigaction sa_alarm;	/*struct per il set dei vari handler*/
	struct msgbufConq conq;	/*struct dei messaggiper la conquista di bandierine*/
	int game_time;	/*variabile accumulatore del tempo di gioco*/
	double rap;
	int tot_punt;
	char* args[5];
	char* env_args[2];
	char l;	/*lettera assegnata ad ogni player*/
	char buffer[100], buffer1[100], buffer2[100], buffer3[100], buffer4[100];
	/*buffer utilizzati per l'uso di execve e sprintf*/

	/*CODE*/

	srand(time(0));
	initConf(config);

	if(control(config)){
		printf("Interruzione del codice\n");
		exit(0);
	}
	playerA=malloc(config[SO_NUM_G]* sizeof(struct player));
	
	

	/*Creo id della memoria condivisa*/
	idshm=shmget(IPC_PRIVATE, sizeof(struct map), IPC_CREAT|0666);
	TEST_ERROR;

	/* apro la memoria condivisa */
	sch=(struct map*)shmat(idshm,NULL,0);
	TEST_ERROR;

	/*creo id dei semafori*/
	idsem=semget(IPC_PRIVATE,config[SO_BASE]*config[SO_ALTEZZA], IPC_CREAT|0666);
	TEST_ERROR;
	sch->idSam=idsem;

	/*creo id della matrice in memoria condivisa*/
	idshmM=shmget(IPC_PRIVATE, sizeof(char)*config[SO_BASE]*config[SO_ALTEZZA], IPC_CREAT|0666);
	TEST_ERROR;
	sch->idMap =idshmM;

	/*creo id della matrice dei punteggi in memoria condivisa*/
	idshmMP=shmget(IPC_PRIVATE, sizeof(int)*config[SO_BASE]*config[SO_ALTEZZA], IPC_CREAT|0666);
	TEST_ERROR;
	sch->idScore =idshmMP;
	
	/*set iniziale di tutti i semafori ad 1*/
	for (i = 0; i < config[SO_ALTEZZA]*config[SO_BASE]; ++i){
		semctl(idsem, i,SETVAL, 1);
	}
	stet(idshm);
	shmdt(sch);

	/*creo la coda di messaggi per comunicare */
	idmsn=msgget(IPC_PRIVATE, IPC_CREAT|0666);
	TEST_ERROR;

	/*Handler per la gestione dei segnali SIGINT SIGALRM*/
	bzero(&sa_alarm, sizeof(sa_alarm));
	sa_alarm.sa_handler= GameOver;
	sigaction(SIGALRM,&sa_alarm,NULL);
	sigaction(SIGINT,&sa_alarm,NULL);

	/*creo i giocatori*/
	for (i = 0; i < config[SO_NUM_G]; ++i){

		l=(char)(65+i);
		args[0]="player";
		
		sprintf(buffer,"%d",idshm);
		args[1]=buffer;

		sprintf(buffer1,"%d",idmsn);
		args[2]=buffer1;

		sprintf(buffer2,"%c",l);
		args[3]=buffer2;

		args[4]= (char*)0;
		env_args[0]="/player";
		env_args[1]=(char*)0;
				
		f=fork();
		switch(f){
			case -1:
				printf("Error 10: Impossibili creare il processo player\n");
				for (p = 0; p < i; p++){
					kill((playerA+p)->PID,SIGKILL);
				}
				exit(0);
			break;

			case 0:
				execve(args[0],args,env_args);
				exit(0);
			break;

			default:
				(playerA+i)->PID=f;
				(playerA+i)->punteggio=0;
				(playerA+i)->mosse=config[SO_NUM_P]*config[SO_N_MOVES];
				(playerA+i)->id=l;
		}
	}
	
	/*i giocatori piazzano le proprie pedine a turno*/
	risp.tag='F';
	for (p = 0; p < config[SO_NUM_P]; ++p){
		for (g = 0; g < config[SO_NUM_G]; ++g){
			turn.type=(playerA+g)->PID;
			turn.tag='G';
			turn.id=getpid();
			
			msgsnd(idmsn,&turn,sizeof(turn),0);
			msgrcv(idmsn,&risp,sizeof(risp),getpid(),0);
		}
	}
	
	
	game_time=0;
	/*ROUND*/
	for (round=0;TRUE; round++){
		/*preparazione*/
		resetPunt(idshm);

		/*il master piazza le bandiere*/
		numF=(int)(rand()%
			(config[SO_FLAG_MAX]-config[SO_FLAG_MIN]+1))
			+config[SO_FLAG_MIN];
		printf("Numero bandiere\t%d\n",numF );
		flagA=malloc(sizeof(struct flag)*numF);
		posixFlag(idshm,flagA, numF);


		printf("stampa Stato inizio Round\n");
		PrintStatus(idshm, playerA);

		/*i giocatori danno le prime indicazioni alle pedine*/
		for (g = 0; g < config[SO_NUM_G]; ++g){
			turn.type=(playerA+g)->PID;
			turn.tag='F';
			turn.id=getpid();
			msgsnd(idmsn,&turn,sizeof(turn),0);		
		}

		/*ricevo risposta dai giocatori */
		i=0;
		do{
			msgrcv(idmsn,&risp,sizeof(risp),getpid(),0);
			i++;
		}while(risp.tag=='F' && i!=config[SO_NUM_G]);
		/*fine preparazione*/	


		/*parte il round*/
		
		/*parte il TIMER*/
		alarm(config[SO_MAX_TIME]);
		/*informo i giocatori*/
		for (g = 0; g < config[SO_NUM_G]; ++g){
			turn.type=(playerA+g)->PID;
			turn.tag='R';
			turn.id=getpid();
			msgsnd(idmsn,&turn,sizeof(turn),0);	
		}

		/*controllo la terminazione del round*/
		while(isFlagNew(idshm, config, flagA, numF)==TRUE && ALLARME==FALSE){}
		/*termine n-esimo round*/

		/*aggiorno tempo totale di gioco*/
		game_time+=(config[SO_MAX_TIME]-alarm(0));

		/*invio tag di fine round*/
		for(g = 0; g < config[SO_NUM_G]; ++g){	
			turn.type=(playerA+g)->PID;
			turn.tag='P';
			turn.id=getpid();
			msgsnd(idmsn,&turn,sizeof(turn),0);
		}
		/*ricevo un messaggio per ogni conquista e aggiorno il punteggio*/
		for (g = 0; g < numF-numFlag(idshm,config); ++g){
			msgrcv(idmsn, &conq, sizeof(conq), getpid(),0);
			for(k = 0; k < config[SO_NUM_G]; ++k){	
				if((playerA+k)->PID== conq.id){
					(playerA+k)->punteggio += scoreFlag(idshm, conq.pos);
				}
			}
		}
		/*richiedo messaggi di fine round*/
		for(g = 0; g < config[SO_NUM_G]; ++g){	
			turn.type=(playerA+g)->PID;
			turn.tag='p';
			turn.id=getpid();
			/*invio tag di fine round*/
			msgsnd(idmsn,&turn,sizeof(turn),0);
		}
		/*ricevo messaggio di termine round contenente le mosse*/
		for(g = 0; g < config[SO_NUM_G]; ++g){	
			/*ricevo i messaggi del giocatore*/
			msgrcv(idmsn,&t_round,sizeof(t_round),getpid(),0);
			/*gestisto i messaggi ricevuti*/
			for(k = 0; k < config[SO_NUM_G]; ++k){	
				if((playerA+k)->PID== t_round.id){
					(playerA+k)->mosse=t_round.mox;
				}
			}
		}
		PrintStatus(idshm, playerA);
		
		/*se l'allarme e' scattato allora fermo il game*/
		if(ALLARME){
			/*invio messaggio di termine game ai player*/
			for (q = 0; q < config[SO_NUM_G]; ++q){
				turn.type=(playerA+q)->PID;
				turn.tag='T';
				turn.id=getpid();
				msgsnd(idmsn,&turn,sizeof(turn),0);
			}

			printf("stampa fine Game\n\n");
			printf("\tRound Giocati:\t\t%d\n",round);
			for (p = 0; p < config[SO_NUM_G]; ++p){
				printf("\tPlayer:\t\t%c\n",(playerA+p)->id );
				rap=((double)(config[SO_N_MOVES]*config[SO_NUM_P]-(playerA+p)->mosse)/(config[SO_NUM_P]*config[SO_N_MOVES]));
				printf("\t rapporto mosse utilizzate/totali\t\t%f\n",rap );
				rap=((double)(playerA+p)->punteggio/(playerA+p)->mosse);
				printf("\t rapporto punteggio/mosse utilizzate\t\t%f\n",rap );
				tot_punt+=(playerA+p)->punteggio;
				printf("\n\n");
			}
			rap=((double)tot_punt/game_time);
			printf("\t rapporto punti totali/tempo di gioco\t\t%f\n",rap );
			printf("\n\n");

			/*aspetto terminazione dei player*/
			while(wait(0)!=-1){}

			/*chiudo coda di messaggi e semafori*/
			msgctl(idmsn,IPC_RMID,NULL);
			semctl(idsem,0, IPC_RMID );

			/*chiudo memoria condivisa*/
			shmctl(idshmMP,IPC_RMID,NULL);
			shmctl(idshmM,IPC_RMID,NULL);
			shmctl(idshm,IPC_RMID,NULL);
			exit(0);
		}
	}
}

/*
* idshm=id della memoria condivisa
* flagA= dati delle bandierine
* numF= numero delle bandierine da posizionare
*/
void posixFlag(int idshm, struct flag* flagA, int numF){
	int config[CONF]; /*array che conterrà tutte le config del gioco*/
	struct map* mappa;
	boolean b=FALSE;
	int x,y;
	int num;
	int j;
	int i;
	struct sembuf sops;
	char* map;
	int* sco;
	int pippo;/**/
	int punt,puntot;

	/*CODE*/
	initConf(config);
	
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);
	sco=(int*)shmat(mappa->idScore,NULL,0);
	puntot=0;

	for (i = 0; i < numF; ++i){
		b=FALSE;
		while(!b){
			punt=0;
			x=rand()%config[SO_BASE];
			y=rand()%config[SO_ALTEZZA];

			num=( ( y * config[SO_BASE] ) + x );
			if(*(map+num)!=FLAG){
				sops.sem_num=num;
				sops.sem_op=-1;
				sops.sem_flg=IPC_NOWAIT;

				j=semop (mappa->idSam, &sops , 1);
				if(j==0){
					(flagA+i)->pos[0]=x;
					(flagA+i)->pos[1]=y;
					if(i == numF-1){
						punt=config[SO_ROUND_SCORE]-puntot;
					}else{
						punt=rand()%
							(config[SO_ROUND_SCORE]
							-(numF-i+1)-puntot);
					}
					
					(flagA+i)->punteggio=punt;
					*(map+num)=FLAG;
					*(sco+num)=punt;
					b=TRUE;
					puntot+=punt;

					sops.sem_num=num;
					sops.sem_op=1;
					sops.sem_flg=IPC_NOWAIT;
					j=semop(mappa->idSam, &sops , 1);
				}
			}
					
		}

	}
	shmdt(map);
	shmdt(mappa);
}

/*
* idshm=id della memoria condivisa
*/
void resetPunt(int idshm){
	struct map* mappa;
	int config[CONF]; 
	int i,j;
	int num;
	int* map;
	/*CODE*/
	initConf(config);
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(int*)shmat(mappa->idScore,NULL,0);

	for(i=0; i<config[SO_BASE];i++){
		for(j=0; j<config[SO_ALTEZZA];j++){
			num=( ( j * config[SO_BASE] ) + i );
			*(map+num)=0;
		}
	}
	shmdt(map);
	shmdt(mappa);
}

/*
* idshm=id della memoria condivisa
*/
void stet(int idshm){
	int i,j;	/*variabili contatori*/
	int num;
	int config[CONF]; /* array che conterrà tutte le config del gioco*/
	struct map* mappa;
	char* map;

	initConf(config);
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);

	for(i=0; i<config[SO_BASE];i++){
		for(j=0; j<config[SO_ALTEZZA];j++){
			num=( ( j * config[SO_BASE] ) + i );
			*(map+num)='.';
		}
	}
	shmdt(map);
	shmdt(mappa);
}

/*
* idshm=id della memoria condivisa
*/
void PrintMap(int idshm){
	int i,j;	/*variabili contatori*/
	int config[CONF]; /* array che conterrà tutte le config del gioco*/
	struct map* mappa;
	int num;
	char* map;
	/*CODE*/
	initConf(config);
	
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);

	printf("\n");
	for(i=0; i<config[SO_ALTEZZA];i++){
		for(j=0; j<config[SO_BASE];j++){
			num=( ( i * config[SO_BASE] ) + j );
			printf("%c", *(map+num));
		}
		printf("\n");
	}
	printf("\n");
	shmdt(map);
	shmdt(mappa);
}

/*
* idshm=id della memoria condivisa
* playerA= dati dei vari player
*/
void PrintStatus(int idshm, struct player* playerA){
	int config[CONF]; /* array che conterrà tutte le config del gioco*/
	int g;
	int mox;

	initConf(config);

	for ( g= 0, mox=0; g < config[SO_NUM_G]; ++g){
		printf("Giocatore:\t\t%c\n\n", (playerA+g)->id);
		printf("\tpunteggio:\t%d\n",(playerA+g)->punteggio);

		mox=(playerA+g)->mosse;

		printf("\tmosse residue\t%d\n\n\n", mox);
	}
	PrintMap(idshm);
}
