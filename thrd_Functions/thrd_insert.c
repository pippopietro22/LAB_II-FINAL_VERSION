#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<threads.h>
#include<stdatomic.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Questa funzione utilizza variabili globali definite in main.c che vengono riportate qui come extern

extern lista_t *lista_emergenze;   //Puntatore globale (accessibile in ogni parte del codice) alla lista emergenze (tramite mutex)

extern mtx_t lista_mtx;    //mtx per accesso lista emergenze
extern mtx_t log_mtx;      //mtx per accesso a file LOG

extern cnd_t lista_cnd;    //cnd per attesa lista emergenze

extern atomic_int keep_running; //Settata a 1: i thrd continuano ad operare. Settata a 0: i thrd terminano
extern atomic_int thrd_attivi;  //Se si genera un thrd con detach si aumenta di 1. Prima che il thrd termini, si decrementa a -1.
                                //Serve a capire quando tutti i thrd (su cui non si fa una join) terminano per evitare memory leaks.

int thrd_insert(void *data){
    //Casto il puntatore agli argomenti
    thrd_data_insert *args = (thrd_data_insert*)data;

    //Variabile tempo per documentare le azioni su logFile.txt
    char time_now[BUFF];

    //Analizzo la stringa estratta dalla message_queue
    emergency_request_t *analised = analisi_richiesta(args->emrg, args->flog);

    //Se ritorna NULL (non c'è una struttura emergency_request_t da deallocare), dealloco lo spazio per gli argomenti ed esco
    if(!analised){
        printf("Emergenza scartata, formato errato.\n");
        fflush(stdout);
        
        free(args);

        //Sottraggo alla variabile atomica 1, che indica i thrd in corso a cui è stata fatta una detach
        atomic_fetch_sub(&thrd_attivi,1);
        return thrd_success;
    }

    //Si valida la richiesta di emergenza
    emergency_t *valid = validazione_richiesta(analised, args);

    //Si dealloca lo spazio per l'emergenza analizzata
    destroy_analisi(analised);

    //Se ritorna NULL (non bisogna deallocare una struttura emergency_t), dealloco lo spazio per gli argomenti ed esco
    if(!valid){ 
        printf("Emergenza scartata, dati errati.\n");
        fflush(stdout);
        
        
        free(args);

        //Sottraggo alla variabile atomica 1, che indica i thrd in corso a cui è stata fatta una detach
        atomic_fetch_sub(&thrd_attivi,1);
        return thrd_success;
    }

    //Inizializzo una variabile tempo per calocalre il delay dell'emergenza prima di inserirla nella coda
    time_t now = time(NULL);
    int delay = valid->timestamp - now;
    
    //Se il delay è maggiore di 0 si esegue un'attesa
    if(delay > 0 && atomic_load(&keep_running)){
        struct timespec attesa = {.tv_sec = delay, .tv_nsec = 0};
        thrd_sleep(&attesa, NULL);
    }

    //keep_running è la variabile che impone la terminazione del programma se settata a 0
    if(atomic_load(&keep_running)){
        //Si compiano localmente (sull stack del thread) le informazioni da stampare sul file log dell'emergenza validata
        emergency_t current = *valid;
        
        //Dopo l'eventuale attesa, si inserisce l'emergenza validata nella lista delle emergenze
        mtx_lock(&lista_mtx);
            add_emrg(lista_emergenze, valid);

            //Si svegliano tutti i thrd in attesa di emergenze
            cnd_broadcast(&lista_cnd);
        mtx_unlock(&lista_mtx);

        //Messaggio di DEBUG
        #ifdef DEBUG
            printf("THRD INSERT: ");
            fflush(stdout);
        #endif
        printf("EMERGENZA %d_%s inserita.\n",current.id, current.type->emergency_desc);
            fflush(stdout);

        //Scrittura su logFile.txt per mezzo di una mutex
        mtx_lock(&log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(args->flog,"[%s] [%d_%s] [MESSAGE_QUEUE] <Nuova emergenza inserita>\n", \
                    time_now, current.id, current.type->emergency_desc), args->flog, "Errore durante scrittura file LOG da thrd_insert().\n");
        mtx_unlock(&log_mtx);
    }else{
        //Se si impone la chiusura del programma, non si inserisce l'emergenza nella lista_emergenze ma si dealloca
        destroy_emrg_validata(valid);
    }

    //Se tutto è andato a compimento, si dealloca lo spazio gli argomenti.
    //Non si dealloca l'emergenza validata; verrà deallocata dal thrd che la estrarrà dalla lista o da una funzione ausigliaria in caso di timeout
    free(args);

    //Sottraggo alla variabile atomica 1, che indica i thrd in corso a cui è stata fatta una detach
    atomic_fetch_sub(&thrd_attivi,1);
    return thrd_success;
}