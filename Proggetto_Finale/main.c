#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>
#include<sys/wait.h>
#include<mqueue.h>
#include<threads.h>
#include<stdatomic.h>

#include "Utils/Macro.h"
#include "Utils/Strutture.h"

lista_t *lista_emergenze;
int soccorritori_liberi[RESCUER_TYPES]; //Variabili accedibile da qualsiasi thread per controllare risorse disponibili (tramite mutex)

mtx_t rescuer_mtx;  //variabili mtx per risorse
mtx_t lista_mtx;    //mtx per accesso lista emergenze
mtx_t log_mtx;      //mtx per accesso a file LOG
cnd_t rescuer_cnd;  //cnd per attesa ripristino risorse
cnd_t lista_cnd;    //cnd per attesa lista emergenze

atomic_int keep_running = 1;
atomic_int id_emrg = 1;
atomic_int thrd_attivi = 0;

//PROTOTIPI FUNZIONI THRD
int thrd_insert(void *data);
int thrd_operatori(void *data);
int rescuers_return(void *data);

int main(){
    rescuer_type_t *tipiSoccorritori = parserRescuers();                    //Array con tipi di soccorritori
    emergency_type_t *tipiEmergenze = parserEmergencies(tipiSoccorritori);  //Array con tipi di emergenze
    environment_t *ambiente = parserEnv();                                  //Struttura dati con dettagli ambiente

    rescuer_digital_twin_t **soccorritori = rescuerTwin(tipiSoccorritori);  //Array composto da N array di gemelli digitali.
                                                                            //Ciascuno di questi array interni rappresenta un tipo rescuer
    //Inizializzazione risorse disponibili
    for(int i = 0; i < RESCUER_TYPES; i++){
        soccorritori_liberi[i] = tipiSoccorritori[i].quantity;
    }

    //SHARED MEMORY: condivisione nome message_queu al client
    int fd,ret; //fd è l'indice della shared_memory
                //ret è il valore di ritorno di ftruncare() e altre chiamate
    void *ptr;  //ptr è il valore di ritorno di mmap

    SCALL(fd,shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666),"Errore durante la creazione della memoria condivisa.\n");    
    SCALL(ret,ftruncate(fd,SHM_SIZE),"Errore durante ftruncate");
    SNCALL(ptr,mmap(NULL,SHM_SIZE,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0),"Errore durante mmap().\n");

    //Si scrive sulla memoria condivisa il nome della coda
    strcpy((char *)ptr, ambiente->queue_name);

    //Fine utilizzo memoria condi
    SCALL(ret,munmap(ptr,SHM_SIZE),"Errore durante munmap().\n");
    SCALL(ret,close(fd),"Errore durante close(fd).\n");

    //MESSAGE_QUEUE: creo la coda per ricevere messaggi dal client
    mqd_t mq;           //mq è indice della message_queue
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = BUFF;
    attr.mq_curmsgs = 0;

    SCALL(mq,mq_open(ambiente->queue_name,O_CREAT | O_RDWR, 0644, &attr),"Errore durante creazione della message queue.\n");

    //INIZIALIZZAZIONE LISTA_EMERGENZE

    lista_emergenze = lista_init();

    //APERTURA FILE LOG
    FILE *flog;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura file LOG da parte di main.c\n");


    //INIZIALIZZO MTX E CND_VARIABLES

    INIT(mtx_init(&rescuer_mtx,mtx_plain),"Errore durante inizializzazione mtx.\n");
    INIT(cnd_init(&rescuer_cnd),"Errore durante inizializzazione cnd.\n");
    
    INIT(mtx_init(&log_mtx,mtx_plain),"Errore durante init log_mtx.\n");
    INIT(mtx_init(&lista_mtx,mtx_plain),"Errore durante init coda_mtx.\n");
    INIT(cnd_init(&lista_cnd),"Errore durante init di coda_cnd.\n");


    //INIZIALIZZO I THRD OPERATORI
    thrd_t operatori[THRD_OPERATIVI];
    
    for(int i = 0; i < THRD_OPERATIVI; i++){
        thrd_data_operative *args_op;
        MALLOC(args_op,(thrd_data_operative*)malloc(sizeof(thrd_data_operative)),"Errore durante malloc args_op.\n");

        args_op->id = i+1;
        args_op->flog = flog;
        args_op->tipiSoccorritori = tipiSoccorritori;
        args_op->tipiEmergenze = tipiEmergenze;
        args_op->soccorritori = soccorritori;

        thrd_create(&operatori[i],thrd_operatori,args_op);
    }

    printf("INSERISCI UN'EMERGENZA (./client [EMERGENZA] [COORD X] [COORD Y] [DELAY (in sec)]).\n");
    printf("INSERISCI UN FILE (./client -f [file_name]).\n");
    printf("TERMINA L'ESECUZIONE (./client exit).\n");
    fflush(stdout);

    //RICEZIONE MESSAGGI
    while(1){
        ssize_t bytesRead;  //bytesRead è il valore di ritorno di mq_receive()
        char buffer[BUFF];  //buffer è dove salvo il messaggio proveniente dal client

        //RICEVO MESSAGGIO DA CLIENT
        SCALL(bytesRead, mq_receive(mq,buffer,BUFF,NULL),"Errore di ricezione message queue.\n");

        #ifdef DEBUG
            printf("RICEVUTO: %s\n",buffer);
            fflush(stdout); 
        #endif

        //Se il messaggio è "exit" termino il ciclo e chiudo il programma
        if(strcmp(buffer,EXIT_MSG) == 0) break;     //CONTROLLO SE CHIUDERE IL PROGRAMMA

        //inizializzo gli argomenti per il thrd. Alloco una struttura nuova per ciascun thrd di modo
        //da evitare race condition, per poi farla deallocare al thrd una volta svolto il suo compito
        thrd_data_insert *argomenti;
        MALLOC(argomenti,(thrd_data_insert*)malloc(sizeof(thrd_data_insert)),"Errore durante malloc argomenti.\n");

        //Memorizzo i puntatore alla struttura ambiente e ai tipi emergenze
        argomenti->ambiente = ambiente;    
        argomenti->tipiEmergenze = tipiEmergenze;
        argomenti->flog = flog;
        //Copio il messaggio dalla message_queue
        strcpy(argomenti->emrg,buffer);   

        thrd_t insert;
        atomic_fetch_add(&thrd_attivi,1);
        thrd_create(&insert,thrd_insert,argomenti);
        thrd_detach(insert);
    }

    //Una volta terminato il programma, setto keep_running a 0 (interrompo il ciclo while) e sveglio tutti i thrd per l'uscita
    atomic_store(&keep_running,0);
    cnd_broadcast(&lista_cnd);

    //Aspetto la terminazione di tutti i thrd operativi
    int res;
    for(int i= 0; i < THRD_OPERATIVI; i++){
        THRDJOIN(ret,res,thrd_join(operatori[i],&res),"Errore thrd_join operatori.\n");
    }

    //Aspetto la terminazione dei thrd a cui ho fatto detach in caso terminassi il programma prima che abbia finito
    while(atomic_load(&thrd_attivi) > 0){
        #ifdef DEBUG
            printf("THRD ATTIVI: %d\n",atomic_load(&thrd_attivi));
            fflush(stdout);
            SLEEPCALL(ret,sleep(2),"Errore durante sleep() da main.\n");
        #endif
    }

    printf("ESECUZIONE TERMINATA, PULENDO LA MEMORIA.\n");
    fflush(stdout);

    //Dealloco tutta la memoria usata per la lista, eventuali nodi ed emergenze rimaste
    destroy_list(lista_emergenze);


    //CHIUDO sistemi di comunicazione con il client (MESSAGE_QUEUE e SHARED_MEMORY)
    SCALL(ret,mq_close(mq),"Errore durante chiusura della coda.\n");
    SCALL(ret,shm_unlink(SHM_NAME),"Errore durante shm_unlink.\n");
    
    
    //DISTRUGGO MUTEX E CND
    mtx_destroy(&rescuer_mtx);
    cnd_destroy(&rescuer_cnd);
    mtx_destroy(&log_mtx);
    mtx_destroy(&lista_mtx);
    cnd_destroy(&lista_cnd);
    

    //DEALLOCO STRUTTURE
    destroy_resTwin(soccorritori);      //gemelli digitali
    destroy_env(ambiente);              //struttura con dati dell'ambiente e message_queue
    delete_resType(tipiSoccorritori);   //array con tipi soccorritori
    destroy_emrgType(tipiEmergenze);    //array con tipi emergenze

    //Chiudo il puntatore a file LOG
    fclose(flog);

    return 0;
}