#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<threads.h>
#include<stdatomic.h>

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


int rescuer_on_scene(void *data){
    thrd_data_socc *args = (thrd_data_socc*)data;

    char time_now[BUFF];

    //Gestione emergenza
    if(atomic_load(&keep_running)){
        struct timespec attesa = {.tv_sec = args->time_to_manage, .tv_nsec = 0};
        thrd_sleep(&attesa,NULL);
    }

    //Setto lo stato del soccorritore per il ritorno alla base e documento sul file LOG
    mtx_lock(&rescuer_mtx);
        args->digTwin->status = RETURNING_TO_BASE;

        #ifdef DEBUG
            printf("SOCCORRITORE %s_%d TORNA ALLA BASE\n", args->digTwin->rescuer->rescuer_type_name, args->digTwin->id);
            fflush(stdout);
        #endif

        //Documento sul file log
        if(atomic_load(&keep_running)){
            mtx_lock(&log_mtx);
                tempo_corrente(time_now);
                FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore sta tornando alla base>\n",time_now, \
                                args->digTwin->rescuer->rescuer_type_name, args->digTwin->id), args->flog,"Erroredurante scrittura file LOG da rescuer_return().\n");
            mtx_unlock(&log_mtx);
        }
    mtx_unlock(&rescuer_mtx);

    thrd_t soccorso;
    atomic_fetch_add(&thrd_attivi,1);
    thrd_create(&soccorso,rescuers_return,args);
    thrd_detach(soccorso);

    return thrd_success;
}