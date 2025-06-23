#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>
#include <sys/wait.h>
#include<unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>

#include "Utils/Macro.h"
#include "Utils/Strutture.h"

void da_rigaComando(char *argv[],mqd_t mq);
void da_file(char *argv[],mqd_t mq);

int main(int argc, char *argv[]){
    //Si ottiene il nome della message_queue tramite shared_memory con il main
    int fd,ret; //fd è un indice riguardante la shared_memory
                //ret è un valore di ritorno usato da munmap e close
    void *ptr;  //ptr è il puntatore per mmap
    //apertura shared_memory
    SCALL(fd,shm_open(SHM_NAME, O_RDWR, 0666),"Errore durante shm_open.\n");
    //Esecuzione mmap
    SNCALL(ptr,mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0),"Errorre durante mmap().\n");

    //Il CLIENT ricava il nome della message queue dalla shared_memory con il main
    char queue_name[BUFF];
    strcpy(queue_name, (char *)ptr);

    //Una volta ottenuto il nome della message_queue, si termina l'utilizzo della shared_memory
    SCALL(ret, munmap(ptr,SHM_SIZE),"Errore durante munmap.\n");
    SCALL(ret,close(fd),"Errore durante close(fd).\n");

    //Apriamo la message queue (già inizializzata dal main)
    mqd_t mq;
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = BUFF;
    attr.mq_curmsgs = 0;

    SCALL(mq,mq_open(queue_name, O_RDWR, 0644, &attr),"Errore durante apertura della message queue.\n");

    //Si analizzano gli argomenti passati durante l'avvio dell'eseguibile ./client
    switch(argc){
        case 2: //Input "exit"
            //Se l'input corrisponde a "exit" allora si invia direttamente
            if(strcmp(argv[1],"exit") == 0) SCALL(ret,mq_send(mq,"exit",strlen("exit") + 1, 0), \
                                                "Errore durante invio messaggio di chiusura.\n");
            break;

        case 3: //Input "-f" "file.txt"
            da_file(argv,mq);
            break;

        case 5: //Input "Emergenza" "X" "Y" "Delay"
            da_rigaComando(argv,mq);
            break;

        default:    //Qualsiasi altro input viene scartato
            ERROR("Formattazione input emergenza errata.\n");
    }

    //Chiusura Message_queue
    SCALL(ret,mq_close(mq),"Errore durante chiusura coda.\n");

    return 0;
}

//Funzione che si occupa di trattare l'input da riga di comando
void da_rigaComando(char *argv[],mqd_t mq){
    int ret;   //ret conterrà i valori di ritorno della mq_send
    char combined[BUFF] = "";   //combined rappresenta il messaggio da mandare con tutti gli argomenti passati

    for(int i = 1; i < 5; i++){
        strcat(combined,argv[i]);           //Si concatenano tutti gli argomenti uno alla volta
        if(i < 4) strcat(combined," ");     //Si aggiunge uno spazio in mezze a ogni coppia di argomenti adiacente
    }

    strcat(combined,"\n");  //Si aggiunge un new line alla fine 

    //Invio messaggio al main
    SCALL(ret,mq_send(mq,combined,strlen(combined) + 1, 0),"Errore durante invio messaggio.\n");    
}

//Funzione che si occupa di input da file
void da_file(char *argv[],mqd_t mq){
    //Controlla che la flag sia "-f"
    if(strcmp(argv[1],"-f")) ERROR("Errore inserimento argomenti emergenza\n");
    
    //Apertura del file
    FILE *fd;
    FILEOPEN(fd,fopen(argv[2],"r"),"Errore durante apertura file.\n");
    
    int int_ret;    //Valore ritorno mq_send
    char *char_ret, line[BUFF]; //Valore ritorno fgets

    //Lettura prima linea del file
    FGETS(char_ret,fgets(line,BUFF,fd),fd,"Errore durante fgets().\n");
    while(char_ret){
        //Invio linea del file
        SCALL(int_ret,mq_send(mq,line,strlen(line) + 1, 0),"Errore durante invio messaggio.\n");

        //Ricomincio il ciclo
        FGETS(char_ret,fgets(line,BUFF,fd),fd,"Errore durante fgets().\n");
    }

    //Chiusura puntatore file
    fclose(fd);
}