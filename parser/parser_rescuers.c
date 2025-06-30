#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Funzione che processa il file rescuers.conf e ritorna un puntatore ad array rescuer_type_t.
rescuer_type_t *parserRescuers(){
    //Puntatore che la funzione ritorna come risultato, dimensione settabile da Strutture.h
    rescuer_type_t *rescuerType;
    MALLOC(rescuerType, (rescuer_type_t*)malloc(RESCUER_TYPES*sizeof(rescuer_type_t)), "Errore durante malloc da pareser_rescue.c.\n");

    //Variabile che viene settata con la funzione "tempo_corrente()": indica il tempo da scrivere nel logFile.txt durante la documentazioni
    char time_now[BUFF];
    tempo_corrente(time_now);

    //Si aprono i file Log e Conf e si documentano le azioni nel logFile.txt
    FILE *fin, *flog;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_rescuers.c\n");
    
    FILEOPEN(fin, fopen("conf/rescuer.conf","r"), "Errore durante apertura File di Input da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Apertura file rescuer.conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");

    int i = 0, match;   //i conterrà il numero di soccorritori man mano che si legge il file
                        //match è il valore di ritorno di sscanf
    char *ret;          //ret è il valore di ritorno di fgets()
    char line[BUFF];    //line conterrà la linea letta

    //Lettura riga file con FGETS
    FGETS(ret, fgets(line,BUFF-1,fin), fin, "Errore durante fgets da parser_rescuers.c\n");
    //Documentazione su logFile.txt
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Lettura file conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");
    
    //Si continua l'analisi del file per mezzo di un ciclo while
    while(ret){
        //Alloco spazio per nome tipo soccorritore
        MALLOC(rescuerType[i].rescuer_type_name,(char*)malloc(BUFF*sizeof(char)), "Errore durante malloc da pareser_rescue.c.\n");
        
        //Parsing linea: nelle relative variabili (alcune già di struttura) salvo le informazioni
        SSCANF(match, 5, sscanf(line,"[%99[^]]] [%d] [%d] [%d;%d]",rescuerType[i].rescuer_type_name, &rescuerType[i].quantity, \
                        &rescuerType[i].speed, &rescuerType[i].x, &rescuerType[i].y), \
                        "Formattazione errata file rescuers.conf\n");

        //Documentazione su logFile.txt
        FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] parsing file rescuer.conf: %s",time_now,line),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");

        //Alla fine dell'iterazione, si incrementa il contatore "i" che funge anche da indice nell'array
        i++;

        //Ricomincia il ciclo
        //Lettura linea
        FGETS(ret, fgets(line,BUFF-1,fin), fin, "Errore durante fgets da parser_rescuers.c\n");
        //Documentazione su logFile.txt
        FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Lettura file conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");
    }

    //Documentazione fine programma
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FINE_PARSING] <Chiusura programma parser_rescuers.c>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");

    //Chiusura puntatori a file
    fclose(fin);
    fclose(flog);
    return rescuerType;
}

//Funzione dedita alla deallocazione di memoria della struttura creata soprastante
void delete_resType(rescuer_type_t *target){
    for(int i = 0; i < RESCUER_TYPES; i++){
        free(target[i].rescuer_type_name);
    }
    free(target);
}

//Funzione dedita alla creazione di gemelli digitali
rescuer_digital_twin_t **rescuerTwin(rescuer_type_t *resType){
    //Ritorna un puntatore a puntatori di tipo rescuer_digital_twin_t
    //la matrice è suddivisa in righe: ciascuna rappresentante un tipo di soccorso (nello stesso ordine in cui sono salvati i tipi di soccorsi)
    //Ogni riga ha un numero di soccorsi specificato nel file conf
    rescuer_digital_twin_t **resPoint;
    MALLOC(resPoint, (rescuer_digital_twin_t**)malloc(RESCUER_TYPES*sizeof(rescuer_digital_twin_t*)), "Errore durante malloc da parser_rescuer.c.\n");

    //Variabile che viene settata con la funzione "tempo_corrente()": indica il tempo da scrivere nel logFile.txt durante la documentazioni
    char time_now[BUFF];
    tempo_corrente(time_now);  

    //Si aprono i file Log e Conf e si documentano le azioni nel logFile.txt
    FILE *flog;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS-TWINS] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_rescuers.c\n");

    //Si fa un ciclo per allocare sulla matrice, scorrendo le righe e allocando la quantità di gemelli per il dato tipo di soccorso
    for(int j = 0; j < RESCUER_TYPES; j++){
        MALLOC(resPoint[j], (rescuer_digital_twin_t*)malloc(resType[j].quantity*sizeof(rescuer_digital_twin_t)), "Errore durante malloc da parser_rescuer.c.\n");

        //nel ciclo interno si definiscono le proprietà di ogni gemello (in particolare l'"id")
        for(int i = 0; i < resType[j].quantity; i++){
            resPoint[j][i].id = i+1;        //Identificatore soccorritore>
            resPoint[j][i].idType = j;        //Identificatore del tipo di soccorritore
            resPoint[j][i].x = resType[j].x;    //Coordinate della base
            resPoint[j][i].y = resType[j].y;
            resPoint[j][i].rescuer = &resType[j];   //Puntatore struttura dati del tipo soccorritore
            resPoint[j][i].status = IDLE;   //Label contenente lo status del soccorritore
        }

        //E, prima di passare al tipo di gemelli successivo, si documenta la creazione
        FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS-TWINS] [FILE_PARSING] <Sono stati creati %d gemelli digitali per %s>\n", \
                        time_now, resType[j].quantity,resType[j].rescuer_type_name),flog,
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");
    }

    //Documentazione di fine creazione gemelli
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS-TWINS] [FILE_PARSING] <Creazione gemelli digitali completata>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_rescuers.c\n");

    //Chiusura descrittore logFile.txt
    fclose(flog);
    
    return resPoint;
}

//Funzione dedita alla deallocazione di memoria del doppio puntatore relativo ai gemelli digitali
void destroy_resTwin(rescuer_digital_twin_t **twins){
    for(int i = 0; i < RESCUER_TYPES; i++){
        free(twins[i]);
    }
    free(twins);
}