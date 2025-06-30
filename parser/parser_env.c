#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Funzione che processa il file env.conf e ritorna un puntatore a environment_t.
environment_t *parserEnv(){
    //Puntatore che la funzione ritorna come risultato.
    environment_t *ambiente;
    MALLOC(ambiente, (environment_t*)malloc(sizeof(environment_t)), "Errore durante malloc da parse_env.c.\n");

    //Allocazione spazio nome coda
    MALLOC(ambiente->queue_name, (char*)malloc(BUFF*(sizeof(char))), "Errore durante malloc da parse_env.c.\n");
    //Si aggiunge il carattere "/" all'inizio del nome
    ambiente->queue_name[0] = '\0'; 
    strcat(ambiente->queue_name,"/");

    //Variabile che viene settata con la funzione "tempo_corrente()": indica il tempo da scrivere nel logFile.txt durante la documentazioni
    char time_now[BUFF];
    tempo_corrente(time_now);

    //Si aprono i file Log e Conf e si documentano le azioni nel logFile.txt
    FILE *flog, *fin;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_env.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_ENV.c\n");
    FILEOPEN(fin,fopen("conf/env.conf","r"),"Durante Apertura File di Input da parser_env.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <Apertura file env.conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da env.types.c\n");

    //Non si effettua un ciclo perché la formattazione delle 3 righe differiscono tutte, quindi si legge riga per riga
    //e si estrae la relativa informazione documentando l'azione
    int match;  //Match rappresenta il valore di ritorno di sscanf()
    char line[BUFF], *ret, queue_name[BUFF];    //line salverà la riga letta con fgets()
                                                //ret è il valore di ritorno de fgets()
                                                //queue_name conterrà il nome della coda "emergenza690948"

    //Estrazione nome della coda
    FGETS(ret,fgets(line,BUFF-1,fin),fin,"Errore durante fgets da parser-env.c\n");

    //PARSING NOME
    SSCANF(match,1,sscanf(line,"queue=%s[^\n]",queue_name),"Formattazione errata File env.c\n");
    //Si inserisce il nome nella struttura con il "/" a precedere e di documenta l'azione
    strcat(ambiente->queue_name,queue_name); // Si ottiene "/emergenza690948"
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <Lettura file conf: nome queue %s>\n",time_now, ambiente->queue_name),flog, \
                        "Errore durante scrittura fileLog da parser_env.c\n");


    //Estrazione dimensione Height
    FGETS(ret,fgets(line,BUFF-1,fin),fin,"Errore durante fgets da parser-env.c\n");

    //PARSING DIMENSIONE HEIGHT
    SSCANF(match,1,sscanf(line,"height=%d",&ambiente->dimY),"Formattazione errata File env.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <Lettura file conf: height %d>\n",time_now,ambiente->dimY),flog, \
                        "Errore durante scrittura fileLog da parser_env.c\n");
    

    //Estrazione dimensione Width
    FGETS(ret,fgets(line,BUFF-1,fin),fin,"Errore durante fgets da parser-env.c\n");

    //PARSING DIMENSIONE WIDTH
    SSCANF(match,1,sscanf(line,"width=%d",&ambiente->dimX),"Formattazione errata File env.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <Lettura file conf: width %d>\n",time_now, ambiente->dimX),flog, \
                        "Errore durante scrittura fileLog da parser_env.c\n");

    
    //Documentazione di fine operazione riportando i dati ottenuti
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <%s, %d, %d>\n",time_now,ambiente->queue_name,ambiente->dimX, ambiente->dimY), \
                        flog,"Errore durante scrittura fileLog da parser_env.c\n");

    //Documentazione fine programma
    FPRINT(fprintf(flog,"[%s] [PARSER-ENVIRONMENT] [FILE_PARSING] <chiusura programma parser_env.c>\n",time_now),flog,\
                        "Errore durate scrittura fileLog da parser_env.c\n");

    //Chiusura puntatori a file
    fclose(fin);
    fclose(flog);
    
    return ambiente;
}



//Funzione che dealloca lo spazio impiegato per la struttura relativa all'ambiente
void destroy_env(environment_t *target){
    free(target->queue_name);
    free(target);
}