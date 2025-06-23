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

#include "Macro.h"
#include "Strutture.h"

extern lista_t *lista_emergenze;
extern int soccorritori_liberi[RESCUER_TYPES];

extern mtx_t log_mtx;
extern atomic_int id_emrg;

void emergenza_errata(FILE *fd, char *msg){
    char time_now[BUFF];
    tempo_corrente(time_now);

    mtx_lock(&log_mtx);
        FPRINT(fprintf(fd,"[%s] [MAIN] [ERRORE INSERIMENTO] %s",time_now, msg),fd,\
                        "Errore scrittura file LOG da emergenza_errata.\n");
    mtx_unlock(&log_mtx);
}

emergency_request_t *analisi_richiesta(char *msg, FILE *flog){
    int delay, x, y;
    char name[BUFF];
    
    if (sscanf(msg, "%s %d %d %d", name, &x, &y, &delay) != 4){ 
        emergenza_errata(flog,"Errore formato emergenza.\n");
        return NULL;
    }

    emergency_request_t *request;
    MALLOC(request,(emergency_request_t*)malloc(sizeof(emergency_request_t)),"Errore durante malloc da analisi_richiesta().\n");
    
    request->x = x;
    request->y = y;
    strcpy(request->emergency_name, name);
    request->timestamp = time(NULL) + delay;
    
    return request;
}

emergency_t *validazione_richiesta(emergency_request_t *emg, thrd_data_insert*dati){
    if(emg == NULL) return NULL;

    int i = 0;
    while(i < EMERGENCY_TYPES && strcmp(emg->emergency_name,dati->tipiEmergenze[i].emergency_desc) != 0) i++;
    if(i == EMERGENCY_TYPES){
        emergenza_errata(dati->flog,"Errore nome emergenza.\n");
        return NULL;
    }
    if(emg->x < 0 || emg->x > dati->ambiente->dimX){ 
        emergenza_errata(dati->flog,"Errore dimensione x emergenza.\n");
        return NULL;
    }
    if(emg->y < 0 || emg->y > dati->ambiente->dimY){ 
        emergenza_errata(dati->flog,"Errore dimensione y emergenza.\n");
        return NULL;
    }

    emergency_t *validato;
    MALLOC(validato,(emergency_t *)malloc(sizeof(emergency_t)), "Errore durante malloc da validazione_richiesta().\n");
    validato->id = atomic_fetch_add(&id_emrg,1);
    validato->type = &dati->tipiEmergenze[i];
    validato->status = WAITING;
    validato->x = emg->x;
    validato->y = emg->y;
    validato->timestamp = emg->timestamp;
    validato->rescuer_count = 0;

    for(i = 0; i < RESCUER_TYPES; i++){
        if(validato->type->rescuers[i].type == NULL) continue;
        validato->rescuer_count += validato->type->rescuers[i].required_count;
    }

    return validato;
}

void destroy_analisi(emergency_request_t* emg){
    free(emg);
}

void destroy_emrg_validata(emergency_t *val){ 
    if(val != NULL){
        free(val);
    }
}


//Funzione di sorrting di coda
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


void tempo_corrente(char *buffer){
    time_t now = time(NULL);
    char *time_now = ctime(&now);

    strcpy(buffer, time_now);
    buffer[strlen(buffer) - 1] = '\0';
}


int tempo_arrivo_soccorsi(emergency_t *emrg){
    int t_attesa = 0;
        for(int i = 0; i < RESCUER_TYPES; i++){
            if(emrg->type->rescuers[i].type == NULL) continue;
            int t_arrivo = distanza_Manhattan(emrg->x,emrg->y,emrg->type->rescuers[i].type->x,emrg->type->rescuers[i].type->y) \
                            /emrg->type->rescuers[i].type->speed;
            t_attesa = MAX(t_attesa,t_arrivo);
        }
    return t_attesa;
}

void controllo_situazione(rescuer_type_t *tipiSoccorritori){
    int numero_emrgs = lista_emergenze->dim_lista;
    int socc_pronti = 1;
    for(int i = 0; i < RESCUER_TYPES; i++){
        if(soccorritori_liberi[i] < tipiSoccorritori[i].quantity) socc_pronti = 0;
    }

    if(!numero_emrgs && socc_pronti){
        printf("TUTTE LE EMERGENZE COMPLETATE\n");
        printf("TUTTI I SOCCORRITORI LIBERI\n");
        fflush(stdout);
    }
}