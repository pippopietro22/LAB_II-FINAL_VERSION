#include<threads.h>
#include<errno.h>
#include<time.h>

// STRUTTURE SOCCORRITORI
#define RESCUER_TYPES 6

typedef enum{
    IDLE,
    EN_ROUTE_TO_SCENE, 
    ON_SCENE, 
    RETURNING_TO_BASE
}rescuer_status_t;

typedef struct{
    char *rescuer_type_name;
    int quantity;
    int speed;
    int x;
    int y;
}rescuer_type_t;

typedef struct{
    int id;
    int idType;
    int x;
    int y;
    rescuer_type_t * rescuer ;
    rescuer_status_t status ;
}rescuer_digital_twin_t;


// STRUTTURE EMERGENZE
#define EMERGENCY_TYPES 10
#define EMERGENCY_NAME_LENGTH 64

typedef enum{
    WAITING,
    ASSIGNED,
    IN_PROGRESS,
    PAUSED,
    COMPLETED,
    CANCELED,
    TIMEOUT
}emergency_status_t;

typedef struct{
    rescuer_type_t* type;
    int required_count;
    int time_to_manage;
}rescuer_request_t;

typedef struct{
    short priority;
    char *  emergency_desc;
    rescuer_request_t * rescuers;
    int rescuers_req_number;
}emergency_type_t;

typedef struct{
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x;
    int y;
    time_t timestamp;
}emergency_request_t;

typedef struct{
    int id;
    emergency_type_t *type;
    emergency_status_t status;
    int x;
    int y;
    time_t timestamp;
    int rescuer_count;
    rescuer_digital_twin_t **rescuers_dt[RESCUER_TYPES];
}emergency_t;

//STRUTTURA AMBIENTE

typedef struct{
    char *queue_name;
    int dimX;
    int dimY;
}environment_t;


//STRUTTURA LISTA
typedef struct nodo{
    emergency_t *emrg;
    struct nodo *next;
    struct nodo *prev;
}nodo_t;

typedef struct{
    nodo_t *head;
    nodo_t *tail;
    int dim_lista;
}lista_t;



//STRUTTURA DATI PER THREADS
#define THRD_OPERATIVI 5

typedef struct{
    char emrg[BUFF];    //Messaggio proveniente dalla message_queue
    FILE *flog;             //Per far scrivere ai thread sul file log solo dopo aver accesso a logFile_mtx
    emergency_type_t *tipiEmergenze;    //Puntatore all'array contenente i vari dipi di emergenza,
                                        //utile per validare il nome dell'emergenza
    environment_t *ambiente;        //Puntatore alla struttura contenente i dati riguardo l'ambiente,
                                    //utlie per valutare le coordinate dell'emergenza
}thrd_data_insert;

typedef struct{
    int id;
    FILE *flog;             //Per far scrivere ai thread sul file log solo dopo aver accesso a logFile_mtx
    emergency_type_t *tipiEmergenze;    //Puntatore all'array contenente i vari dipi di emergenza,
                                        //utile per validare il nome dell'emergenza
    rescuer_type_t *tipiSoccorritori;
    rescuer_digital_twin_t **soccorritori;  //matrice completa con tutti i digital twins per recuperare quelli necessari
                                            //prima di iniziare a risolvere l'emergenza
}thrd_data_operative;

typedef struct{
    rescuer_digital_twin_t * digTwin;
    rescuer_type_t *tipiSoccorritori;
    FILE *flog;
    int time_to_manage;
}thrd_data_socc;






// PROTOTIPI FUNZIONI PARSER

rescuer_type_t *parserRescuers();
void delete_resType(rescuer_type_t *target);
rescuer_digital_twin_t **rescuerTwin(rescuer_type_t *type);
void destroy_resTwin(rescuer_digital_twin_t **twin);


emergency_type_t *parserEmergencies(rescuer_type_t *resType);
void destroy_emrgType(emergency_type_t *target);

environment_t *parserEnv();
void destroy_env(environment_t *target);



//PROTOTIPI FUNZIONI VALIDAZIONE RICHIESTA
emergency_request_t *analisi_richiesta(char *msg, FILE *flog);
void destroy_analisi(emergency_request_t* emg);
emergency_t *validazione_richiesta(emergency_request_t *emg, thrd_data_insert*dati);
void destroy_emrg_validata(emergency_t *val);


//PROTOTIPI FUNZIONI LISTA_EMERGENZE
lista_t *lista_init();
void destroy_list(lista_t *list);
void add_emrg(lista_t *list, emergency_t *emrg);
void rimuovi_timeout(lista_t *list, mtx_t *log_mtx, FILE*flog);
emergency_t *estrai_nodo(lista_t *list);
void stampa_lista(lista_t *list);


//FUNZIONI THRD
int thrd_insert(void *data);
int thrd_operatori(void *data);
int rescuer_on_scene(void *data);
int rescuers_return(void *data);


//PROTOTIPI FUNZIONI AUSIGLIARIE
int distanza_Manhattan(int x1, int y1, int x2, int y2);
int confronto(emergency_t *emrg1, emergency_t *emrg2);
int tempo_rimanente(emergency_t *emrg);
void tempo_corrente(char *buffer);
int tempo_arrivo_soccorsi(emergency_t *emrg);
void controllo_situazione(rescuer_type_t *tipiSoccorritori);