
#include "data.h"
#include<math.h>

void resetIntArray(int v[], int dim);
void initConf(int array[]);
int control(int config[]);
void reset(char v[], int dim);
int findGoal(int pos[], int obbj[], int idshm, int config[]);

boolean isFlag(int idshm, int config[]);
int isFlagNew(int idshm, int config[], struct flag* flagA, int numF);
int numFlag(int idshm,int config[]);

int scoreFlag(int idshm, int pos[]);
void findBA(int number,int BA[]);