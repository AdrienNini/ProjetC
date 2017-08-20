#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
//#include <windows.h>

//Constantes globaux
#define NB_PARTICIPANTS 22
#define CHANCE_STAND 15
#define CHANCE_ABANDON 2
#define chanceDrapeauJaune 5
#define chanceDrapeauRouge 2
#define chanceSecurityCar 4
#define chanceSecurityCarRentre 45
#define vitesseSecurityCar 2        //valeur par laquelle on multiplie le temps max pour parccourir un secteur 
#define dureeStandMax 5             //durée maximale qu'on peut passer dans un stand
#define dureeStandMin 2             //durée minimale
#define visitesStandMax 3           //nombre de visites au stands permis

//Constantes pour les essais
#define nbTourEssai_1 2
#define nbTourEssai_2 2
#define nbTourEssai_3 2
#define dureeEssai_1 90
#define dureeEssai_2 90
#define dureeEssai_3 60

//Constantes pour les qualifs
#define NB_PARTICIPANTSQualif_2 16
#define NB_PARTICIPANTSQualif_3 10
#define nbTourQualif_1 2
#define nbTourQualif_2 2
#define nbTourQualif_3 2
#define dureeQualif_1 18
#define dureeQualif_2 15
#define dureeQualif_3 12

//Constantes pour la course
#define nbTourCourse 27
#define dureeCourse 600

sem_t var_threads_en_att;   //semaphore pour controler l'acces des variables communes aux threads 
int threads_en_attente_secteur_1 = 0; //nombre  de threads dans le premier secteur
int threads_en_attente_secteur_2 = 0; //nombre  de threads dans le deuxième secteur
int threads_en_attente_secteur_3 = 0; //nombre  de threads dans le dernier secteur
sem_t secteur_1_mutex; //semaphore pour le premier secteur
sem_t secteur_2_mutex; //semaphore pour le deuxième secteur
sem_t secteur_3_mutex; //semaphore pour le dernier secteur
int drapeauJaune = 0; //variable qui annonce un drapeau jaune aux coureurs
int drapeauRouge = 0; //variable qui annonce un drapeau rouge aux coureurs
int securityCar = 0; //variable qui annonce la voiture de sécurité aux coureurs

/**
* Il y a un soucis quand deux threads cherchent à modifier deux variables en même temps.
* Donc on utilise les semaphores :
* Les semaphores permettent l'execution mutuelle (mecanisme de synchronisation)
* Une semaphore est en C une variable de type sem_t.
* Elle va nous servir de verrou, pour nous permettre de protéger des données.
**/

struct t_pilote {
    int numVoiture;
	char name[255];
	int sector[100][3];
    int tour[100];
    int tempsTotal;
    char auStand[100][3];
    int meilleurTour;
    int meilleurSecteur[3];
    int abandonneCourse;
    pthread_t piloteThread;
};

//Definition de tous les pilotes participants
struct t_pilote tb_coureur[NB_PARTICIPANTS]={
    {44,"L.HAMILTON",{{0}},{0},0,{{0}},0,{0},0,0},
	{6,"N.ROSBERG",{{0}},{0},0,{{0}},0,{0},0,0},
	{5,"S.VETTEL",{{0}},{0},0,{{0}},0,{0},0,0},
	{7,"R.RAIKKONEN",{{0}},{0},0,{{0}},0,{0},0,0},
	{3,"D.RICCIARDO",{{0}},{0},0,{{0}},0,{0},0,0},
	{33,"M.VERSTAPPEN",{{0}},{0},0,{{0}},0,{0},0,0},
	{19,"F.MASSA",{{0}},{0},0,{{0}},0,{0},0,0},
	{77,"V.BOTTAS",{{0}},{0},0,{{0}},0,{0},0,0},
	{11,"S.PEREZ",{{0}},{0},0,{{0}},0,{0},0,0},
	{27,"N.HULKENBERG",{{0}},{0},0,{{0}},0,{0},0,0},
	{26,"D.KVYAT",{{0}},{0},0,{{0}},0,{0},0,0},
	{55,"C.SAINZ",{{0}},{0},0,{{0}},0,{0},0,0},
	{14,"F.ALONSO",{{0}},{0},0,{{0}},0,{0},0,0},
	{22,"J.BUTTON",{{0}},{0},0,{{0}},0,{0},0,0},
	{9,"M.ERICSSON",{{0}},{0},0,{{0}},0,{0},0,0},
	{12,"F.NASR",{{0}},{0},0,{{0}},0,{0},0,0},
	{20,"K.MAGNUSSEN",{{0}},{0},0,{{0}},0,{0},0,0},
	{30,"J.PALMER",{{0}},{0},0,{{0}},0,{0},0,0},
	{8,"R.GROSJEAN",{{0}},{0},0,{{0}},0,{0},0,0},
	{21,"E.GUTIERREZ",{{0}},{0},0,{{0}},0,{0},0,0},
	{31,"E.OCON",{{0}},{0},0,{{0}},0,{0},0,0},
	{94,"P.WEHRLEIN",{{0}},{0},0,{{0}},0,{0},0,0}
};



//fonction random pour générer un temps aléatoire
int random_number(int min, int max){
    return rand()%(max-min+1)+min;
}

// Méthode de comparation pour les meilleurs temps (le tour le plus court en temps)
int compareMeilleurTour(const void *p1, const void *p2) { 
    const struct t_pilote *elem1 = p1;
    const struct t_pilote *elem2 = p2;

    if (elem1->meilleurTour == -1) return 1; //s'il n'a pas participé (valeur par défaut)
    if (elem2->meilleurTour == -1) return -1;
    if (elem1->meilleurTour < elem2->meilleurTour) return -1;
    if (elem1->meilleurTour > elem2->meilleurTour) return 1;
    return 0;
}

