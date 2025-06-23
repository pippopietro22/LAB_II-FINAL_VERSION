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

int thrd_insert(void *data){
    //Casto il puntatore agli argomenti
    thrd_data_insert *args = (thrd_data_insert*)data;

    //Variabile tempo per documentare le azioni sul file LOG
    char time_now[BUFF];

    //Analizzo la stringa estratta dalla message_queue
    emergency_request_t *analised = analisi_richiesta(args->emrg, args->flog);

    //Se il risultato è negativo, dealloco lo spazio per gli argomenti ed esco
    if(!analised){
        printf("Emergenza scartata, formato errato.\n");
        fflush(stdout);
        free(args);
        return thrd_success;
    }

    //Si valida la richiesta di emergenza
    emergency_t *valid = validazione_richiesta(analised, args);

    //Si dealloca lo spazio per l'emergenza analizzata
    destroy_analisi(analised);

    //Se il risultato è negativo, dealloco lo spazio per la struttura emergency_request_t generata e gli argomenti ed esco
    if(!valid){ 
        printf("Emergenza scartata, dati errati.\n");
        fflush(stdout);
        free(args);
        return thrd_success;
    }

    //Si compiano localmente (sull stack del thread) le informazioni da stampare sul file log
    emergency_t current = *valid;

    //Inizializzo una variabile tempo per calocalre il delay dell'emergenza prima di inserirla nella coda
    time_t now = time(NULL);
    int delay = valid->timestamp - now;
    
    //Se il delay è maggiore di 0 si esegue un'attesa
    if(delay > 0){
        struct timespec attesa = {.tv_sec = delay, .tv_nsec = 0};
        thrd_sleep(&attesa, NULL);
    }

    //INTERRUZIONE PROGRAMMA
    if(!atomic_load(&keep_running)){
        destroy_emrg_validata(valid);
        free(args);
        atomic_fetch_sub(&thrd_attivi,1);
        return thrd_success;
    }

    //Dopo l'eventuale attesa, si inserisce l'emergenza validata nella lista delle emergenze
    mtx_lock(&lista_mtx);
        add_emrg(lista_emergenze, valid);
    mtx_unlock(&lista_mtx);

    //Si svegliano tutti i thrd in attesa di emergenze
    cnd_broadcast(&lista_cnd);

    #ifdef DEBUG
        printf("THRD INSERT ");
        fflush(stdout);
    #endif
    printf("EMERGENZA %d_%s inserita.\n",current.id, current.type->emergency_desc);
    fflush(stdout);

    //Scrittura sul file LOG per mezzo di una mutex
    mtx_lock(&log_mtx);
        tempo_corrente(time_now);
        FPRINT(fprintf(args->flog,"[%s] [%d_%s] [MESSAGE_QUEUE] <Nuova emergenza inserita>\n", \
                time_now, current.id, current.type->emergency_desc), args->flog, "Errore durante scrittura file LOG da thrd_insert().\n");
    mtx_unlock(&log_mtx);

    //Se tutto è andato a compimento, si dealloca lo spazio per l'emergency_request_t e gli argomenti.
    //Non si dealloca l'emergenza validata; verrà deallocata dal thrd che la estrarrà dalla lista o da una funzione ausigliaria in caso di timeout
    free(args);

    atomic_fetch_sub(&thrd_attivi,1);
    return thrd_success;
}