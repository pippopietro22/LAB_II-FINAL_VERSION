#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "Macro.h"
#include "Strutture.h"

lista_t *lista_init(){
    lista_t *new_list;
    MALLOC(new_list,(lista_t*)malloc(sizeof(lista_t)),"Errore durante malloc da lista_init().\n");

    new_list->head = NULL;
    new_list->tail = NULL;
    new_list->dim_lista = 0;

    return new_list;
}

void destroy_list(lista_t *list){
    if (list != NULL) {
        nodo_t *current = list->head;
        while (current != NULL) {
            nodo_t *next = current->next;
            destroy_emrg_validata(current->emrg);
            free(current);
            current = next;
        }
        free(list);
    }
}



void add_emrg(lista_t *list, emergency_t *emrg) {
    nodo_t *newNode;
    MALLOC(newNode,(nodo_t*)malloc(sizeof(nodo_t)),"Errore durrante malloc da add_emrg.\n");
    newNode->emrg = emrg;
    newNode->next = newNode->prev = NULL;

    if (list->dim_lista == 0) {
        list->head = list->tail = newNode;
    }else if(list->dim_lista == 1){
        if(confronto(emrg,list->head->emrg) > 0){
            list->tail->next = newNode;
            newNode->prev = list->tail;
            list->tail = newNode;
        }else{
            newNode->next = list->head;
            list->head->prev = newNode;
            list->head = newNode;
        }
    }else{
        nodo_t *current = list->head;

        while (current && confronto(emrg, current->emrg) > 0) current = current->next;

        if (!current) {
            newNode->prev = list->tail;
            list->tail->next = newNode;
            list->tail = newNode;
        } else if (current == list->head) {
            newNode->next = list->head;
            list->head->prev = newNode;
            list->head = newNode;
        } else {
            newNode->next = current;
            newNode->prev = current->prev;
            current->prev->next = newNode;
            current->prev = newNode;
        }
    }

    list->dim_lista++;
}



void rimuovi_timeout(lista_t *list, mtx_t *log_mtx, FILE*flog){
    if(list->dim_lista == 0) return;
    
    nodo_t *current = list->tail;

    while(current != NULL && tempo_rimanente(current->emrg) <= 0){
        time_t now = time(NULL);
        char *time_now = ctime(&now);
        time_now[strlen(time_now) - 1] = '\0';

        mtx_lock(log_mtx);
            FPRINT(fprintf(flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <TIMEOUT: tempo esaurito, emergenza rimasta in attesa per troppo tempo.>\n", \
                            time_now,current->emrg->id, current->emrg->type->emergency_desc),flog, "Errore durante scrittura file LOG da rimuovi_timeout().\n");
        mtx_unlock(log_mtx);

        #ifdef DEBUG
            printf("EMERGENZA %d_%s TIMEOUT, rimasta in attesa per troppo tempo\n", current->emrg->id, current->emrg->type->emergency_desc);
            fflush(stdout);
        #endif

        nodo_t *da_cancellare = current;
        current = current->prev;

        destroy_emrg_validata(da_cancellare->emrg);
        free(da_cancellare);
        list->dim_lista--;

        if (current == NULL) {
            list->head = list->tail = NULL;
        } else {
            current->next = NULL;
            list->tail = current;
        }
    }
}

emergency_t *estrai_nodo(lista_t *list){
    if(list->dim_lista == 0) return NULL;

    emergency_t *emrg = list->tail->emrg;

    if(list->dim_lista == 1){
        free(list->tail);
        list->head = list->tail = NULL;
    }else{
        nodo_t *current;
        current = list->tail;
        list->tail = list->tail->prev;
        list->tail->next = NULL;
        free(current);
    }

    list->dim_lista--;

    return emrg;
}

void stampa_lista(lista_t *list){
    if(list->dim_lista == 0) return;
    
    nodo_t *current = list->head;
    while(current){
        printf("EMERGENZA %s TEMPO %d\n",current->emrg->type->emergency_desc, tempo_rimanente(current->emrg));
        current = current->next;
    }
}