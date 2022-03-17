#include<stdio.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/shm.h>
#include<string.h>

#include "function.h"

/*
*Funzione utilizzata per resettare un array di char
*/
void resetCharArray(char v[], int dim){
	int i;
	for(i=0; i<dim; i++){
		v[i]='\0';
	}
}

/*
*Funzione utilizzata per resettare un array di int
*/
void resetIntArray(int v[], int dim){
	int i;
	for(i=0; i<dim; i++){
		v[i]=0;
	}
}

/*
*Funzione utilizzata per inizializzare il gioco prendendo tutti le config dal file "config"
*/
void initConf(int array[]){
	char* res;
	char str[256], num[10];
	int i,j,z,n;
	FILE* f= fopen("config","r");

	z=0;
	res=fgets(str,256, f);

	do{
		resetCharArray(num, sizeof(num));
		for(i=0; str[i]!=':'; i++){};

		for(j=0; str[i]!='\n'; i++){
			if(str[i]!=':' && str[i]!=' ' && str[i]!='\r'){
				num[j]=str[i];
				j++;
			}
		}
		array[z]=atoi(num);
		z++;
		res=fgets(str,256, f);
	}while(res!=NULL);


	fclose(f);
}

/*
* funzione utilizzata per controllare i valori di input
*/
int control(int config[]){
	int result;
	int i;

	result=0;
	for (i = 0; i < CONF && !result; ++i){
		if(config[i]<=0 && i!=SO_FLAG_MIN){
			result=-1;
			printf("Error 12: Non e' possibile inserire valori negativi o ugulali a 0\n");
		}
	}
	if(config[SO_NUM_G]>26){
		result=-1;
		printf("Error 13: Impossible inserire piu' di 26 giocatori\n");
	}
	if(config[SO_NUM_P]*config[SO_NUM_G]+config[SO_FLAG_MAX]>(config[SO_BASE]*config[SO_ALTEZZA])){
		result=-1;
		printf("Error 14: Dimensioni scacchiera minori del necessario\n");
	}
	if(config[SO_FLAG_MAX]<config[SO_FLAG_MIN]){
		result=-1;
		printf("Error 15: SO_FLAG_MAX deve essere maggiore di SO_FLAG_MIN\n");
	}
	if(config[SO_FLAG_MIN]==config[SO_FLAG_MAX] && config[SO_FLAG_MIN]==0){
		result=-1;
		printf("Error 16: SO_FLAG_MAX=0 SO_FLAG_MIN=0, Possibile Loop\n");
	}
	if(config[SO_ROUND_SCORE]<config[SO_FLAG_MAX]){
		result=-1;
		printf("Error 17: Non ci cono abbastanza punti per pedina(SO_ROUND_SCORE<SO_FLAG_MAX)\n");
	}
	return result;
}
/*
* data la pos di una pedina
* restituisce la distanza dalla bandiera piu' vicina 
* inserisce nel secondo parametro obbj la posizione di essa(bandierina)
*/
int findGoal(int pos[], int obbj[], int idshm, int config[]){
	int x,y;
	int num, numpce;
	int mom, dist;
	int a,b;
	int obj[2]; /*inizializzare le operazioni*/
	struct map* mappa;
	char* map;

	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);

	numpce=(pos[1]* config[SO_BASE])+pos[0];

	dist=config[SO_ALTEZZA]*config[SO_BASE]-1;

	for(x=0; x<config[SO_BASE]; x++){
		for(y=0; y<config[SO_ALTEZZA]; y++){
			num=(y* config[SO_BASE])+x;
			if(*(map+num)==FLAG){
				a=abs(pos[0]-x);
				b=abs(pos[1]-y);
				
				mom=a+b;
				if(mom<dist){
					dist=mom;
					obbj[0]=x;
					obbj[1]=y;

				}
			}
		}
	}
	shmdt(map);
	shmdt(mappa);
	return dist;
}

/*
* restituisce 1 se è presente almeno una bandiera, 0 altrimenti
*/
boolean isFlag(int idshm, int config[]){
	int i;
	boolean f;
	struct map* mappa;
	char* map;

	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);
	f=FALSE;
	for(i = 0; i < (config[SO_BASE]*config[SO_ALTEZZA]) && !f; ++i){
		if(*(map+i)==FLAG){
			f=TRUE;
		}
	}
	shmdt(map);
	shmdt(mappa);
	return f;
}

int isFlagNew(int idshm, int config[], struct flag* flagA, int numF){
	struct map* mappa;
	char* map;
	int f;
	int num;
	int i;

	f=FALSE;num=0;
	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);

	for (i = 0; i < numF && !f; ++i){
		num=((flagA+i)->pos[1]* config[SO_BASE])+(flagA+i)->pos[0];
		if(*(map+num)==FLAG){
			f=TRUE;
		}
	}

	shmdt(map);
	shmdt(mappa);
	return f;
}
/*
* restituisce il numero di bandiere presenti su mappa
*/
int numFlag(int idshm,int config[]){
	int i,f;
	struct map* mappa;
	char* map;

	mappa=(struct map*)shmat(idshm,NULL,0);
	map=(char*)shmat(mappa->idMap,NULL,0);
	f=0;
	for(i = 0; i < (config[SO_BASE]*config[SO_ALTEZZA]); ++i){
		if(*(map+i)==FLAG){
			f++;
		}
	}
	shmdt(map);
	shmdt(mappa);
	return f;
}

/*
* data la posizione di una bandiera ne restituisce il punteggio
*/
int scoreFlag(int idshm, int pos[]){
	int num,score;
	struct map* mappa;
	int config[CONF];
	int* sc;

	initConf(config);
	mappa=(struct map*)shmat(idshm,NULL,0);
	sc=(int*)shmat(mappa->idScore,NULL,0);

	num=(pos[1]*config[SO_BASE])+pos[0];
	score=*(sc+num);
	
	shmdt(sc);
	shmdt(mappa);
	return score;
}

/*
*Dato unn numero trova la combinazione media dei fattori che generano quel numero
*es: 20=(5,4);
*/
void findBA(int number,int BA[]){
	int minimo;
	int x,y;

	minimo=number+1;
	for(x=0; x<number+1; x++){
		for (y = 0; y < number+1; y++){
			if(x*y==number && x+y<=minimo){
				BA[0]=x;
				BA[1]=y;
				minimo=x+y;
			}
		}
	}
}
