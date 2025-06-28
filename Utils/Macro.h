#define DEBUG
#undef DEBUG

//ERRORE GENERICO
#define ERROR(e) (perror(e), exit(EXIT_FAILURE))

//COSTANTI
#define BUFF 1024   //LUNGHEZZA BUFFER
#define EXIT_MSG "exit" //MESSAGGIO CHIUSURA PROGRAMMA

//MIN E MAX
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))

//SHARED MEMORY
#define SHM_NAME "/memoria_condivisa_690948"
#define SHM_SIZE 1024   //DIM SHARED_MEMORY

//INIT MUTEX/CND_VARIABLES
#define INIT(var_init,e) do { if( var_init != thrd_success ) { ERROR(e); } } while(0)
#define THRDJOIN(ret,res,join,e) do { if((ret = join) != 0 || res != thrd_success) { ERROR(e); } } while(0)

//FILE OPEN/READ/WRITE
#define FILEOPEN(ret,fopen,e) do { if((ret = fopen) == NULL) { ERROR(e); }} while(0)    //
#define FGETS(ret,fgets,fid,e) do { if((ret = fgets) == NULL && !feof(fid)) { ERROR(e); } } while(0)
#define SSCANF(match,expected,sscanf,e) do {if((match = sscanf) != expected) { ERROR(e); }} while(0)
#define FPRINT(fprintf,fid,e) do { if(fprintf < 0){ ERROR(e); } else { fflush(fid); } } while(0)

//MALLOC
#define MALLOC(id,malloc,e) do { if((id = malloc) == NULL ) { ERROR(e); } } while(0)


//Chiamate di sistema:
#define SCALL_ERROR -1
#define SCALL(ret,call,e) do { if((ret = call) == SCALL_ERROR){ ERROR(e); } } while(0)
#define SNCALL(ret,call,e) do { if((ret = call) == NULL) { ERROR(e); } } while(0)
#define SLEEPCALL(ret,call,e) do { if((ret = call) != 0){ ERROR(e); } } while(0)
#define SCALLREAD(ret,loop_cond_op,read_loop_op,e) do { while ((ret = loop_condition_op) > 0) { read_loop_op } if(rescuerTwin == SCALL_ERROR) { ERROR(e); } } while(0)
#define PARENT_OR_CHILD(pid,f_parent,f_child) do { if(pid == 0) { f_child; } else { f_parent; } } while(0)