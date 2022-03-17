#define TEST_ERROR    if (errno) {fprintf(stderr,			\
					  "%s:%d: PID=%5d: Error %d (%s)\n",\
					  __FILE__,			\
					  __LINE__,			\
					  getpid(),			\
					  errno,			\
					  strerror(errno));}

#define VUOTO '.'
#define FLAG '1'

/*
* struttura definita per l'uso della memoria condivisa
*/
struct map{
	int idSam;	/*id dei semafori della scacchiera(0 sem libero, 1 occupato)*/
	int idMap; 	/*1 per le bandierine, (A,B,C,D...)per i giocatori, '.' se è vuota*/
	int idScore;	/*matrice per i punteggi*/
};

/*
* struttura per i dati delle bandiere utilizzata dal master
*/
struct flag{
	int punteggio;	/*punteggio della bandiera*/
	int pos[2];	/*posizione della bandiera*/		
};

/*
* struttura per i dati delle bandiere utilizzata dal master
*/
struct player{
	int PID;	/*pid del player*/
	int punteggio;	/*punteggio del giocatore*/
	int mosse;	/*totale mosse utilizzate*/
	char id;	/*lettera che indica il giocatore*/
};

/*
* struttura per i dati delle pedine utilizzata dal player
*/
struct pce{
	int PID;	
	int pos[2];	
	int obj[2];	
	int moves;
};

/*utilizzata dal master per comunicare con i player*/
struct msgbuf_Player {
	long type; 	/* type of message (id destinatario)*/
	char tag;	/*G=go, F=Finished, R=restart round, T=termine Game, F=first indication, P=termine round*/
	int id;		/*id mittente*/
};

/*utilizzata dai player per passare i dati della pedina ad essa*/
struct msgbuf_Piece {	
	long type; 	/* type of message (id destinatario)*/
	char tag;	/*R=restart round,T=termine Game, F=first indication, P=termine round*/
	int pos[2];	/*posizione della pedina*/
	int obj[2];	/*obiettivo della pedina*/
};

/*utilizzata quando una pedina effettua una conquista*/
struct msgbufConq {
	long type; 	/* type of message (id destinatario)*/
	int pos[2];	/*posizione della bandierina conquistata*/
	char tag;	/*C=conquista*/
	int id;		/*id mittente*/
};

struct msgbufTR {
	long type; 	/* type of message (id destinatario)*/
	int pos[2];	/*posizione della bandierina conquistata*/
	int mox;	/*mosse utilizzate in quel round*/
	char tag;	/*C=conq, L=loser*/
	int id;		/*id mittente*/
};


typedef enum{FALSE, TRUE} boolean;

enum{	
	SO_NUM_G,		/*numero giocatori*/
	SO_NUM_P,		/*numero pedine*/
	SO_MAX_TIME,		/*durata massima di un singolo round*/
	SO_BASE,		/*dimensione campo di gioco*/
	SO_ALTEZZA,		/*dimensione campo di gioco*/
	SO_FLAG_MIN,		/*numero minimo di bandiere*/
	SO_FLAG_MAX,		/*numero massimo di bandiere*/
	SO_ROUND_SCORE,		/*punyteggio totale delle bandierine*/
	SO_N_MOVES,		/*numero totale di mosse per ogni pedina*/
	SO_MIN_HOLD_NSEC,	/*numero nanosecondi di occupazione di una cella per una pedina*/
	CONF			/*numero di configurazioni*/
};

enum{
	UP,
	LEFT,
	DOWN,
	RIGHT
};
