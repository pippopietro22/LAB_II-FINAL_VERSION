#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<threads.h>
#include<stdatomic.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Questa funzione utilizza variabili globali definite in main.c che vengono riportate qui come extern

extern int soccorritori_liberi[RESCUER_TYPES]; //Variabili accedibile da qualsiasi thread per controllare risorse disponibili (tramite mutex)

extern mtx_t rescuer_mtx;  //variabili mtx per risorse
extern mtx_t log_mtx;      //mtx per accesso a file LOG
extern cnd_t rescuer_cnd;  //cnd per attesa ripristino risorse

extern atomic_int keep_running; //Settata a 1: i thrd continuano ad operare. Settata a 0: i thrd terminano
extern atomic_int emrg_gestite; //Indica quante emergenze vengono prese in carico da un thrd per essere gestite.
extern atomic_int thrd_attivi;  //Se si genera un thrd con detach si aumenta di 1. Prima che il thrd termini, si decrementa a -1.
                                //Serve a capire quando tutti i thrd (su cui non si fa una join) terminano per evitare memory leaks.


//Funzione di gestione individuale dei soccorritori per il ritorno
int rescuers_return(void *data){
    //Casto il puntatore agli argomenti
    thrd_data_socc *args = (thrd_data_socc*)data;

    //Variabile che indica data e ora
    char time_now[BUFF];

    //Ricavo dati per il tragitto di ritorno
    int tempo_ritorno;
    //Devo utilizzare una mtx per l'accesso ai dati del gemello digitale per evitare sovrapposizioni con altri thrd
    mtx_lock(&rescuer_mtx);
        tempo_ritorno = distanza_Manhattan(args->digTwin->x, args->digTwin->y, args->digTwin->rescuer->x, args->digTwin->rescuer->y) \
                        /args->digTwin->rescuer->speed;
    mtx_unlock(&rescuer_mtx);

    //Ritorno alla base: simulo l'attesa, ammesso che non si sia terminato il programma (keep_running = 1)
    if(atomic_load(&keep_running)){
        struct timespec attesa = {.tv_sec = tempo_ritorno, .tv_nsec = 0};
        thrd_sleep(&attesa,NULL);
    }

    //Aggiorni i dati dei soccorritori tornati disponibili
    mtx_lock(&rescuer_mtx);
        //Aggiorno coordinate e stato
        args->digTwin->x = args->digTwin->rescuer->x;
        args->digTwin->y = args->digTwin->rescuer->y;
        args->digTwin->status = IDLE;

        //Segno una risorsa ripristinata
        soccorritori_liberi[args->digTwin->idType]++;

        //Messaggio di DEBUG
        #ifdef DEBUG
            printf("SOCCORRITORE %s_%d E' TORNATO ALLA BASE\n",args->digTwin->rescuer->rescuer_type_name, args->digTwin->id);
            fflush(stdout);
        #endif

        //Documento sul file log
        mtx_lock(&log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore tornato alla base>\n",time_now, \
                            args->digTwin->rescuer->rescuer_type_name,args->digTwin->id), args->flog,"Erroredurante scrittura file LOG da rescuer_return().\n");
        mtx_unlock(&log_mtx);

        //Messaggio di DEBUG che stampa tutte le risorse correnti disponibili
        #ifdef DEBUG
            printf("SOCCORRITORI: ");
            for(int i = 0; i < RESCUER_TYPES; i++){
                printf("%d ",soccorritori_liberi[i]);
            }
            printf("\n");
            fflush(stdout);
        #endif
    mtx_unlock(&rescuer_mtx);

    //Messaggio di DEBUG
    #ifdef DEBUG
        printf("thrd con in carico un'emergenza: %d\n",atomic_load(&emrg_gestite));
        fflush(stdout);
    #endif

    //Sveglio tutti i thrd in attesa di risorse
    cnd_broadcast(&rescuer_cnd);

    //Controllo se non ci sono più emergenze da svolgerer e che tutti i soccorritori siano di nuovo liberi.
    //In questo caso stampo un messaggio a schermo
    controllo_situazione(args->tipiSoccorritori);

    //Dealloco lo spazio per gli argomenti del thrd
    free(args);

    //Segnalo che questo thrd (con detach) è terminato
    atomic_fetch_sub(&thrd_attivi,1);
    return thrd_success;
}