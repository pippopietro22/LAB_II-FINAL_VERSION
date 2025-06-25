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


int thrd_operatori(void *data){
    //Si converte il puntatore void in un puntatore ad argomenti della funzione
    thrd_data_operative *args = (thrd_data_operative*)data;

    #ifdef DEBUG
        printf("THRD %d ATTIVATO\n",args->id);
        fflush(stdout);
    #endif

    //Si crea una struttura locale per salvare le varie emergenze da trattare
    emergency_t current;

    //Variabile tempo per documentare le azioni sul file log
    char time_now[BUFF];

    //Il ciclo operativo del thrd viene regolato dalla variabile atomica keep_runninv
    while(atomic_load(&keep_running)){
        //Utilizzo un accesso mtx alla coda per estrarre l'emergenza con maggior priorità e minor tempo rimanente
        mtx_lock(&lista_mtx);
            //Rimuovo emergenze con timeout
            rimuovi_timeout(lista_emergenze, &log_mtx, args->flog);

            //Mi metto in attesa su lista_cnd in caso non ci siano emergenze disponibili
            while(lista_emergenze->dim_lista == 0 && atomic_load(&keep_running)){
                cnd_wait(&lista_cnd, &lista_mtx);
            }

            //Se keep_running impone la fine del programma, rilascio la mtx e termino il thrd
            if(!atomic_load(&keep_running)){
                mtx_unlock(&lista_mtx);
                break;
            }

            //Estraggo il nodo dalla lista con la mia emergenza
            emergency_t *newEmergency = estrai_nodo(lista_emergenze);
        mtx_unlock(&lista_mtx);

        //In caso il nodo, per un qualsiasi malfunzionamento, mi restituisse un puntatore a NULL, salto l'iterazione
        if (newEmergency == NULL) continue;

        //Salvo nella struttura locale tutti i dati riguardanti l'emergenza e dealloco la struttura dall'heap
        current = *newEmergency;
        current.status = ASSIGNED;
        destroy_emrg_validata(newEmergency);


        //CONTROLLO SE I SOCCORSI POSSONO ARRIVARE IN TEMPO
        int t_attesa = tempo_arrivo_soccorsi(&current);
        int t_rimanente = tempo_rimanente(&current);
        if(t_rimanente <= 0 || t_rimanente < t_attesa){
            #ifdef DEBUG
                printf("EMERGENZA %d_%s TIMEOUT: Impossibile raggiungere il luogo dell'emergenza in tempo.\n",current.id, current.type->emergency_desc);
                fflush(stdout);
            #endif

            mtx_lock(&log_mtx);
                tempo_corrente(time_now);
                FPRINT(fprintf(args->flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <TIMEOUT: tempo insufficiente per raggiungere il luogo dell'emergenza>\n",time_now, \
                                current.id, current.type->emergency_desc),args->flog,"Errore durante scrittura su file LOG.\n");
            mtx_unlock(&log_mtx);
            continue;
        }

        #ifdef DEBUG
            printf("THRD %d ",args->id);
            fflush(stdout);
        #endif
        printf("EMERGENZA %d_%s PRESA IN CARICO\n", current.id, current.type->emergency_desc);
        fflush(stdout);

        //Documento presa in carico dell'emergenza
        mtx_lock(&log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(args->flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <Emergenza presa in carico.>\n",time_now, current.id, current.type->emergency_desc),args->flog, \
                            "Errore durante scrittura file LOG da thrd_operatori().\n");
        mtx_unlock(&log_mtx);

        //ACCEDO ALLE RISORSE NECESSARIE.
        //Scorro tutti i  tipi di soccorritori per vedere di quali ha bisogno la mia emergenza
        for(int i = 0; i < RESCUER_TYPES; i++){
            //Se l'emergenza non ha bisogno di questo tipo, salterà l'iterazione corrente
            if(current.type->rescuers[i].type == NULL) continue;

            //Si copiano su una variabile più comoda il numero di soccorritori del dato tipo necessari
            int res_count = current.type->rescuers[i].required_count;

            //Alloco spazio sull'array dell'emergenza contenente i soccorritori
            MALLOC(current.rescuers_dt[i], (rescuer_digital_twin_t**)malloc(res_count*sizeof(rescuer_digital_twin_t*)),"Errore durante malloc da thrd_operatori().\n");

            //Si accede alle risorse tramite mtx
            mtx_lock(&rescuer_mtx);
                //Se la quantità di soccorritori non è sufficente, ci si mette in attesa
                while(res_count > soccorritori_liberi[i]){
                    cnd_wait(&rescuer_cnd, &rescuer_mtx);
                }

                //Si recuperano le risorse necessarie
                soccorritori_liberi[i] -= res_count;

                //Si recuperano i gemelli digitali
                int idx_res = 0;
                for(int j = 0; j < args->tipiSoccorritori[i].quantity; j++){
                    if(args->soccorritori[i][j].status != IDLE) continue;

                    current.rescuers_dt[i][idx_res] = &args->soccorritori[i][j];
                    current.rescuers_dt[i][idx_res]->status = EN_ROUTE_TO_SCENE;
                    
                    //Documento sul file LOG
                    mtx_lock(&log_mtx);
                        tempo_corrente(time_now);
                        FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore procede verso l'emergenza.>\n",time_now, \
                                        current.rescuers_dt[i][idx_res]->rescuer->rescuer_type_name, current.rescuers_dt[i][idx_res]->id),args->flog, \
                                        "Errore durante scrittura file LOG da thrd_operatori().\n");
                    mtx_unlock(&log_mtx);

                    idx_res++;

                    if(idx_res == res_count) break;
                }
            mtx_unlock(&rescuer_mtx);
        }

        #ifdef DEBUG
            printf("THRD %d EMERGENZA %d_%s RISORSE OTTENUTE ATTENDO ARRIVO\n",args->id, current.id, current.type->emergency_desc);
            fflush(stdout);
        #endif

        //ATTENDO L'ARRIVO DEI SOCCORSI
        struct timespec attesa = {.tv_sec = t_attesa, .tv_nsec = 0};
        if(atomic_load(&keep_running)) thrd_sleep(&attesa,NULL);

        //Modifico le coordinaate dei soccorritori
        mtx_lock(&rescuer_mtx);
            for(int i = 0; i < RESCUER_TYPES; i++){
                if(current.type->rescuers[i].type == NULL) continue;

                for(int j = 0; j < current.type->rescuers[i].required_count; j++){
                    current.rescuers_dt[i][j]->x = current.x;
                    current.rescuers_dt[i][j]->y = current.y;
                    current.rescuers_dt[i][j]->status = ON_SCENE;

                    //Documento sul file LOG
                    mtx_lock(&log_mtx);
                        tempo_corrente(time_now);
                        FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore arrivato sul luogo dell'emergenza>\n",time_now, \
                                    current.rescuers_dt[i][j]->rescuer->rescuer_type_name, current.rescuers_dt[i][j]->id), args->flog, \
                                    "Erroredurante scrittura file LOG da thrd_operatori().\n");
                    mtx_unlock(&log_mtx);
                }
            }
        mtx_unlock(&rescuer_mtx);

        #ifdef DEBUG
            printf("THRD %d ",args->id);
            fflush(stdout);
        #endif
        printf("EMERGENZA %d_%s INIZIO SVOLGIMENTO\n", current.id, current.type->emergency_desc);
        fflush(stdout);

        //Documento l'inizio dello svolgimento dell'emergenza
        mtx_lock(&log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(args->flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <L'emergenza sta venendo gestita>\n",time_now, current.id, current.type->emergency_desc), \
                            args->flog,"Errore scrittura file LOG da thrd_operatori().\n");
        mtx_unlock(&log_mtx);

        //SOCCORRITORI SVOLGONO EMERGENZA
        int n_res = 0;
        thrd_t res_return[current.rescuer_count];
        for(int i = 0; i < RESCUER_TYPES; i++){
            if(current.type->rescuers[i].type == NULL) continue;

            for(int j = 0; j < current.type->rescuers[i].required_count; j++){
                thrd_data_socc *args_resc;
                MALLOC(args_resc, (thrd_data_socc*)malloc(sizeof(thrd_data_socc)),"Errore durante malloc da thrd_operatori.\n");
                args_resc->digTwin = current.rescuers_dt[i][j];
                args_resc->tipiSoccorritori = args->tipiSoccorritori;
                args_resc->flog = args->flog;
                args_resc->time_to_manage = current.type->rescuers[i].time_to_manage;
                thrd_create(&res_return[n_res++],rescuer_on_scene,args_resc);
            }
        }

        //ATTENDO CHE TUTTI I SOCCORRITORI TERMININO IL LAVORO SUL LUOGO DELL'EMERGENZA
        n_res = 0;
        int ret, res;
        for(int i = 0; i < RESCUER_TYPES; i++){
            if(current.type->rescuers[i].type == NULL) continue;

            for(int j = 0; j < current.type->rescuers[i].required_count; j++){
                THRDJOIN(ret,res,thrd_join(res_return[n_res++],&res),"Errore durante thrd da thrd_operativi().\n");
            }

            free(current.rescuers_dt[i]);
        }    
        current.status = COMPLETED;

        #ifdef DEBUG
            printf("THRD %d ",args->id);
            fflush(stdout);
        #endif
        printf("EMERGENZA %d_%s CONCLUSA\n", current.id, current.type->emergency_desc);
        fflush(stdout);

        //Documento fine emergenza
        mtx_lock(&log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(args->flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <Emergenza CONCLUSA con successo.>\n",time_now, current.id, current.type->emergency_desc), \
                            args->flog,"Errore scrittura file LOG da thrd_operatori().\n");
        mtx_unlock(&log_mtx);

    }

    free(args);

    return thrd_success;
}