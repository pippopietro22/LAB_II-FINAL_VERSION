#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<threads.h>
#include<stdatomic.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Questa funzione utilizza variabili globali definite in main.c che vengono riportate qui come extern

extern mtx_t rescuer_mtx;  //variabili mtx per risorse
extern mtx_t log_mtx;      //mtx per accesso a logFile.txt

extern atomic_int keep_running; //Settata a 1: i thrd continuano ad operare. Settata a 0: i thrd terminano
extern atomic_int thrd_attivi;  //Se si genera un thrd con detach si aumenta di 1. Prima che il thrd termini, si decrementa a -1.
                                //Serve a capire quando tutti i thrd (su cui non si fa una join) terminano per evitare memory leaks


int rescuer_on_scene(void *data){
    //Casto il puntatore agli argomenti
    thrd_data_socc *args = (thrd_data_socc*)data;

    //Variabile che riporta ora e data
    char time_now[BUFF];

    //Gestione emergenza: se il programma non è stato terminato (keep_running = 1) si simula l'attesa per lo svolgimento dell'emergenza
    if(atomic_load(&keep_running)){
        struct timespec attesa = {.tv_sec = args->time_to_manage, .tv_nsec = 0};
        thrd_sleep(&attesa,NULL);
    }

    //Setto lo stato del soccorritore per il ritorno alla base e documento sul file LOG
    mtx_lock(&rescuer_mtx);
        //Modifico lo stato
        args->digTwin->status = RETURNING_TO_BASE;

        //Messaggio di DEBUG
        #ifdef DEBUG
            printf("SOCCORRITORE %s_%d TORNA ALLA BASE\n", args->digTwin->rescuer->rescuer_type_name, args->digTwin->id);
            fflush(stdout);
        #endif

        //Documento sul file log
        mtx_lock(&log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore sta tornando alla base>\n",time_now, \
                            args->digTwin->rescuer->rescuer_type_name, args->digTwin->id), args->flog,"Erroredurante scrittura file LOG da rescuer_return().\n");
        mtx_unlock(&log_mtx);
    mtx_unlock(&rescuer_mtx);

    //Inizializzo un nuovo thrd per il ritorno alla base del soccorritore perché questo thrd termina con una join
    thrd_t soccorso;

    //Indico che un thrd con detach sta per essere inizializzato
    atomic_fetch_add(&thrd_attivi,1);

    thrd_create(&soccorso,rescuers_return,args);
    thrd_detach(soccorso);

    return thrd_success;
}