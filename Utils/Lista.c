#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "Macro.h"
#include "Strutture.h"

//Tutte le strutture sono definite in Strutture.h
//Le funzioni che operano sulla lista_emergenze (globale) vengono chiamate solo quando il thrd chiamante ha ottenuto
//il lock sulla lista (lista_mtx) in modo da evitare sovrapposizioni

//Funzione che crea struttra lista e ritorna un puntatore
lista_t *lista_init(){
    //Allocazione memoria
    lista_t *new_list;
    MALLOC(new_list,(lista_t*)malloc(sizeof(lista_t)),"Errore durante malloc da lista_init().\n");

    //Lista doppia con puntatore sia al nodo succeessivo che precedente
    new_list->head = NULL;
    new_list->tail = NULL;
    new_list->dim_lista = 0;    //Valore che tiene di conto del numero dei nodi (emergenze) presenti in un dato momento

    return new_list;
}

//Funzione per deallocazione memoria lista e nodi se eventualmente presenti
void destroy_list(lista_t *list){
    //Se il list punta effettivamente ad una struttura esegue la deallocazione
    if (list != NULL) {
        nodo_t *current = list->head;   //nodo ausigliario che punta alla testa

        //Finche il nodo corrente è diverso da null, ci saranno nodi da deallocare
        while (current != NULL) {
            nodo_t *next = current->next;  //Secondo nodo ausigliario che punta al nodo successivo mentre current dealloca il nodo corrente

            //Prima si dealloca l'emergenza salvata nel nodo e solo dopo si dealloca il nodo
            destroy_emrg_validata(current->emrg);  
            free(current);

            //Current inizia a puntare al successore finché non dealloca l'intera lista
            current = next;
        }

        //Infine si dealloca la struttura lista_t
        free(list);
    }
}


//Funzione aggiunta nodo (emergenza)
void add_emrg(lista_t *list, emergency_t *emrg) {
    //Nuovo nodo a cui si alloca spazio
    nodo_t *newNode;
    MALLOC(newNode,(nodo_t*)malloc(sizeof(nodo_t)),"Errore durrante malloc da add_emrg.\n");

    //Viene associato al nodo la sua emergenza
    newNode->emrg = emrg;
    newNode->next = newNode->prev = NULL;   //Si inizializzano i campi next e prev

    //Se la lista è vuota, il nuovo nodo diventa testa e coda
    if (list->dim_lista == 0) {
        list->head = list->tail = newNode;
        
    }else if(list->dim_lista == 1){
        //Se c'è solo un nodo si inserisce prima o dopo il nodo già presente, seconda del valore della funzione confronto
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
        //Se ci sono più nodi, bisogna scorrere la lista in cerca della posizione con la giusta priorità
        nodo_t *current = list->head;

        //Scorrimento
        while (current && confronto(emrg, current->emrg) > 0) current = current->next;

        //Se siamo arrivati in fondo alla lista, newNode diventa la nuova coda
        if (!current) {
            newNode->prev = list->tail;
            list->tail->next = newNode;
            list->tail = newNode;
        } else if (current == list->head) {
            //Se siamo rimasti in testa alla coda, newNode si colloca in cima
            newNode->next = list->head;
            list->head->prev = newNode;
            list->head = newNode;
        } else {
            //Si inserisce in mezzo alla lista dove la funzione confronto() ha stabilito
            newNode->next = current;
            newNode->prev = current->prev;
            current->prev->next = newNode;
            current->prev = newNode;
        }
    }

    list->dim_lista++;
}


