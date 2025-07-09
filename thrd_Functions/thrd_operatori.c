#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<threads.h>
#include<stdatomic.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Questa funzione utilizza variabili globali dichiarate in main.c
//Vengono riportate qui come extern

extern lista_t *lista_emergenze;  //Puntatore globale (accessibile in ogni parte del codice) alla lista emergenze (tramite mutex)
extern int soccorritori_liberi[RESCUER_TYPES]; //Variabili accedibile da qualsiasi thread per controllare risorse disponibili (tramite mutex)

extern mtx_t rescuer_mtx;  //variabili mtx per risorse
extern mtx_t lista_mtx;    //mtx per accesso lista emergenze
extern mtx_t log_mtx;      //mtx per accesso a file LOG

extern cnd_t rescuer_cnd;  //cnd per attesa ripristino risorse
extern cnd_t lista_cnd;    //cnd per attesa lista emergenze

extern atomic_int keep_running; //Settata a 1: i thrd continuano ad operare. Settata a 0: i thrd terminano
extern atomic_int emrg_gestite; //Indica quante emergenze vengono prese in carico da un thrd per essere gestite.


int thrd_operatori(void *data){
    //Si converte il puntatore void in un puntatore ad argomenti della funzione
    thrd_data_operative *args = (thrd_data_operative*)data;

    //Messaggio DEBUG
    #ifdef DEBUG
        printf("THRD %d ATTIVATO\n",args->id);
        fflush(stdout);
    #endif

    //Si crea una struttura locale per salvare le varie emergenze da trattare
    emergency_t current;

    //Variabile tempo per documentare le azioni sul file log
    char time_now[BUFF];

    //Il ciclo operativo del thrd viene regolato dalla variabile atomica keep_running
    while(atomic_load(&keep_running)){
        //Utilizzo un accesso mtx alla coda per estrarre l'emergenza con maggior priorità e minor tempo rimanente
        mtx_lock(&lista_mtx);
            //Rimuovo emergenze con timeout (tempo essaurito per eccesso di attesa nella coda)
            rimuovi_timeout(lista_emergenze, &log_mtx, args->flog, args->tipiSoccorritori);

            //Mi metto in attesa su lista_cnd in caso non ci siano emergenze disponibili (e keep_running sempre uguale a 1)
            while(lista_emergenze->dim_lista == 0 && atomic_load(&keep_running)){
                cnd_wait(&lista_cnd, &lista_mtx);
            }

            //Se keep_running impone la fine del programma, rilascio la mtx e termino il thrd
            if(!atomic_load(&keep_running)){
                mtx_unlock(&lista_mtx);
                break;
            }

            //Estraggo il nodo dalla lista con l'emergenza a maggior priorità
            emergency_t *newEmergency = estrai_nodo(lista_emergenze);
        mtx_unlock(&lista_mtx);

        //In caso il nodo, per un qualsiasi malfunzionamento, mi restituisse un puntatore a NULL, salto l'iterazione
        if (newEmergency == NULL) continue;

        //Segno che il thrd sta gestendo un'emergenza
        atomic_fetch_add(&emrg_gestite,1);

        //Salvo nella struttura locale tutti i dati riguardanti l'emergenza e dealloco la struttura dall'heap
        current = *newEmergency;
        current.status = ASSIGNED;

        //Dealloco la struttura nell'heap dell'emergenza
        destroy_emrg_validata(newEmergency);


        //CONTROLLO SE I SOCCORSI POSSONO ARRIVARE IN TEMPO
        int t_attesa = tempo_arrivo_soccorsi(&current);     //Attesa massima soccorsi
        int t_rimanente = tempo_rimanente(&current);        //Tempo rimanente prima del timeout dell'emergenza
        //Se il tempo rimanente non è sufficiente per far arrivare i soccorsi, l'emergenza viene scartata
        if(t_rimanente < t_attesa){
            //Messaggio di DEBUG
            printf("EMERGENZA %d_%s TIMEOUT: Impossibile raggiungere il luogo dell'emergenza in tempo.\n",current.id, current.type->emergency_desc);
            fflush(stdout);

            //Si documenta l'impossibilità di far arrivare i soccorsi intempo
            mtx_lock(&log_mtx);
                tempo_corrente(time_now);
                FPRINT(fprintf(args->flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <TIMEOUT: tempo insufficiente per raggiungere il luogo dell'emergenza>\n",time_now, \
                                current.id, current.type->emergency_desc),args->flog,"Errore durante scrittura su file LOG.\n");
            mtx_unlock(&log_mtx);

            //Indico che l'emergenza gestita viene terminata
            atomic_fetch_sub(&emrg_gestite,1);

            //controllo la situazione
            controllo_situazione(args->tipiSoccorritori);

            //Si procede con la prossima iterazione
            continue;
        }

        //Messaggio Presa in carico
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

            //Alloco spazio per l'array dell'emergenza contenente i gemelli digitali
            MALLOC(current.rescuers_dt[i], (rescuer_digital_twin_t**)malloc(res_count*sizeof(rescuer_digital_twin_t*)),"Errore durante malloc da thrd_operatori().\n");

            //Si accede alle risorse tramite mtx
            mtx_lock(&rescuer_mtx);
                //Se la quantità di soccorritori non è sufficente, ci si mette in attesa
                while(res_count > soccorritori_liberi[i]){
                    cnd_wait(&rescuer_cnd, &rescuer_mtx);
                }

                //Si segna la quantità ri risorse sottratte al totale
                soccorritori_liberi[i] -= res_count;

                //Si recuperano i gemelli digitali
                int idx_res = 0;  //Indice che scorre nell'array di gemelli digitali (di un tipo soccorritori dato da "i") dell'emergenza
                //Scorriamo l'array di gemelli digitali di un dato tipo "i"
                for(int j = 0; j < args->tipiSoccorritori[i].quantity; j++){
                    //Se il soccorritore non ha status IDLE, non è assegnabile alla nostra emergenza, perciò si salta quest'iterazione
                    if(args->soccorritori[i][j].status != IDLE) continue;

                    //Se il gemello è libero, si salva il suo indirizzo nell'array dell'emergenza e si modifica il suo status
                    current.rescuers_dt[i][idx_res] = &args->soccorritori[i][j];
                    current.rescuers_dt[i][idx_res]->status = EN_ROUTE_TO_SCENE;
                    
                    //Documento sul file LOG
                    mtx_lock(&log_mtx);
                        tempo_corrente(time_now);
                        FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore procede verso %d_%s>\n",time_now, \
                                        current.rescuers_dt[i][idx_res]->rescuer->rescuer_type_name, current.rescuers_dt[i][idx_res]->id, \
                                        current.id, current.type->emergency_desc),args->flog, \
                                        "Errore durante scrittura file LOG da thrd_operatori().\n");
                    mtx_unlock(&log_mtx);

                    //Una volta salvato il gemello in una cella dell'array, si incrementa l'indice per passare alla successiva cella da riempire
                    idx_res++;

                    //Se siamo arrivati al completamento del nostro array di gemelli digitali (dell'emergenza), si termina con questo tipo di soccorritore
                    if(idx_res == res_count) break;
                }
            mtx_unlock(&rescuer_mtx);
        }


        //CONTROLLO SE È RIMASTO ABBASTANZA TEMPO DOPO AVER RECUPERATO LE RISORSE
        t_rimanente = tempo_rimanente(&current);
        //Se il tempo rimanente non è sufficiente per far arrivare i soccorritori sul luogo dell'emergenza (o keep_running fa terminare il programma)
        //Si liberano tutte le risorse e si termina il programa
        if(t_rimanente < t_attesa || !atomic_load(&keep_running)){
            //Si scorrono tutti i tipi di risorse per liberarle
            for(int i = 0; i < RESCUER_TYPES; i++){
                //Se questo tipo di risorsa non è richiesto dall'emergenza si salta l'iterazione
                if(current.type->rescuers[i].type == NULL) continue;

                //Variabile più comoda che indica quante risorse del dato tipo sono richieste dall'emergenza
                int res_count = current.type->rescuers[i].required_count;
                
                mtx_lock(&rescuer_mtx);
                    //Si scorre l'array dei gemelli digitali dell'emergenza
                    for(int j = 0; j < res_count; j++){
                        //Si settano i soccorritori come IDLE
                        current.rescuers_dt[i][j]->status = IDLE;

                        //Si documenta sul logFile.txt
                        mtx_lock(&log_mtx);
                            tempo_corrente(time_now);
                            FPRINT(fprintf(args->flog,"[%s] [%s_%d] [RESCUER_STATUS] <Soccorritore abbandona l'emergenza>\n",time_now, \
                                        current.rescuers_dt[i][j]->rescuer->rescuer_type_name, current.rescuers_dt[i][j]->id),args->flog, \
                                        "Errore durante scrittura file LOG da thrd_operatori().\n");
                        mtx_unlock(&log_mtx);
                    }

                    //Si segna sui contatori il numero di risorse restituite
                    soccorritori_liberi[i] += res_count;

                    //Sveglio tutti i thrd in attesa di risorse
                    cnd_broadcast(&rescuer_cnd);
                mtx_unlock(&rescuer_mtx);

                //Si dealloca lo spazio utilizzato dall'array di gemelli digitali
                free(current.rescuers_dt[i]);
            }


            if(atomic_load(&keep_running)){
                //Messaggio di DEBUG
                #ifdef DEBUG
                        printf("THRD %d \n",args->id);
                        fflush(stdout);
                #endif
                printf("EMERGENZA %d_%s TIMEOUT, IMPIEGATO TROPPO TEMPO PER ACQUISIRE RISOSRE.\n", current.id, current.type->emergency_desc);
                fflush(stdout);
            }

            //Questo thrd rilascia la sua emergenza e lo segnala
            atomic_fetch_sub(&emrg_gestite,1);

            controllo_situazione(args->tipiSoccorritori);

            continue;
        }


        //Messaggio di DEBUG
        #ifdef DEBUG
            printf("THRD %d EMERGENZA %d_%s RISORSE OTTENUTE ATTENDO ARRIVO\n",args->id, current.id, current.type->emergency_desc);
            fflush(stdout);
        #endif

        //ATTENDO L'ARRIVO DEI SOCCORSI
        //Dato che l'emergenza inizia ad essere gestita con l'arrivo di tutti i soccorsi, ho utilizzato una banale attesa per simulare l'arrivo del soccorso che 
        //impiega più tempo per arrivare sul luogo.
        struct timespec attesa = {.tv_sec = t_attesa, .tv_nsec = 0};
        thrd_sleep(&attesa,NULL);

        //Modifico le coordinaate dei soccorritori
        mtx_lock(&rescuer_mtx);
            //Scorrere sui vari tipi di soccorsi
            for(int i = 0; i < RESCUER_TYPES; i++){
                //Saltare tipi di soccorsi non richiesti
                if(current.type->rescuers[i].type == NULL) continue;

                //Scorrere l'array di gemelli digitali
                for(int j = 0; j < current.type->rescuers[i].required_count; j++){
                    //Settare le coordinate e lo stato
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

        //Messaggio di Debug
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
        int n_res = 0; //indice che serve a scorrere il numero di thrd da inizializzare riguardanti i vari soccorritori
        thrd_t res_return[current.rescuer_count];
        //Si scorrornon i tipi soccorritori
        for(int i = 0; i < RESCUER_TYPES; i++){
            //Saltiamo i tipi soccorritori non interessati
            if(current.type->rescuers[i].type == NULL) continue;

            //Si scorrono i gemelli digitali del dato tipo
            for(int j = 0; j < current.type->rescuers[i].required_count; j++){
                //Si inizializzano argomenti per i thrd allocando dinamicamente lo spazio ogni volta per evitare race conditions
                thrd_data_socc *args_resc;
                MALLOC(args_resc, (thrd_data_socc*)malloc(sizeof(thrd_data_socc)),"Errore durante malloc da thrd_operatori.\n");

                args_resc->digTwin = current.rescuers_dt[i][j];         //Il gemello digitale che il thrd deve gestire
                args_resc->tipiSoccorritori = args->tipiSoccorritori;   //L'array contenente i tipi soccorsi
                args_resc->flog = args->flog;       //Il descrittore del logFile.txt
                args_resc->time_to_manage = current.type->rescuers[i].time_to_manage;   //Il tempo impiegato dal soccorritore per svolgere l'emergenza
                
                thrd_create(&res_return[n_res++],rescuer_on_scene,args_resc);     
            }
        }

        //ATTENDO CHE TUTTI I SOCCORRITORI TERMININO IL LAVORO SUL LUOGO DELL'EMERGENZA
        int ret, res;   //valori di ritorno per thrd_join
        //Scorriamo l'arrai di gemelli digitali del dato tipo per eseguire 
        for(int i = 0; i < n_res; i++){
            THRDJOIN(ret,res,thrd_join(res_return[i],&res),"Errore durante thrd da thrd_operativi().\n");
        }

        //DEALLOCHIAMO LO SPAZIO PER GLI ARRAY DI GEMELLI DIGITALI
        //Scorriamo i tipi soccorritore
        for(int i = 0; i < RESCUER_TYPES; i++){
            //Saltiamo i tipi soccorritore non interessati
            if(current.type->rescuers[i].type == NULL) continue;

            //Deallocazione array Gemelli digitali
            free(current.rescuers_dt[i]);
        }    

        //Si cambia lo status dell'emergenza
        current.status = COMPLETED;

        //Messaggio di debug
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

        //Indico che viene gestita un'emergenza in meno
        atomic_fetch_sub(&emrg_gestite,1);
    }

    //Messaggio DEBUG
    #ifdef DEBUG
        printf("THRD %d USCITO\n",args->id);
        fflush(stdout);
    #endif

    //Dealloco spazio argomenti prima di terminare il thrd
    free(args);

    return thrd_success;
}