// Méthode de comparation pour les temps totaux de la course (sert pour donner le classement; le moins de temps, le mieux)
int compareTempsTotal(const void *p1, const void *p2) { 
    const struct t_pilote *elem1 = p1;
    const struct t_pilote *elem2 = p2;

    if (elem2->tempsTotal == -1) return -1; //s'il n'a pas participé (valeur par défaut)
    if (elem1->tempsTotal == -1) return 1;

    if (elem1->abandonneCourse == -1 && elem2->abandonneCourse != -1) return -1; //s'il a abandonné ou pas
    if (elem1->abandonneCourse != -1 && elem2->abandonneCourse == -1) return 1;

    if (elem1->abandonneCourse < elem2->abandonneCourse) return 1; //s'il a abandonné un ou plusieurs tours plus tot
    if (elem1->abandonneCourse > elem2->abandonneCourse) return -1;

    // Tri mineur sur le temps 
    if (elem1->tempsTotal < elem2->tempsTotal) return -1;
    if (elem1->tempsTotal > elem2->tempsTotal) return 1;
    return 0;
}

void drapeauBleu(int participants, int nbTour){
    if (nbTour == 0) return; //si on est toujours au premier tour, ne pas se gener

    struct t_pilote * tempArray = malloc(NB_PARTICIPANTS * sizeof(tb_coureur[0]));   //allouer la mémoire pour l'array contenant la copie de notre array contenant les pilotes
                                                                                    //ça permet d'éviter de modifier l'array originale avec le tri
    memcpy(tempArray, tb_coureur, NB_PARTICIPANTS * sizeof(tb_coureur[0]));          //copier l'array originale dans la nouvelle array (c'est pour en faire une copie profonde)

    qsort(tempArray, NB_PARTICIPANTS, sizeof(tempArray[0] ), compareTempsTotal);     //tri de la nouvelle array de pilotes

    //pour déterminer les dépassements, on cherche dans la liste de pilotes :
    // - deux pilotes dont aucun des deux n'ont abandonné la course
    // - deux pilotes dont le premier a un tour d'avance sur le second
    //   pour le calculer, suffit de comparer le temps total du premier pilote 
    //   et le temps total du deuxième pilote soustrait par le temps mis pour le tour précedent par le premier pilote
    for(int compteur_1=0; compteur_1<participants; compteur_1++){
        for(int compteur_2=participants-1; compteur_2>=0; compteur_2--){    //on parcours l'array à l'envers pour commencer par le pilote qui est la dernière place du classement
            if(compteur_1 != compteur_2){
                //la soustraction est ici
                if(tempArray[compteur_1].tempsTotal < tempArray[compteur_2].tempsTotal-tempArray[compteur_1].tour[nbTour] && \
                    tempArray[compteur_1].abandonneCourse == -1 && tempArray[compteur_2].abandonneCourse == -1){
                    printf("Drapeau bleu : %s double %s !\n\n",tempArray[compteur_1].name, tempArray[compteur_2].name);
                }
            }
        }
    }
    free(tempArray); //on libère la mémoire allouée
}

//fonction qui va afficher les meilleur temps (par secteurs et par tour)
void affiche_meilleur_temps(int participants){
    qsort(tb_coureur, NB_PARTICIPANTS, sizeof(tb_coureur[0] ), compareMeilleurTour); //tri de l'array de pilotes

    printf("\nMeilleurs temps par secteur et tour complet\n\n");
    printf("====================================================================\n");
    printf("|     ID   |      s1    |      s2    |      s3    |        Tour    |\n");
    printf("|==================================================================|\n");
    for(int compteur=0; compteur<participants; compteur++){
        if (tb_coureur[compteur].meilleurTour != -1){ //si différent de la valeur par défaut
            printf("|     %2d   |    %4d    |    %4d    |    %4d    |    %8d    |\n", \
                tb_coureur[compteur].numVoiture, \
                tb_coureur[compteur].meilleurSecteur[0], tb_coureur[compteur].meilleurSecteur[1], tb_coureur[compteur].meilleurSecteur[2], \
                tb_coureur[compteur].meilleurTour );
        }else{
            printf("|     %2d   |------------|------------|------------|----(Abandon)---|\n", tb_coureur[compteur].numVoiture);
        }
    }
    printf("====================================================================\n\n");
}

void affiche_temps_course(int participants, int nbTour){
    struct t_pilote * tempArray = malloc(NB_PARTICIPANTS * sizeof(tb_coureur[0]));   //allouer la mémoire pour l'array contenant la copie de notre array contenant les pilotes
                                                                                    //ça permet d'éviter de modifier l'array originale avec le tri
    memcpy(tempArray, tb_coureur, NB_PARTICIPANTS * sizeof(tb_coureur[0]));          //copier l'array originale dans la nouvelle array (c'est pour en faire une copie profonde)

    qsort(tempArray, NB_PARTICIPANTS, sizeof(tempArray[0] ), compareTempsTotal);     //tri de la nouvelle array de pilotes

    printf("\nTour n°%d.\n\n",nbTour+1);
    printf("====================================================================\n");
    printf("|     ID   |      s1    |      s2    |      s3    |        Tour    |\n");
    printf("|==================================================================|\n");
    for(int compteur=0; compteur<participants; compteur++){
        if (tempArray[compteur].tour[nbTour] != 0){ //si temps pour faire le tour est non-nul
            printf("|     %2d   |    %4d    |    %4d    |    %4d%c%c%c |    %8d    |\n", \
                tempArray[compteur].numVoiture, tempArray[compteur].sector[nbTour][0], tempArray[compteur].sector[nbTour][1], tempArray[compteur].sector[nbTour][2], \
                tempArray[compteur].auStand[nbTour][0], tempArray[compteur].auStand[nbTour][1], tempArray[compteur].auStand[nbTour][2], \
                tempArray[compteur].tour[nbTour] );
        }else{
            printf("|     %2d   |------------|------------|------------|----(Abandon)---|\n", tempArray[compteur].numVoiture);
        }
    }
    printf("====================================================================\n\n");
    free(tempArray); //on libère la mémoire allouée
}

