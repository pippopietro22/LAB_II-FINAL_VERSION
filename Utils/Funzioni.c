#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdatomic.h>

#include "Macro.h"
#include "Strutture.h"

//Le funzioni si servono di alcune variabili globali dichiarate nel main che vengono riportate 
//qui come extern
extern lista_t *lista_emergenze;
extern int soccorritori_liberi[RESCUER_TYPES];

extern mtx_t log_mtx;

extern atomic_int id_emrg;
extern atomic_int emrg_gestite;


//Funzione che documenta sul file log l'inserimento di un emergenza con qualche errore nel formato
void emergenza_errata(FILE *fd, char *msg){
    //Variabile temporale da inserire nel messaggio
    char time_now[BUFF];
    tempo_corrente(time_now);

    mtx_lock(&log_mtx);
        FPRINT(fprintf(fd,"[%s] [MAIN] [ERRORE INSERIMENTO] %s",time_now, msg),fd,\
                        "Errore scrittura file LOG da emergenza_errata.\n");
    mtx_unlock(&log_mtx);
}

//Funzione che analizza la stringa estratta dalla message_queue (inviata dal client)
emergency_request_t *analisi_richiesta(char *msg, FILE *flog){
    //Variabili di supporto per la creazione della struttura emergency_request_t
    int delay, x, y;
    char name[BUFF];
    
    //PARSING STRINGA
    if (sscanf(msg, "%s %d %d %d", name, &x, &y, &delay) != 4){ 
        emergenza_errata(flog,"Errore formato emergenza.\n");
        return NULL;
    }

    //Puntatore a struttura emergency_request_t che verrà ritornato al chiamante
    emergency_request_t *request;
    MALLOC(request,(emergency_request_t*)malloc(sizeof(emergency_request_t)),"Errore durante malloc da analisi_richiesta().\n");
    
    //Si settano le varie proprietà
    request->x = x;
    request->y = y;
    strcpy(request->emergency_name, name);
    request->timestamp = time(NULL) + delay;    //Timestamp inteso come il tempo corrente dell'analisi più il delay specificato
    
    return request;
}


//Funzione che valida la richiesta d'emergenza
emergency_t *validazione_richiesta(emergency_request_t *emg, thrd_data_insert*dati){
    if(emg == NULL) return NULL; //Se il puntatore emergency_request_t* è NULL termina la funzione

    //Si scorre l'array tipiEmergenze per trovare una corrispondenza fra l'emergenza richiesta e quelle 
    //specificate nel file conf
    int i = 0;
    while(i < EMERGENCY_TYPES && strcmp(emg->emergency_name, dati->tipiEmergenze[i].emergency_desc) != 0) i++;

    //Se non è stata trovata corrispondenza, si documenta sul logFile.txt e si ritorna NULL (Alla deallocazione di emergency_request_t penserà il thrd chiamante)
    if(i == EMERGENCY_TYPES){
        emergenza_errata(dati->flog,"Errore nome emergenza.\n");
        return NULL;
    }

    //Se Una delle coordinate è errata, si procede come sopra e si ritorna NULL
    if(emg->x < 0 || emg->x > dati->ambiente->dimX){ 
        emergenza_errata(dati->flog,"Errore dimensione x emergenza.\n");
        return NULL;
    }
    if(emg->y < 0 || emg->y > dati->ambiente->dimY){ 
        emergenza_errata(dati->flog,"Errore dimensione y emergenza.\n");
        return NULL;
    }

    //Una volta finiti i controlli, si crea la struttura dati e si assegnano le informazioni
    emergency_t *validato;
    MALLOC(validato,(emergency_t *)malloc(sizeof(emergency_t)), "Errore durante malloc da validazione_richiesta().\n");
    validato->id = atomic_fetch_add(&id_emrg,1);
    validato->type = &dati->tipiEmergenze[i];
    validato->status = WAITING;
    validato->x = emg->x;
    validato->y = emg->y;
    validato->timestamp = emg->timestamp;
    validato->rescuer_count = 0;

    //Per il campo rescuer_count, si scorrono tutti i rescuers richiesti dall'emergenza e si sommano tutti i soccorritori richiesti
    for(i = 0; i < RESCUER_TYPES; i++){
        //Se l'emergenza non richiede un tipo di soccorritore, si passa all'iterazione successiva
        if(validato->type->rescuers[i].type == NULL) continue;
        //Si somma
        validato->rescuer_count += validato->type->rescuers[i].required_count;
    }

    return validato;
}


//Funzione modulabile a seconda di come si definisce la struttura per deallocare emergency_request_t
void destroy_analisi(emergency_request_t* emg){
    if(emg != NULL){
        free(emg);
    }
}

