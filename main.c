//VERSIONE FINALE <<CONSEGNA>>
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

lista_t *lista_emergenze;   //Puntatore globale (accessibile in ogni parte del codice) alla lista emergenze (tramite mutex)
int soccorritori_liberi[RESCUER_TYPES]; //Variabili accedibile da qualsiasi thread per controllare risorse disponibili (tramite mutex)

mtx_t rescuer_mtx;  //variabili mtx per accesso a risorse (soccorritori_liberi e digital_twins)
mtx_t lista_mtx;    //mtx per accesso lista_emergenze
mtx_t log_mtx;      //mtx per accesso a logFile.txt

cnd_t rescuer_cnd;  //cnd per attesa ripristino risorse
cnd_t lista_cnd;    //cnd per attesa su lista_emergenze (in caso di lista vuota)

atomic_int keep_running = 1;    //Settata a 1: i thrd continuano ad operare. Settata a 0: i thrd terminano
atomic_int id_emrg = 1;         //Identificatore emergenze, letto, assegnato ed incrementato ogni volta che si genera un emergenza
atomic_int thrd_attivi = 0;     //Se si genera un thrd con detach si aumenta di 1. Prima che il thrd termini, si decrementa a -1.
                                //Serve a capire quando tutti i thrd (su cui non si fa una join) terminano per evitare memory leaks.
atomic_int emrg_gestite = 0;    //Indica quante emergenze vengono prese in carico da un thrd per essere gestite, la coda non le conta perché
                                //vengono estratte da un thrd.

//PROTOTIPI FUNZIONI THRD
int thrd_insert(void *data);        //thrd che aggiunge emergenza alla coda (con detach)
int thrd_operatori(void *data);     //thrd che gestisce emergenze: continua ad operare finché non si termina il programma (con join)