void affiche_temps(int participants, int nbTours){
    qsort(tb_coureur, NB_PARTICIPANTS, sizeof(tb_coureur[0] ), compareMeilleurTour); //tri de l'array de pilotes

    for(int tour=0; tour<nbTours; tour++){
        printf("\nTour n°%d.\n\n",tour+1);
        printf("====================================================================\n");
        printf("|     ID   |      s1    |      s2    |      s3    |        Tour    |\n");
        printf("|==================================================================|\n");
        for(int compteur=0; compteur<participants; compteur++){
            if (tb_coureur[compteur].abandonneCourse == -1 || tb_coureur[compteur].abandonneCourse > tour){ //si le pilote n'a pas abandonné ou n'a pas abandonné à partir de ce tour-ci
                printf("|     %2d   |    %4d    |    %4d    |    %4d%c%c%c |    %8d    |\n", \
                    tb_coureur[compteur].numVoiture, tb_coureur[compteur].sector[tour][0], tb_coureur[compteur].sector[tour][1], tb_coureur[compteur].sector[tour][2], \
                    tb_coureur[compteur].auStand[tour][0], tb_coureur[compteur].auStand[tour][1], tb_coureur[compteur].auStand[tour][2], \
                    tb_coureur[compteur].tour[tour] );
            }else{
                printf("|     %2d   |------------|------------|------------|----(Abandon)---|\n", tb_coureur[compteur].numVoiture);
            }
        }
        printf("====================================================================\n\n");
    }
}

//fonction pour afficher tous les coureurs
void afficheCoureurs(){
    printf("	*********************************\n");
    printf("	*   ID 	 |	 Nom\n");
    printf("	*-------------------------------*\n");
    for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){
        printf("	*   %d 	 |	 %s\n",tb_coureur[compteur].numVoiture,tb_coureur[compteur].name);
    }
    printf("	*********************************\n");
}

//ici les threads

//Thread pour les pilotes lors de la grande course
void * thread_coureur_course(void* parameter){
    int indicePilote = ((int*)parameter)[0];    //parameter est un pointeur vers des éléments void 
    int duree = ((int*)parameter)[1];           //on va donc d'abord le cast en pointeur vers des éléments int
    int nbTours = ((int*)parameter)[2];         //et prendre les valeurs comme si c'était un array

    int dureeSecteurMax = ((duree/nbTours)-dureeStandMax)/3;    //calcul pour maintenir une certaine proportionalité
    int dureeSecteurMin = dureeSecteurMax/2;                    //pour les temps mis pour parcourir chaque secteur
    int visitesStand = 0;

    tb_coureur[indicePilote].abandonneCourse = -1;        //(ré)initialisation des valeurs pour le pilote
    tb_coureur[indicePilote].tempsTotal = 0;              //
    tb_coureur[indicePilote].meilleurSecteur[0] = -1;     //
    tb_coureur[indicePilote].meilleurSecteur[1] = -1;     //
    tb_coureur[indicePilote].meilleurSecteur[2] = -1;     //

    for(int tour = 0; tour<nbTours; tour++){

        int tempsTour = 0; //(ré)initialisation du temps mis pour faire un tour complet
        for(int secteur=0; secteur<3; secteur++){

            if ((securityCar == 1) && (tb_coureur[indicePilote].abandonneCourse == -1)){ //si la voiture de sécurité est sortie, 
                                                                                         //maintenir le temps selon la vitesse de la voiture de sécurité

                tb_coureur[indicePilote].sector[tour][secteur] = dureeSecteurMax*vitesseSecurityCar;
                tempsTour += dureeSecteurMax*vitesseSecurityCar;

            }else if ((drapeauJaune == 1) && (tb_coureur[indicePilote].abandonneCourse == -1)){

                tb_coureur[indicePilote].sector[tour][secteur] = dureeSecteurMax;   //on maintien la meme vitesse pour ne pas doubler les autres pilotes
                tempsTour += dureeSecteurMax;

            }else if((drapeauRouge == 1) && (tb_coureur[indicePilote].abandonneCourse == -1)){

                tempsTour = 0;  //on doit recommencer ce tour-ci à cause du drapeau rouge donc on annulle le temps mis pour ce tour 
                                //(pour ne pas qu'il soit marqué comme un nouveau meilleur temps)

            }else if((random_number(1,100) > CHANCE_ABANDON) && (tb_coureur[indicePilote].abandonneCourse == -1)) { //si on n'abandonne pas ou qu'on a pas déjà abandonné

                tb_coureur[indicePilote].sector[tour][secteur] = random_number(dureeSecteurMin,dureeSecteurMax);

                if (tb_coureur[indicePilote].meilleurSecteur[secteur] == -1){ //si meilleurSecteur pour ce secteur n'est pas encore mis
                    tb_coureur[indicePilote].meilleurSecteur[secteur] = tb_coureur[indicePilote].sector[tour][secteur];
                }else if (tb_coureur[indicePilote].meilleurSecteur[secteur] > tb_coureur[indicePilote].sector[tour][secteur]){ //prendre si plus petit
                    tb_coureur[indicePilote].meilleurSecteur[secteur] = tb_coureur[indicePilote].sector[tour][secteur];
                }

                if (secteur == 2 && random_number(1,100) <= CHANCE_STAND && visitesStandMax > visitesStand){  //si on visite le stand

                    tb_coureur[indicePilote].auStand[tour][0] = '('; //pour l'ffichage
                    tb_coureur[indicePilote].auStand[tour][1] = 'P'; //
                    tb_coureur[indicePilote].auStand[tour][2] = ')'; //
                    tb_coureur[indicePilote].sector[tour][2] += random_number(dureeStandMin,dureeStandMax); //delai à cause de la visite du stand
                    visitesStand += 1;

                }else{

                    tb_coureur[indicePilote].auStand[tour][0] = ' '; //pour l'affichage
                    tb_coureur[indicePilote].auStand[tour][1] = ' '; //
                    tb_coureur[indicePilote].auStand[tour][2] = ' '; //
                }

                tempsTour += tb_coureur[indicePilote].sector[tour][secteur]; //on ajoute le temps mis pour parcourir le secteur au temps total du tour
                tb_coureur[indicePilote].tempsTotal += tb_coureur[indicePilote].sector[tour][secteur]; //on l'ajoute aussi au temps total (en géneral)

            }else { //si le pilote abandonne
                if (tb_coureur[indicePilote].abandonneCourse == -1){ //on met la variable au tour auquel le pilote a abandonné 
                    tb_coureur[indicePilote].abandonneCourse = tour; //(c'est pour faciliter le classement)
                }
                tb_coureur[indicePilote].sector[tour][secteur] = 0; //on met tous les temps à 0

                tempsTour = 0; //

                tb_coureur[indicePilote].auStand[tour][0] = ' '; //pour l'affichage
                tb_coureur[indicePilote].auStand[tour][1] = ' '; //
                tb_coureur[indicePilote].auStand[tour][2] = ' '; //
            }

            if (secteur == 0){  //si on est au premier secteur
                sem_wait(&var_threads_en_att);
                threads_en_attente_secteur_1 += 1; //s'ajouter à la liste de threads en attente
                sem_post(&var_threads_en_att);

                sem_wait(&secteur_1_mutex); //attendre le signal du thread principal
                sem_post(&secteur_1_mutex);
            }else if (secteur == 1){  //si on est au deuxième secteur
                sem_wait(&var_threads_en_att);
                threads_en_attente_secteur_2 += 1; //s'ajouter à la liste de threads en attente
                sem_post(&var_threads_en_att);

                sem_wait(&secteur_2_mutex); //attendre le signal du thread principal
                sem_post(&secteur_2_mutex);
            }else {  //si on est au dernier secteur
                sem_wait(&var_threads_en_att);
                threads_en_attente_secteur_3 += 1; //s'ajouter à la liste de threads en attente
                sem_post(&var_threads_en_att);

                //comme on a fini le dernier secteur, on a fait un tour complet
                tb_coureur[indicePilote].tour[tour]=tempsTour; //on assigne le temps mis pour faire le tour

                if (tb_coureur[indicePilote].meilleurTour == -1 && tempsTour != 0){ //si meilleurTour n'est pas encore mis et tempsTour est non nul
                    tb_coureur[indicePilote].meilleurTour=tempsTour;
                }else if (tb_coureur[indicePilote].meilleurTour > tempsTour && tempsTour != 0){ // prendre tour si plus petit que meilleurTour et tempsTour est non nul
                    tb_coureur[indicePilote].meilleurTour=tempsTour;
                }
                if(drapeauRouge == 1){ //s'il y a un drapeau rouge, recommencer ce tour-ci
                    tour -= 1;
                }

                sem_wait(&secteur_3_mutex); //attendre le signal du thread principal
                sem_post(&secteur_3_mutex);
            }
        }    
    }

    free(parameter); //libérer la mémoire allouée
}

