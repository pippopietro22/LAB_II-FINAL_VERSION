#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Funzione che processa il file emergency_type.conf e ritorna un puntatore ad array emergency_type_t.
emergency_type_t *parserEmergencies(rescuer_type_t *resType){
    //Puntatore che la funzione ritorna come risultato, dimensione settabile da Strutture.h
    emergency_type_t *emergencyType;
    MALLOC(emergencyType,(emergency_type_t*)malloc(EMERGENCY_TYPES*sizeof(emergency_type_t)), "Errore durante malloc da parser_emergency_types.c.\n");

    //Variabile che viene settata con la funzione "tempo_corrente()": indica il tempo da scrivere nel logFile.txt durante la documentazioni
    char time_now[BUFF];
    tempo_corrente(time_now);

    //Si aprono i file Log e Conf e si documentano le azioni nel logFile.txt
    FILE *flog, *fin;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_emergency_types.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_emergency_types.c\n");

    FILEOPEN(fin,fopen("conf/emergency_types.conf","r"),"Durante Apertura File di Input da parser_emergency_types.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Apertura file emergency_types.conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");
    
    //PARSING
    int i = 0, match;   //i conta il numero di emergenze man mano che si legge il file
                        //match è il valore di ritorno di sscanf()
    char *ret;          //ret è il valore di ritorno di fgets()
    char line[BUFF], rescuers[BUFF], *token;    //line salva la riga letta
                                                //rescuers[] salva la parte di riga con soccorritori e attributi relativi
                                                //token serve per strtok (tokenizzazione di rescuers[])

    //Lettura di riga file con FGETS
    FGETS(ret,fgets(line,BUFF-1,fin),fin,"Errore durante fgets da parte di parser_emergency_types.c\n");
    //Documentazione su logFile.txt
    FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] <Lettura file conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");
    //Si continua l'analisi del file per mezzo di un ciclo while
    while(ret){
        //Alloco spazio per il nome dell'emergenza
        MALLOC(emergencyType[i].emergency_desc, (char*)malloc(BUFF*sizeof(char)), "Errore durante malloc da parser_emergency_types.c\n");

        //Parsing linea: nelle relative variabili (alcune già di struttura) salvo le informazioni
        SSCANF(match,3,sscanf(line,"[%99[^]]] [%hi] %99[^\n]", \
                        emergencyType[i].emergency_desc, &emergencyType[i].priority, rescuers), \
                        "Formattazione errata file emergency_types.conf\n");
        
        //Adesso si alloca memoria per la proprietà "rescuers" che rappresenta un array di soccorsi richiesti, ciascuno con 
        //le relative informazioni (quantità richiesta etc.).
        //Questo processo si esegue per mezzo di Tokenizzazione, dato che la quantità di soccorsi richiesti non è fissata, ma conosciamo
        //la dimensione dell'array: RESCUER_TYPES (macro definita in Strutture.h)
        int socc = 0;   //Numero di tipi soccorsi richiesti

        MALLOC(emergencyType[i].rescuers, (rescuer_request_t*)malloc(RESCUER_TYPES*sizeof(rescuer_request_t)), "Errore durante malloc da parser_emergency_types.c\n");
        
        //Inizializzo tutti i puntatori a tipi di soccorso NULL.
        //Successivamente, le celle dell'array relative soccorsi richiesti dall'emergenza verranno popolati da dati validi
        for(int j = 0; j < RESCUER_TYPES; j++){
            emergencyType[i].rescuers[j].type = NULL;
            emergencyType[i].rescuers[j].required_count = 0;
            emergencyType[i].rescuers[j].time_to_manage = 0;
        }

        //Tokenizzazione parte di linea relativa a soccorritori
        token = strtok(rescuers,";");   
        while(token){
            char name[BUFF];    //name conterrà il nome dell'emergenza
            int count, time_to_manage;  //count conterrà il numero di soccorritori di un dato tipo
                                        //time_to_manage conterrà il tempo necessarrio di ciascun soccorritore a completare l'emergenza
            
            //Parsing soccorritori
            SSCANF(match,3,sscanf(token,"%99[^:]:%d,%d",name, &count, &time_to_manage), \
                        "Formattazione errata file emergency_types.conf\n");

            //Per associare un tipo di soccorso(rescuer_type_t), bisogna confrontare il nome (del soccorso richiesto) indicato 
            //nell'emergenza con quello di tutti i tipi di soccorso finché non si trova una corrispondenza
            for(int j = 0; j < RESCUER_TYPES; j++){
                //Se il nome non corrisponde, si passa al tipo successivo
                if(strcmp(name,resType[j].rescuer_type_name) != 0) continue; 

                emergencyType[i].rescuers[j].type = &resType[j];
                emergencyType[i].rescuers[j].required_count = count;
                emergencyType[i].rescuers[j].time_to_manage = time_to_manage;
                    
            }
            if(socc == RESCUER_TYPES) ERROR("Errore nel file emergency_types.conf");    //Se non trova corrispondenza con il nome di un soccorritore, da errore.

            //Si riparte con la tokenizzazione
            token = strtok(NULL,";");
            socc++;    //socc si incrementa ogni volta che abbiamo aggiunto un tipo di soccorritore richiesto
        }
        //socc rappresenta il numero di soccorritori richiesti
        emergencyType[i++].rescuers_req_number = socc;  //Si conta l'iterazione eseguita con i++, quindi il tipo di emergenza parsata

        //Si documenta la fine dell'iterazione
        FPRINT(fprintf(flog,"[%s] [PARSER-EMERGENCIES] [FILE_PARSING] Parsing dati emergency_types.conf: %s",time_now,line),flog, \
                        "Errore durante scrittura fileLog da parser_emergency_types.c\n");

        //Il ciclo ricomincia con una nuova linea
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