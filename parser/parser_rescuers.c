#include "../Utils/Macro.h"
#include "../Utils/Strutture.h"

//Funzione incaricata di interpretare il file rescuer.conf per restituire un puntatore ad array di "rescuer_type_t" contenente 
//tutti i tipi di soccorritore presenti nel file
rescuer_type_t *parserRescuers(){
    rescuer_type_t *rescuerType;    //Array con tutti i tipi di soccorritori
    MALLOC(rescuerType, (rescuer_type_t*)malloc(RESCUER_TYPES*sizeof(rescuer_type_t)), "Errore durante malloc da pareser_rescue.c.\n");

    char time_now[BUFF];
    tempo_corrente(time_now);

    //Si aprono i file Log e Conf e si documenta l'azione immediatamente
    FILE *fin, *flog;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_rescuers.c\n");
    
    FILEOPEN(fin, fopen("conf/rescuer.conf","r"), "Errore durante apertura File di Input da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Apertura file rescuer.conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");

    int i = 0, match;   //i conterrà il numero di righe lette => numero di tipi soccorritore
                        //match è il valore di ritorno di sscanf, numero corrispondenze 
    char *ret;          //ret è il valore di ritorno di fgets()
    char line[BUFF];    //line conterrà la linea letta

    //Lettura linea
    FGETS(ret, fgets(line,BUFF-1,fin), fin, "Errore durante fgets da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] <Lettura file conf>\n",time_now),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");

    while(ret){
        //Alloco spazio per nome tipo soccorritore
        MALLOC(rescuerType[i].rescuer_type_name,(char*)malloc(BUFF*sizeof(char)), "Errore durante malloc da pareser_rescue.c.\n");
        
        //Parsing linea
        SSCANF(match, 5, sscanf(line,"[%99[^]]] [%d] [%d] [%d;%d]",rescuerType[i].rescuer_type_name, &rescuerType[i].quantity, \
                        &rescuerType[i].speed, &rescuerType[i].x, &rescuerType[i].y), \
                        "Formattazione errata file rescuers.conf\n");

        FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS] [FILE_PARSING] parsing file rescuer.conf: %s",time_now,line),flog, \
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");

        //Si incrementa il contatore "i" che funge anche da indice nell'array
        i++;

        //Ricomincia il ciclo
        //Lettura linea
        FGETS(ret, fgets(line,BUFF-1,fin), fin, "Errore durante fgets da parser_rescuers.c\n");

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

//Funzione dedita alla creazione di un puntatore a puntarore di gemelli digitali:
//il primo puntatore indica un array, ciascuno relativo ad un tipo di soccorritore;
//i puntatori interni indicano un array di gemelli digitali relativi al tipo di soccorritore
rescuer_digital_twin_t **rescuerTwin(rescuer_type_t *type){
    rescuer_digital_twin_t **resPoint;
    MALLOC(resPoint, (rescuer_digital_twin_t**)malloc(RESCUER_TYPES*sizeof(rescuer_digital_twin_t*)), "Errore durante malloc da parser_rescuer.c.\n");

    char time_now[BUFF];
    tempo_corrente(time_now);  

    //File Log per documentare le azioni
    FILE *flog;
    FILEOPEN(flog,fopen("logFile.txt","a"),"Errore durante apertura File Log da parser_rescuers.c\n");
    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS-TWINS] [FILE_PARSING] <Apertura file LOG>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_rescuers.c\n");

    //Si fa un ciclo per allocare, ad ogni cella dell'array esterno, lo spazio per la giusta quantità di gemelli
    for(int j = 0; j < RESCUER_TYPES; j++){
        MALLOC(resPoint[j], (rescuer_digital_twin_t*)malloc(type[j].quantity*sizeof(rescuer_digital_twin_t)), "Errore durante malloc da parser_rescuer.c.\n");

        //nel ciclo interno si definiscono le proprietà di ogni gemello (in particolare l'"id")
        for(int i = 0; i < type[j].quantity; i++){
            resPoint[j][i].id = i+1;
            resPoint[j][i].idType = j;
            resPoint[j][i].x = type[j].x;
            resPoint[j][i].y = type[j].y;
            resPoint[j][i].rescuer = &type[j];
            resPoint[j][i].status = IDLE;
        }

        //E, prima di passare al tipo di gemelli successivo, si documenta la creazione
        FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS-TWINS] [FILE_PARSING] <Sono stati creati %d gemelli digitali per %s>\n", \
                        time_now, type[j].quantity,type[j].rescuer_type_name),flog,
                        "Errore durante scrittura fileLog da parser_rescuers.c\n");
    }

    FPRINT(fprintf(flog,"[%s] [PARSER-RESCUERS-TWINS] [FILE_PARSING] <Creazione gemelli digitali completata>\n",time_now),flog, \
                        "Errore durate scrittura fileLog da parser_rescuers.c\n");

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