//Thread pour les pilotes hors de la grande course
void * thread_coureur(void* parameter){
    int indicePilote = ((int*)parameter)[0];         //parameter est un pointeur vers des éléments void 
    int dureeEssai = ((int*)parameter)[1];           //on va donc d'abord le cast en pointeur vers des éléments int
    int nbToursEssai = ((int*)parameter)[2];         //et prendre les valeurs comme si c'était un array

    int dureeSecteurMax = ((dureeEssai/nbToursEssai)-dureeStandMax)/3;    //calcul pour maintenir une certaine proportionalité
    int dureeSecteurMin = dureeSecteurMax/2;                              //pour les temps mis pour parcourir chaque secteur
    int visitesStand = 0;

    tb_coureur[indicePilote].meilleurTour=-1;           //(ré)initialisation des valeurs pour le pilote
    tb_coureur[indicePilote].abandonneCourse = -1;      //
    tb_coureur[indicePilote].tempsTotal=0;              //
    tb_coureur[indicePilote].meilleurSecteur[0] = -1;   //
    tb_coureur[indicePilote].meilleurSecteur[1] = -1;   //
    tb_coureur[indicePilote].meilleurSecteur[2] = -1;   //

    for(int tour = 0; tour<nbToursEssai; tour++){

        int tempsTour = 0; //(ré)initialisation du temps mis pour faire un tour complet
        if ((random_number(1,100) > CHANCE_ABANDON) && (tb_coureur[indicePilote].abandonneCourse == -1)){ //si on n'abandonne pas ou qu'on a pas déjà abandonné

            for(int secteur=0; secteur<3; secteur++){

                tb_coureur[indicePilote].sector[tour][secteur] = random_number(dureeSecteurMin,dureeSecteurMax);

                if (tb_coureur[indicePilote].meilleurSecteur[secteur] == -1){ //si meilleurSecteur pour ce secteur n'est pas encore mis
                    tb_coureur[indicePilote].meilleurSecteur[secteur] = tb_coureur[indicePilote].sector[tour][secteur];
                }else if (tb_coureur[indicePilote].meilleurSecteur[secteur] > tb_coureur[indicePilote].sector[tour][secteur]){ //prendre si plus petit
                    tb_coureur[indicePilote].meilleurSecteur[secteur] = tb_coureur[indicePilote].sector[tour][secteur];
                }

                if (secteur == 2 && random_number(1,100) <= CHANCE_STAND && visitesStandMax > visitesStand){  //si on visite le stand

                    tb_coureur[indicePilote].auStand[tour][0] = '('; //pour l'affichage
                    tb_coureur[indicePilote].auStand[tour][1] = 'P'; //
                    tb_coureur[indicePilote].auStand[tour][2] = ')'; //
                    tb_coureur[indicePilote].sector[tour][secteur] += random_number(dureeStandMin,dureeStandMax); //delai à cause de la visite du stand
                    visitesStand += 1;

                }else{

                    tb_coureur[indicePilote].auStand[tour][0] = ' '; //pour l'affichage
                    tb_coureur[indicePilote].auStand[tour][1] = ' '; //
                    tb_coureur[indicePilote].auStand[tour][2] = ' '; //
                }

                tempsTour += tb_coureur[indicePilote].sector[tour][secteur]; //on ajoute le temps mis pour parcourir le secteur au temps total du tour
                tb_coureur[indicePilote].tempsTotal += tb_coureur[indicePilote].sector[tour][secteur]; //on l'ajoute aussi au temps total (en géneral)
            }

            tb_coureur[indicePilote].tour[tour]=tempsTour;

            if (tb_coureur[indicePilote].meilleurTour == -1){ //si meilleurTour n'est pas encore mis
                tb_coureur[indicePilote].meilleurTour=tempsTour;
            }else if (tb_coureur[indicePilote].meilleurTour > tempsTour){ // prendre tour si plus petit que meilleurTour
                tb_coureur[indicePilote].meilleurTour=tempsTour;
            }

        }else {
            if (tb_coureur[indicePilote].abandonneCourse == -1){ //on met la variable au tour auquel le pilote a abandonné 
                tb_coureur[indicePilote].abandonneCourse = tour; //(c'est pour faciliter le classement) //(c'est pour faciliter le classement)
            }
            tb_coureur[indicePilote].sector[tour][0] = 0; //on met tous les temps à 0
            tb_coureur[indicePilote].sector[tour][1] = 0; //
            tb_coureur[indicePilote].sector[tour][2] = 0; //

            tb_coureur[indicePilote].tour[tour] = 0; //

            tb_coureur[indicePilote].auStand[tour][0] = ' '; //pour l'affichage
            tb_coureur[indicePilote].auStand[tour][1] = ' '; //
            tb_coureur[indicePilote].auStand[tour][2] = ' '; //
        }

    }

    free(parameter); //libérer la mémoire allouée
}

