#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include<mqueue.h>
#include<errno.h>
#include<math.h>
#include <threads.h>
#include <stdatomic.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

extern lista_t *lista_emergenze;
extern int soccorritori_liberi[RESCUER_TYPES]; //Variabili accedibile da qualsiasi thread per controllare risorse disponibili (tramite mutex)

extern mtx_t rescuer_mtx;  //variabili mtx per risorse
extern mtx_t lista_mtx;    //mtx per accesso lista emergenze
extern mtx_t log_mtx;      //mtx per accesso a file LOG
extern cnd_t rescuer_cnd;  //cnd per attesa ripristino risorse
extern cnd_t lista_cnd;    //cnd per attesa lista emergenze

extern atomic_int keep_running;
extern atomic_int id_emrg;
extern atomic_int thrd_attivi;


//Funzione di gestione individuale dei soccorritori
int rescuers_return(void *data){
    thrd_data_socc *args = (thrd_data_socc*)data;
    char time_now[BUFF];

    //Ricavo dati per il tragitto di ritorno
    int tempo_ritorno;
    mtx_lock(&rescuer_mtx);
        tempo_ritorno = distanza_Manhattan(args->digTwin->x, args->digTwin->y, args->digTwin->rescuer->x, args->digTwin->rescuer->y) \
                        /args->digTwin->rescuer->speed;
    mtx_unlock(&rescuer_mtx);

    //Ritorno alla base
    if(atomic_load(&keep_running)){
        struct timespec attesa = {.tv_sec = tempo_ritorno, .tv_nsec = 0};
        thrd_sleep(&attesa,NULL);
    }

    //Aggiorni i dati dei soccorritori tornati disponibili
    mtx_lock(&rescuer_mtx);
        args->digTwin->x = args->digTwin->rescuer->x;
        args->digTwin->y = args->digTwin->rescuer->y;
        args->digTwin->status = IDLE;
        soccorritori_liberi[args->digTwin->idType]++;

        #ifdef DEBUG
            printf("SOCCORRITORE %s_%d E' TORNATO ALLA BASE\n",args->digTwin->rescuer->rescuer_type_name, args->digTwin->id);
            fflush(stdout);
        #endif

        //Documento sul file log
        if(atomic_load(&keep_running)){
            mtx_lock(&log_mtx);
                tempo_corrente(time_now);
                FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore tornato alla base>\n",time_now, \
                                args->digTwin->rescuer->rescuer_type_name,args->digTwin->id), args->flog,"Erroredurante scrittura file LOG da rescuer_return().\n");
            mtx_unlock(&log_mtx);
        }

        #ifdef DEBUG
            printf("SOCCORRITORI: ");
            for(int i = 0; i < RESCUER_TYPES; i++){
                printf("%d ",soccorritori_liberi[i]);
            }
            printf("\n");
            fflush(stdout);
        #endif
    mtx_unlock(&rescuer_mtx);

    //Sveglio tutti i thrd in attesa di risorse
    cnd_broadcast(&rescuer_cnd);

    controllo_situazione(args->tipiSoccorritori);

    free(args);

    atomic_fetch_sub(&thrd_attivi,1);
    return thrd_success;
}