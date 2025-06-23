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

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Funzione relativa all'interpretazione del file emergency_type.conf che ritorna un puntatore emergency_type_t che indica
//un array di tipi di emergenza, ciascuna con le relative informazioni
emergency_type_t *parserEmergencies(rescuer_type_t *resType){
    emergency_type_t *emergencyType;
    MALLOC(emergencyType,(emergency_type_t*)malloc(EMERGENCY_TYPES*sizeof(emergency_type_t)), "Errore durante malloc da parser_emergency_types.c.\n");

    char time_now[BUFF];
    tempo_corrente(time_now);

    //Si aprono i file Log e Conf e si documenta l'azione immediatamente
    FILE *flog, *fin;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_emergency_types.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_emergency_types.c\n");

    FILEOPEN(fin,fopen("conf/emergency_types.conf","r"),"Durante Apertura File di Input da parser_emergency_types.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Apertura file emergency_types.conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");
    
    //Si inizia ad interpretare il file Conf riga per riga per mezzo di un ciclo while
    int i = 0, match;   //i conta il numero di righe (quindi di emergenze)
                        //match è il valore di ritorno di sscanf(), ovvero il numero di valori di match
    char *ret;          //ret è il valore di ritorno di fgets()
    char line[BUFF], rescuers[BUFF], *token;    //line salva la riga letta
                                                //rescuers salva la parte di riga con soccorritori e attributi relativi
                                                //token serve per strtok
    FGETS(ret,fgets(line,BUFF-1,fin),fin,"Errore durante fgets da parte di parser_emergency_types.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Lettura file conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");
    while(ret){
        //Alloco spazio per il nome dell'emergenza
        MALLOC(emergencyType[i].emergency_desc, (char*)malloc(BUFF*sizeof(char)), "Errore durante malloc da parser_emergency_types.c\n");

        //Parsing linea
        SSCANF(match,3,sscanf(line,"[%99[^]]] [%hi] %99[^\n]", \
                        emergencyType[i].emergency_desc, &emergencyType[i].priority, rescuers), \
                        "Formattazione errata file emergency_types.conf\n");
        line[strlen(line) - 3] = '\0';
        rescuers[strlen(rescuers)-2] ='\0';     //bisogna mettere un '\0' alla fine di modo che la tokenizzazione avvenga senza problemi
        
        //Adesso si alloca memoria per la proprietà "rescuers" che rappresenta un array di soccorsi richiesti, ciascuno con 
        //le relative informazioni (quantità richiesta etc.).
        //Questo processo si esegue per mezzo di Tokenizzazione, dato che la quantità di soccorsi richiesti non è fissata, ma conosciamo
        //il massimo, ovvero RESCUER_TYPES (macro definita in Strutture.h)
        int socc = 0;   //Numero di tipi soccorsi richiesti

        MALLOC(emergencyType[i].rescuers, (rescuer_request_t*)malloc(RESCUER_TYPES*sizeof(rescuer_request_t)), "Errore durante malloc da parser_emergency_types.c\n");
        
        //Inizializzo tutti i puntatori a tipi di soccorso NULL.
        //Questo perché, solamente i membri dell'array corrispondenti ai soccorsi presenti (secondo l'ordine in cui vengono salvati
        //i tipi da parser_rescuers.c) avranno valori validi da leggere.
        for(int j = 0; j < RESCUER_TYPES; j++){
            emergencyType[i].rescuers[j].type = NULL;
            emergencyType[i].rescuers[j].required_count = 0;
            emergencyType[i].rescuers[j].time_to_manage = 0;
        }
        token = strtok(rescuers,";");   //Tokenizzazione parte di linea relativa a soccorritori
        while(token){
            char name[BUFF];    //name conterrà il nome dell'emergenza
            int count, time_to_manage;  //count conterrà il numero di soccorritori di un dato tipo
                                        //time_to_manage conterrà il tempo necessarrio di ciascun soccorritore
            
            //Parsing soccorritori
            SSCANF(match,3,sscanf(token,"%99[^:]:%d,%d",name, &count, &time_to_manage), \
                        "Formattazione errata file emergency_types.conf\n");

            //Per associare un tipo di soccorso(rescuer_type_t), bisogna confrontare il nome (del soccorso richiesto) indicato 
            //nell'emergenza con quello di tutti i tipi di soccorso finché non si trova una corrispondenza
            for(int j = 0; j < RESCUER_TYPES; j++){
                if(strcmp(name,resType[j].rescuer_type_name) == 0){
                    emergencyType[i].rescuers[j].type = &resType[j];
                    emergencyType[i].rescuers[j].required_count = count;
                    emergencyType[i].rescuers[j].time_to_manage = time_to_manage;
                    break;
                }
            }
            if(socc == RESCUER_TYPES) ERROR("Errore nel file emergency_types.conf");    //Se non trova corrispondenza con il nome di
                                                                                        //soccorritore, da errore.
            token = strtok(NULL,";");
            socc++;    //socc si incrementa ogni volta che abbiamo aggiunto un tipo di soccorritore richiesto
        }
        //socc rappresenta il numero di soccorritori richiesti
        emergencyType[i].rescuers_req_number = socc;

        //Si documenta la fine dell'iterazione
        FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Parsing dati emergency_types.conf: %s>\n",time_now,line),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");

        //Si conta l'iterazione eseguita, quindi il tipo di emergenza parsata
        i++;

        //Il ciclo ricomincia
        FGETS(ret,fgets(line,BUFF-1,fin),fin,"Errore durante fgets da parser_emergency_types.c\n");
        FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Lettura file conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");
    }

    //Documentazione di fine programma
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <chiusura programma parser_emergency_types.c>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");

    //Chiusura puntatori a file
    fclose(fin);
    fclose(flog);

    return emergencyType;
}

//Funzione dedicata alla deallocazione del puntatore ad array rappresentante i tipi di emergenza
void destroy_emrgType(emergency_type_t *target){
    for(int i = 0; i < EMERGENCY_TYPES; i++){
        free(target[i].emergency_desc);
        free(target[i].rescuers);
    }
    free(target);
}