//fonction pour créer le thread d'un pilote avec sa fonction associée
int creer_thread_coureur(int indicePilote, int dureeEvent, int nbTours, int mode){

    int new_thread_pilote = 0; //variable pour obtenir le code d'erreur (s'il y en a un)
    pthread_t piloteThread; //nouveau thread

    int tempArray_1[3] = {indicePilote, dureeEvent*60, nbTours};    //créer une array avec nos paramètres
    int * tempArray_2 = malloc(3 * sizeof(int));                    //allouer la mémoire pour le array contenant la copie de nos paramètres
    memcpy(tempArray_2, tempArray_1, 3 * sizeof(int));              //le copier dans la nouvelle variable (c'est pour en faire une copie profonde)

    if (mode == 0){ //si essai ou qualification
        new_thread_pilote=pthread_create(&piloteThread,0,thread_coureur, tempArray_2 ); //pthread_create retourne un int
    }else { //si grande course
        new_thread_pilote=pthread_create(&piloteThread,0,thread_coureur_course, tempArray_2 );
    }
    if(new_thread_pilote!=0){ //si code d'erreur lors de la création du thread
        printf("Create thread failed error: %d\n",new_thread_pilote);
        exit(1);
    }
    tb_coureur[indicePilote].piloteThread = piloteThread; //ajouter le thread  au struct du pilote (pour join)
}