//Funzione modulabile a seconda di come si definisce la struttura per deallocare emergency_t
void destroy_emrg_validata(emergency_t *val){ 
    if(val != NULL){
        free(val);
    }
}


//Funzione di confronto per la lista_emergenze
int confronto(emergency_t *emrg1, emergency_t *emrg2){
    //Si fa andare verso la coda della lista (zona estrazione) l'emergenza con la più alta priorità
    if(emrg1->type->priority > emrg2->type->priority) return 1;
    if(emrg2->type->priority > emrg1->type->priority) return -1;
    
    //In caso di parità di priorità, si calcolano i tempi rimanenti delle emergenze prima del timeout
    int t1 = tempo_rimanente(emrg1);
    int t2 = tempo_rimanente(emrg2);
        
    //Infine, si favorisce quella con meno tempo rimasto collocandola più vicino alla coda
    return t2 - t1;
}


//Funzione che calcola, a seconda del timestamp e dalla priorità, il tempo rimanente dell'emergenza passata come argomento
int tempo_rimanente(emergency_t *emrg){
    //Si dichiara la variabile contenente il tempo corrente
    time_t now = time(NULL);

    //A seconda della priorità cambia il tempo concesso prima del TIMEOUT
    switch(emrg->type->priority){
        case 1:
            return (30 + emrg->timestamp) - now;   //Tempo_deadline - Tempo_corrent
            break;
        case 2: 
            return (10 + emrg->timestamp) - now;
            break;
        default:
            //In caso di qualsiasi altra priorità (compresa 0) si ritorna 1000 (dato che -1 andrebbe a collidere con la logica)
            return 1000;   
            break;
    }
}


//Funzione che calcola distanza MANHATTAN
int distanza_Manhattan(int x1, int y1, int x2, int y2){
    int x = abs(x2-x1);
    int y = abs(y2-y1);
    return x + y;
}


//Funzione che scrive nel buffer passato come argomento, data e ora corrente
void tempo_corrente(char *buffer){
    time_t now = time(NULL);
    char *time_now = ctime(&now);

    strcpy(buffer, time_now);
    buffer[strlen(buffer) - 1] = '\0';
}


//Funzione che calcola il tempo massimo dell'arrivo dei soccorsi
int tempo_arrivo_soccorsi(emergency_t *emrg){
    //Variabile con il risultato
    int t_attesa = 0;
        //Si scorrono tutti i tipi di soccorritore
        for(int i = 0; i < RESCUER_TYPES; i++){
            //Se l'emergenza non richiede questo tipo di soccorritore si passa al soccorritore successivo
            if(emrg->type->rescuers[i].type == NULL) continue;

            //Si calcola la distanza fra il luogo dell'emergenza e la base del soccorritore e si divide per la velocità
            int t_arrivo = distanza_Manhattan(emrg->x,emrg->y,emrg->type->rescuers[i].type->x,emrg->type->rescuers[i].type->y) \
                            /emrg->type->rescuers[i].type->speed;
            //Si confronta il tempo ottenuto con quello salvato in precedenza
            t_attesa = MAX(t_attesa,t_arrivo);      //Macro definita in Macro.h
        }
    return t_attesa;
}

//Funzione che controlla se non ci sono più emergenze in coda, se nessun thrd sta trattando un emergenza e se tutti i soccorritori sono 
//liberi. Se queste condizioni vengono soddisfatte, stama a schermo lo stato del programma che non sta eseguendo più nulla.
void controllo_situazione(rescuer_type_t *tipiSoccorritori){
    int numero_emrgs = lista_emergenze->dim_lista;  //Numero di emergenze in coda
    int socc_pronti = 1; //socc_pronti = 1: tutti i soccorritori liberi. socc_pronti = 0: ci sono soccorritori impegnati

    //Si scorre l'array soccorritori_liberi e si confronta il numero di soccorritori liberi con il numero di soccorritori disponibili
    for(int i = 0; i < RESCUER_TYPES; i++){
        //Se un solo tipo di soccorritore non ha tutti i soccorritori liber, allora si setta socc_pronti = 0 e terminiamo il ciclo
        if(soccorritori_liberi[i] < tipiSoccorritori[i].quantity){ 
            socc_pronti = 0;
            break;
        }
    }

    //Se tutte le condizioni sono soddisfatte si stampa a schermo il messaggio
    if(!numero_emrgs && socc_pronti && !atomic_load(&emrg_gestite)){    //emrg_gestite indica se ci sono thrd con emergenze in corso da gestire
        printf("TUTTE LE EMERGENZE COMPLETATE\n");
        printf("TUTTI I SOCCORRITORI LIBERI\n");
        fflush(stdout);
    }
}