int main(){
    //INIZIALIZZAZIONE RISORSE
    rescuer_type_t *tipiSoccorritori = parserRescuers();                    //Array con tipi di soccorritori
    emergency_type_t *tipiEmergenze = parserEmergencies(tipiSoccorritori);  //Array con tipi di emergenze
    environment_t *ambiente = parserEnv();                                  //Struttura dati con dettagli ambiente

    rescuer_digital_twin_t **soccorritori = rescuerTwin(tipiSoccorritori);  //Array composto da N array di gemelli digitali.
                                                                            //Ciascuno di questi array interni rappresenta un tipo rescuer
    //Riempio l'array soccorritori_disponibili con il numero 
    //di soccorritori disponibili letto dal file .conf
    for(int i = 0; i < RESCUER_TYPES; i++){
        soccorritori_liberi[i] = tipiSoccorritori[i].quantity;
    }

    //SHARED MEMORY: serve per condividere il nome della message_queue (letto dal file .conf) al client
    int fd,ret; //fd è l'indice della shared_memory
                //ret è il valore di ritorno di ftruncare(); verrà usato anche come ritorno di altre chiamate
    void *ptr;  //ptr è il valore di ritorno di mmap che servira per passare il nome della message_queue.

    SCALL(fd,shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666),"Errore durante la creazione della memoria condivisa.\n");    
    SCALL(ret,ftruncate(fd,SHM_SIZE),"Errore durante ftruncate");
    SNCALL(ptr,mmap(NULL,SHM_SIZE,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0),"Errore durante mmap().\n");

    //Si scrive sulla memoria condivisa il nome della coda messaggi
    strcpy((char *)ptr, ambiente->queue_name);

    //Fine utilizzo memoria condivisa
    SCALL(ret,munmap(ptr,SHM_SIZE),"Errore durante munmap().\n");
    SCALL(ret,close(fd),"Errore durante close(fd).\n");

    //MESSAGE_QUEUE: creo la coda per ricevere messaggi dal client
    mqd_t mq;
    struct mq_attr attr;
    
    //Setto gli attributi della coda
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = BUFF;
    attr.mq_curmsgs = 0;

    SCALL(mq,mq_open(ambiente->queue_name,O_CREAT | O_RDWR, 0644, &attr),"Errore durante creazione della message queue.\n");

    //INIZIALIZZAZIONE LISTA_EMERGENZE
    lista_emergenze = lista_init(); //Funzione che genera la struttura lista_t e la passa al puntatore lista_emergenze

    //APERTURA FILE LOG
    FILE *flog;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura file LOG da parte di main.c\n");


    //INIZIALIZZO MTX E CND_VARIABLES
    INIT(mtx_init(&rescuer_mtx,mtx_plain),"Errore durante inizializzazione mtx.\n");
    INIT(mtx_init(&log_mtx,mtx_plain),"Errore durante init log_mtx.\n");
    INIT(mtx_init(&lista_mtx,mtx_plain),"Errore durante init coda_mtx.\n");

    INIT(cnd_init(&rescuer_cnd),"Errore durante inizializzazione cnd.\n");
    INIT(cnd_init(&lista_cnd),"Errore durante init di coda_cnd.\n");


    //INIZIALIZZO I THRD OPERATORI
    thrd_t operatori[THRD_OPERATIVI];   //Thrd che gestiranno emergenze, numero settabile dal file Macro.h

    for(int i = 0; i < THRD_OPERATIVI; i++){
        //Per ciascun thrd, devo passare degli argomenti da utilizzare per mezzo di una struttura che alloco dinamicamente
        //per evitare race conditions. Ogni thrd la deallocherà prima di terminare.
        thrd_data_operative *args_op;
        MALLOC(args_op,(thrd_data_operative*)malloc(sizeof(thrd_data_operative)),"Errore durante malloc args_op.\n");

        args_op->id = i+1;      //L'ID del thrd (utile durante il debug)
        args_op->flog = flog;   //Descrittore del logFile.txt
        args_op->tipiSoccorritori = tipiSoccorritori;   //Puntatore array tipi soccorritori
        args_op->tipiEmergenze = tipiEmergenze;     //Puntatore array tipi emergenze
        args_op->soccorritori = soccorritori;      //Puntatore array di array digital twins

        //Creo il thrd
        thrd_create(&operatori[i],thrd_operatori,args_op);
    }

    //Semplice interfaccia di inizio programma con i tipi di comandi da usare
    printf("INSERISCI UN'EMERGENZA (./client EMERGENZA X Y DELAY(in sec)).\n");
    printf("INSERISCI UN FILE (./client -f file_name).\n");
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

        //CHIUDERE IL PROGRAMMA ?
        if(strcmp(buffer,EXIT_MSG) == 0) break;     //Se il messaggio è "exit" termino il ciclo e chiudo il programma. Messaggio settabile da Macro.h

        //Per ciascun thrd, devo passare degli argomenti da utilizzare per mezzo di una struttura che alloco dinamicamente
        //per evitare race conditions. Ogni thrd la deallocherà prima di terminare.
        thrd_data_insert *argomenti;
        MALLOC(argomenti,(thrd_data_insert*)malloc(sizeof(thrd_data_insert)),"Errore durante malloc argomenti.\n");

        argomenti->ambiente = ambiente;             //Puntatore alla struttura dati con i dati ambientali
        argomenti->tipiEmergenze = tipiEmergenze;   //Puntatore all'array con tipi emergenze
        argomenti->flog = flog;                     //Descrittore logFile.txt
        
        //Copio il messaggio dalla message_queue
        strcpy(argomenti->emrg,buffer);   

        //INIZIALIZZO THRD PER INSERIMENTO EMERGENZA
        atomic_fetch_add(&thrd_attivi,1);   //Incremento il contatore dei thrd attivi con detach
        thrd_t insert;  
        thrd_create(&insert,thrd_insert,argomenti);
        thrd_detach(insert);    //Detach: non devo fermare il programma finché il thrd non ha inserito l'emergenza
    }

    //Una volta terminato il programma, setto keep_running = 0, dicendo ai thrd di terminare.
    atomic_store(&keep_running,0);

    //In caso di thrd in attesa su lista_emergenza, gli sveglio per farli terminare
    cnd_broadcast(&lista_cnd);



    //CHIUDO sistemi di comunicazione con il client (MESSAGE_QUEUE e SHARED_MEMORY)
    SCALL(ret,mq_close(mq),"Errore durante chiusura della coda.\n");
    SCALL(ret,shm_unlink(SHM_NAME),"Errore durante shm_unlink.\n");


    

    //Aspetto la terminazione di tutti i thrd operativi con una join
    int res;
    for(int i= 0; i < THRD_OPERATIVI; i++){
        THRDJOIN(ret,res,thrd_join(operatori[i],&res),"Errore thrd_join operatori.\n");
    }

    //Aspetto la terminazione dei thrd a cui ho fatto detach per evitare memory leak
    while(atomic_load(&thrd_attivi) > 0){
        #ifdef DEBUG
            printf("THRD ATTIVI: %d\n",atomic_load(&thrd_attivi));
            fflush(stdout);
        #endif
        //Eseguo una sleep di 2 secondi durante l'attesa attiva per la terminazione
        SLEEPCALL(ret,sleep(2),"Errore durante sleep() da main.\n");
    }

    //Dealloco tutta la memoria usata per la lista: eventuali nodi con emergenze rimaste e la lista stessa.
    destroy_list(lista_emergenze);
    
    
    //DISTRUGGO MUTEX E CND
    mtx_destroy(&rescuer_mtx);  
    mtx_destroy(&log_mtx);
    mtx_destroy(&lista_mtx);

    cnd_destroy(&rescuer_cnd);
    cnd_destroy(&lista_cnd);
    

    //DEALLOCO STRUTTURE
    destroy_resTwin(soccorritori);      //gemelli digitali
    destroy_env(ambiente);              //struttura con dati dell'ambiente e message_queue
    delete_resType(tipiSoccorritori);   //array con tipi soccorritori
    destroy_emrgType(tipiEmergenze);    //array con tipi emergenze

    fclose(flog);      //Chiudo il puntatore a file LOG

    return 0;
}