//fonction pour les qualifications
void qualifications_course(){
    int choix_qualif;
    printf("-----------------------------------------------------\n");
    printf("-              Qualifications & Course              -\n");
    printf("-----------------------------------------------------\n");

    while(choix_qualif < 1 || choix_qualif >2){

        printf("Choissiez une option pour continuer :\n");
        printf(" 1. Commencer la première qualification.\n");
        printf(" 2. Ne pas commencer les qualifications, fin du Grand Prix de Forumule 1\n");
        printf(" Votre choix ? ");
        scanf("%d",&choix_qualif);

        switch(choix_qualif){
            
            case 1:
                printf("La première qualification de ce Samedi après-midi va commencer...\n");
                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //on va créer tous les threads des pilotes
                    creer_thread_coureur(compteur, dureeQualif_1, nbTourQualif_1, 0);
                }
                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //attendre que tous les threads soient fini
                    pthread_join(tb_coureur[compteur].piloteThread, NULL);
                }
                affiche_temps(NB_PARTICIPANTS, nbTourQualif_1);
                printf("%d tours effecuté lors de cette première qualification.\nQualification terminée...\n",nbTourQualif_1);

                affiche_meilleur_temps(NB_PARTICIPANTS);

                getchar();

                printf("La deuxième qualification de ce Samedi après-midi va commencer...\n");

                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //on va (ré)initialiser les variables de pilotes
                    tb_coureur[compteur].meilleurTour=-1;                 //(comme ça les non-participants ne sont pas classés)
                    tb_coureur[compteur].tempsTotal=-1;
                }
                for(int compteur=0; compteur<NB_PARTICIPANTSQualif_2; compteur++){     //on va créer tous les threads des pilotes
                    creer_thread_coureur(compteur, dureeQualif_2, nbTourQualif_2, 0); //avec seulement le nombre de pilotes qualifiés
                }
                for(int compteur=0; compteur<NB_PARTICIPANTSQualif_2; compteur++){ //attendre que tous les threads soient fini
                    pthread_join(tb_coureur[compteur].piloteThread, NULL);
                }
                affiche_temps(NB_PARTICIPANTSQualif_2, nbTourQualif_2);
                printf("%d tours effecuté lors de cette première qualification.\nQualification terminée...\n",nbTourQualif_2);

                affiche_meilleur_temps(NB_PARTICIPANTSQualif_2);

                getchar();

                printf("La dernière qualification de ce Samedi après-midi va commencer...\n");

                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //on va (ré)initialiser les variables de pilotes
                    tb_coureur[compteur].meilleurTour=-1;                 //(comme ça les non-participants ne sont pas classés)
                    tb_coureur[compteur].tempsTotal=-1;
                }
                for(int compteur=0; compteur<NB_PARTICIPANTSQualif_3; compteur++){     //on va créer tous les threads des pilotes
                    creer_thread_coureur(compteur, dureeQualif_3, nbTourQualif_3, 0); //avec seulement le nombre de pilotes qualifiés
                }
                for(int compteur=0; compteur<NB_PARTICIPANTSQualif_3; compteur++){ //attendre que tous les threads soient fini
                    pthread_join(tb_coureur[compteur].piloteThread, NULL);
                }
                affiche_temps(NB_PARTICIPANTSQualif_3, nbTourQualif_3);
                printf("%d tours effecuté lors de cette première qualification.\nQualification terminée...\n",nbTourQualif_3);

                affiche_meilleur_temps(NB_PARTICIPANTSQualif_3);

                getchar();

                printf("La Course du Grand Prix de Forumule 1 de ce Dimanche va commencer...\n");

                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //on va (ré)initialiser les variables de pilotes
                    tb_coureur[compteur].meilleurTour=-1;                 //(comme ça les non-participants ne sont pas classés)
                    tb_coureur[compteur].tempsTotal=-1;
                }
                
                sem_wait(&secteur_1_mutex);     //on va réserver cette sémaphore avant de créer les threads
                                                //pour ne pas que les pilotes dépassent le premier secteur
                                                //avant qu'on le souhaite

                for(int compteur=0; compteur<NB_PARTICIPANTSQualif_3; compteur++){ //on va créer tous les threads des pilotes
                    creer_thread_coureur(compteur, dureeCourse, nbTourCourse, 1); //avec seulement le nombre de pilotes qualifiés
                }

                int milliseconds = 160;                         //struct pour pouvoir mettre le thread en 
                struct timespec ts;                             //sleep pour une durée en millisecondes
                ts.tv_sec = milliseconds / 1000;                //
                ts.tv_nsec = (milliseconds % 1000) * 1000000;   //

                int sem_value;  //pour stocker la valeur de la variable commune (voir après)
                for(int tour = 0; tour<nbTourCourse; tour++){
                    
                    sem_wait(&var_threads_en_att);              //on va s'assurer que personne n'est en train
                    sem_value = threads_en_attente_secteur_1;   //d'écrire dessus avant de la lire
                    sem_post(&var_threads_en_att);              //
                    
                    sem_wait(&secteur_2_mutex); //on va réserver cette sémaphore
                                                //pour ne pas que les pilotes dépassent le deuxième secteur
                                                //avant qu'on le souhaite

                    while(sem_value < NB_PARTICIPANTSQualif_3){ //tant que tous les pilotes n'ont pas fini de parcourir le premier secteur
                        nanosleep(&ts, NULL);
                        sem_wait(&var_threads_en_att);
                        sem_value = threads_en_attente_secteur_1;
                        sem_post(&var_threads_en_att);
                    }

                    threads_en_attente_secteur_1 = 0; //ils ont fini, on peut réintitialiser la variable pour le prochain tour

                    //événements random (drapeaux, voiture de sécurité) pour le prochain secteur
                    if (securityCar == 1 && (random_number(1,100)) <= chanceSecurityCarRentre){ //si la voiture de sécurité est dehors
                        printf("La voiture de sécurité va rentrer !\n\n");
                        securityCar = 0;
                    }else if ((random_number(1,100)) <= chanceSecurityCar && securityCar == 0){ //si la voiture de sécurité n'est pas dehors
                        printf("La voiture de sécurité est sortie !\n\n");
                        drapeauJaune = 0;
                        drapeauRouge = 0;
                        securityCar = 1;
                    }else if ((random_number(1,100)) <= chanceDrapeauJaune && securityCar == 0){ //si on hisse le drapeau jaune
                        printf("Drapeau Jaune au Secteur 2 !\n\n");
                        drapeauJaune = 1;
                        drapeauRouge = 0;
                        securityCar = 0;
                    }else if((random_number(1,100)) <= chanceDrapeauRouge && securityCar == 0){ //si on hisse le drapeau rouge
                        drapeauJaune = 0;
                        drapeauRouge = 1;
                        securityCar = 0;
                        tour -= 1;  //on doit recommencer ce tour-ci
                    }else { //rien d'intéressant se passe
                        drapeauJaune = 0;
                        drapeauRouge = 0;
                    }

                    sem_post(&secteur_1_mutex); //on libère la sémaphore du premier secteur pour que les pilotes
                                                //puissent commencer à parcourir le deuxième secteur

                    sem_wait(&var_threads_en_att);              //on va s'assurer que personne n'est en train
                    sem_value = threads_en_attente_secteur_2;   //d'écrire dessus avant de la lire
                    sem_post(&var_threads_en_att);              //

                    sem_wait(&secteur_3_mutex); //on va réserver cette sémaphore
                                                //pour ne pas que les pilotes dépassent le dernier secteur
                                                //avant qu'on le souhaite

                    while(sem_value < NB_PARTICIPANTSQualif_3){ //tant que tous les pilotes n'ont pas fini de parcourir le deuxième secteur
                        nanosleep(&ts, NULL);
                        sem_wait(&var_threads_en_att);
                        sem_value = threads_en_attente_secteur_2;
                        sem_post(&var_threads_en_att);
                    }

                    threads_en_attente_secteur_2 = 0; //ils ont fini, on peut réintitialiser la variable pour le prochain tour

                    //événements random (drapeaux, voiture de sécurité) pour le prochain secteur
                    if (drapeauRouge == 0){
                        if (securityCar == 1 && (random_number(1,100)) <= chanceSecurityCarRentre){ //si la voiture de sécurité est dehors
                            printf("La voiture de sécurité va rentrer !\n\n");
                            securityCar = 0;
                        }else if ((random_number(1,100)) <= chanceSecurityCar && securityCar == 0){ //si la voiture de sécurité n'est pas dehors
                            printf("La voiture de sécurité est sortie !\n\n");
                            drapeauJaune = 0;
                            drapeauRouge = 0;
                            securityCar = 1;
                        }else if ((random_number(1,100)) <= chanceDrapeauJaune && securityCar == 0){ //si on hisse le drapeau jaune
                            printf("Drapeau Jaune au Secteur 3 !\n\n");
                            drapeauJaune = 1;
                            drapeauRouge = 0;
                            securityCar = 0;
                        }else if((random_number(1,100)) <= chanceDrapeauRouge && securityCar == 0){ //si on hisse le drapeau rouge
                            drapeauJaune = 0;
                            drapeauRouge = 1;
                            securityCar = 0;
                            tour -= 1;  //on doit recommencer ce tour-ci
                        }else { //rien d'intéressant se passe
                            drapeauJaune = 0;
                            drapeauRouge = 0;
                        }
                    }

                    sem_post(&secteur_2_mutex); //on libère la sémaphore du deuxième secteur pour que les pilotes
                                                //puissent commencer à parcourir le dernier secteur

                    sem_wait(&var_threads_en_att);              //on va s'assurer que personne n'est en train
                    sem_value = threads_en_attente_secteur_3;   //d'écrire dessus avant de la lire
                    sem_post(&var_threads_en_att);              //

                    sem_wait(&secteur_1_mutex); //on va réserver cette sémaphore
                                                //pour ne pas que les pilotes dépassent le premier secteur du prochain tour
                                                //avant qu'on le souhaite

                    while(sem_value < NB_PARTICIPANTSQualif_3){ //tant que tous les pilotes n'ont pas fini de parcourir le dernier secteur
                        nanosleep(&ts, NULL);
                        sem_wait(&var_threads_en_att);
                        sem_value = threads_en_attente_secteur_3;
                        sem_post(&var_threads_en_att);
                    }

                    threads_en_attente_secteur_3 = 0; //ils ont fini, on peut réintitialiser la variable pour le prochain tour

                    //événements random (drapeaux, voiture de sécurité) pour le premier secteur du prochain tour
                    if (drapeauRouge == 0){
                        drapeauBleu(NB_PARTICIPANTSQualif_3, tour);
                        if (securityCar == 1 && (random_number(1,100)) <= chanceSecurityCarRentre){ //si la voiture de sécurité est dehors
                            printf("La voiture de sécurité va rentrer !\n\n");
                            securityCar = 0;
                        }else if ((random_number(1,100)) <= chanceSecurityCar && securityCar == 0){ //si la voiture de sécurité n'est pas dehors
                            drapeauJaune = 0;
                            drapeauRouge = 0;
                            securityCar = 1;
                        }else if ((random_number(1,100)) <= chanceDrapeauJaune && securityCar == 0){ //si on hisse le drapeau jaune
                            drapeauJaune = 1;
                            drapeauRouge = 0;
                            securityCar = 0;
                        }else if((random_number(1,100)) <= chanceDrapeauRouge && securityCar == 0){ //si on hisse le drapeau rouge
                            drapeauJaune = 0;
                            drapeauRouge = 1;
                            securityCar = 0;
                            tour -= 1;  //on doit recommencer ce tour-ci
                        }else { //rien d'intéressant se passe
                            drapeauJaune = 0;
                            drapeauRouge = 0;
                        }
                    }

                    sem_post(&secteur_3_mutex); //on libère la sémaphore du dernier secteur pour que les pilotes
                                                //puissent commencer à parcourir le premier secteur du prochain tour

                    if (drapeauRouge == 0){ //si drapeau rouge, ne rien afficher
                        affiche_temps_course(NB_PARTICIPANTSQualif_3, tour);
                    }

                    //on fait cet affichage APRES celui des temps et du classement parce que c'est plus clair comme ça
                    //vu que les événements randoms n'affectent que le prochain tour de toute façon
                    if (drapeauRouge == 1) printf("Drapeau Rouge !\n\n");
                    else if (drapeauJaune == 1 && tour != nbTourCourse-1) printf("Drapeau Jaune au Secteur 1 !\n\n");
                    else if (securityCar == 1 && tour != nbTourCourse-1) printf("La voiture de sécurité est sortie !\n\n");
                    
                }
                printf("%d tours effecuté lors de cette course.\nCourse terminée...\n\n",nbTourCourse);

                affiche_meilleur_temps(NB_PARTICIPANTSQualif_3); //afficher les meilleurs temps pour chaque tour/secteur

                getchar();

                break;

            case 2:
                printf("Le Grand Prix de Forumule 1 n'as pas commencé, fin de la course...\n");
                exit(0);
                break;

            default:
                printf("Erreur...Veuillez entrer 1 ou 2 !\n");
                break;
        }
    }
}