//Funzione che rimuove node (emergenze) che sono arrivati in timeout: hanno aspettato troppo nella lista e il tempo è scaduto
void rimuovi_timeout(lista_t *list, mtx_t *log_mtx, FILE*flog, rescuer_type_t* tipiSoccorritore){
    if(list == NULL || list->dim_lista == 0) return;    //Nessun elemento nella lista
    
    nodo_t *current = list->tail;   //Nodo ausigliario per scorrere la lista

    //Finche current punta ad un nodo e l'emergenza corrente ha esaurito il tempo, si continua a rimuovere nodi
    while(current != NULL){
        //Se il nodo analizzato contiene un emergenza ancora valida, si salta quest'iterazione
        if(tempo_rimanente(current->emrg) > 0){ 
            current = current->prev;
            continue;
        }

        //Variabile per scrivere il tempo corrente su logFile.txt
        char time_now[BUFF];

        //Documentazione su logFile.txt
        mtx_lock(log_mtx);
            tempo_corrente(time_now);
            FPRINT(fprintf(flog,"[%s] [%d_%s] [EMERGENCY_STATUS] <TIMEOUT: tempo esaurito, emergenza rimasta in attesa per troppo tempo.>\n", \
                            time_now,current->emrg->id, current->emrg->type->emergency_desc),flog, "Errore durante scrittura file LOG da rimuovi_timeout().\n");
        mtx_unlock(log_mtx);

        //Messaggio di rimozione emergenza
        printf("EMERGENZA %d_%s TIMEOUT, rimasta in attesa in lista per troppo tempo\n", current->emrg->id, current->emrg->type->emergency_desc);
        fflush(stdout);
        

        nodo_t *da_cancellare = current; //Nodo ausigliario che punta al nodo da eliminare
        current = current->prev;
        
        if(da_cancellare->prev == NULL){
            //Se il nodo è in testa, la lista da scorrere è finita
            if(da_cancellare->next == NULL){
                //Se non vi sono altri elementi, si svuota la lista
                list->head = list->tail = NULL;
            }else{
                //Si modifica la testa
                list->head = da_cancellare->next;
                list->head->prev = NULL;
            }
        }else{
            //Altrimenti si controlla se è in coda o in mezzo
            if(da_cancellare->next == NULL){
                //Se è in coda, si modifica la coda
                list->tail = da_cancellare->prev;
                list->tail->next = NULL;
            }else{
                //Si rimuove il nodo in mezzo
                da_cancellare->prev->next = da_cancellare->next;
                da_cancellare->next->prev = da_cancellare->prev;
            }
        }

        //Deallocazione memoria nodo da_cancellare
        destroy_emrg_validata(da_cancellare->emrg); //Prima l'emergenza
        free(da_cancellare);        //Poi il nodo stesso
        //Si decrementa il numero di nodi presenti
        list->dim_lista--;
    }

    controllo_situazione(tipiSoccorritore);
}


//Funzione che estra l'emergenza a più alta priorita (la coda)
emergency_t *estrai_nodo(lista_t *list){
    if(list == NULL || list->tail == NULL || list->dim_lista == 0) return NULL;   //Se non ci sono nodi non esegue nulla

    emergency_t *emrg = list->tail->emrg;   //puntatore ad emergenza

    //In caso fosse l'unico nodo
    if(list->dim_lista == 1){
        //Si dealloca il nodo (NON L'EMERGENZA!) e si settano testa e coda a NULL
        free(list->tail);
        list->head = list->tail = NULL;
    }else{
        //Altrimenti si fa diventare il pen'ultimo nodo la nuova coda e si dealloca l'ultimo nodo
        nodo_t *current;
        current = list->tail;
        list->tail = list->tail->prev;
        list->tail->next = NULL;
        free(current);
    }

    //Si decrementa di uno la dimensione della lista
    list->dim_lista--;

    return emrg;
}


//Funzione usata in DEBUG per stampare la lista_emergenze
void stampa_lista(lista_t *list){
    if(list->dim_lista == 0) return;
    
    nodo_t *current = list->head;
    while(current){
        printf("EMERGENZA %s TEMPO %d\n",current->emrg->type->emergency_desc, tempo_rimanente(current->emrg));
        current = current->next;
    }
}