//fonction pour les essais libres
void essai_libre(){
	int choix_essai;
	printf("-----------------------------------------------------\n");
	printf("- 	                  Essais libres                 -\n");
	printf("-----------------------------------------------------\n");

	while(choix_essai < 1 || choix_essai >2){

		printf("Choissiez une option pour continuer :\n");
		printf(" 1. Commencer le premier essai libre.\n");
        printf(" 2. Ne pas commencer l'essai libre, commencer les qualifications\n");
		printf(" 3. Ne pas commencer l'essai libre, fin du Grand Prix de Forumule 1\n");
		printf(" Votre choix ? ");
		scanf("%d",&choix_essai);

        switch(choix_essai){
            
            case 1:
                printf("L'essai de ce Vendredi matin va commencer...\n");

                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){
                    creer_thread_coureur(compteur, dureeEssai_1, nbTourEssai_1, 0);
                }
                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //attendre que tous les threads soient fini
                    pthread_join(tb_coureur[compteur].piloteThread, NULL);
                }
                affiche_temps(NB_PARTICIPANTS, nbTourEssai_1);
                printf("%d tours effecuté lors de ce premier essai.\nEssai libre terminé...\n\n",nbTourEssai_1);

                getchar();

                printf("L'essai de ce Vendredi après-midi va commencer...\n");

                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){
                    creer_thread_coureur(compteur, dureeEssai_2, nbTourEssai_2, 0);
                }
                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //attendre que tous les threads soient fini
                    pthread_join(tb_coureur[compteur].piloteThread, NULL);
                }
                affiche_temps(NB_PARTICIPANTS, nbTourEssai_2);
                printf("%d tours effecuté lors de ce deuxième essai.\nEssai libre terminé...\n\n",nbTourEssai_2);

                affiche_meilleur_temps(NB_PARTICIPANTS);

                getchar();

                printf("L'essai de ce Samedi matin va commencer...\n");

                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){
                    creer_thread_coureur(compteur, dureeEssai_3, nbTourEssai_3, 0);
                }
                for(int compteur=0; compteur<NB_PARTICIPANTS; compteur++){ //attendre que tous les threads soient fini
                    pthread_join(tb_coureur[compteur].piloteThread, NULL);
                }
                affiche_temps(NB_PARTICIPANTS, nbTourEssai_3);
                printf("%d tours effecuté lors de ce dernier essai.\nEssai libre terminé...\n\n",nbTourEssai_3);

                affiche_meilleur_temps(NB_PARTICIPANTS);

                getchar();

                qualifications_course();
                break;

            case 2:
                qualifications_course();
                break;

            case 3:
                printf("Le Grand Prix de Forumule 1 n'as pas commencé, fin de la course...\n");
                exit(0);
                break;

            default:
                printf("Erreur...Veuillez entrer 1, 2 ou 3 !\n");
                break;
        }
    }
}

void menu_depart(){//fonction de départ
	int choix, i;

	srand(time(NULL));

	printf("\n\n\nProjet OS Pratique présenté par Grégory - Amine - Joel - Nadia\n\n\n");

	printf("\n***************************************************\n");
	printf(" *           Grand Prix de Forumule 1              *\n");
	printf(" *              Lieu : Nürburgring                 *\n");
	printf(" *               Pays : Allemagne                  *\n");
	printf(" *                   * * * *                       *\n");
	printf(" *             05/02/16 - 07/02/17                 *\n");
	printf(" ***************************************************\n\n");

	printf(" ---------------------------------------------------\n");
	printf(" -       -- Présentation de la course --           -\n");
	printf(" -   (22 voitures engagées dans le grand prix)     -\n");
	printf(" ---------------------------------------------------\n\n");

	printf(" -      Programme de la course du week-end         -\n\n");

	printf(" -       05/02/17 PM : essai libre (1h30)          -\n\n");
	printf(" -        06/02/17 AM : essai libre (1h)           -\n\n");

	printf(" -  06/02/17 PM : Qualification 1 (18 minutes)     -\n");
	printf(" -  06/02/17 PM : Qualification 2 (15 minutes)     -\n");
	printf(" -  06/02/17 PM : Qualification 3 (12 minutes)     -\n\n");
	printf(" -      07/02/17 PM : La course (x minutes)        -\n");
	printf(" ---------------------------------------------------\n\n");
	printf("\n");
	printf("\n");

	while(choix <1 || choix >2){
		printf("  Choissiez une option pour continuer :\n");
		printf("    1. Démarrer le Grand Prix de Forumule 1.\n");
		printf("    2. Quitter le Programme.\n");

		printf("  Votre choix ? ");
		scanf("%d",&choix);

		switch(choix){
			case 1:
                printf("Vous allez commencer la course\n");
                int AffichCour;
                printf("Voulez vous afficher tous les Particpants de la course ?\n");
                printf(" 1. OUI - 2. NON\n");
                printf("Votre choix ?");
                scanf("%d",&AffichCour);
    			switch(AffichCour){//switch pour afficher les coureurs ou non
    				case 1:
                        afficheCoureurs();
                        essai_libre();
                        break;
                    case 2:
                        printf("La liste des coureurs n'est pas affichée...\n");
                        essai_libre();
                        break;
                    default:
                        printf("Erreur! Veuillez choisir entre '1' et '2'...");
        					}//fin du switch affichCoureurs ou non
        				break;//fin du choix 1 pour commencer la course
            case 2:
                printf("Vous allez quitter le Programme...\n");
                exit(0);
                break;
            default:
                printf("Choix incorrect...");
                break;
        }
    }
}

 int main(){
    //initialisation des sémaphores
    sem_init(&var_threads_en_att, 0, 1);
    sem_init(&secteur_1_mutex, 0, 1);
    sem_init(&secteur_2_mutex, 0, 1);
    sem_init(&secteur_3_mutex, 0, 1);

    menu_depart();
}
