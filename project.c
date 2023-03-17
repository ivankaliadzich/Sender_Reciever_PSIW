#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define max_size 50
#define max_ans_size 2000

struct msgbuf {
    long type;
    char cmd[max_size];
    char fifo[max_size];
} mes;

void sender() {
    char buf[3 * max_size], rec[max_size], cmd[max_size], fifo[max_size], ans[max_ans_size];
    

    while (true) {
        memset(buf, 0, sizeof(buf));
        memset(rec, 0, sizeof(rec));
        memset(cmd, 0, sizeof(cmd));
        memset(fifo, 0, sizeof(fifo));
        memset(ans, 0, sizeof(ans));

        
	//CZYTANIE PLIKU TXT
        scanf("%[^\n]s", buf); 
        scanf("%*c");          
 
        int i = 0, size = strlen(buf), good_input=1;

        while (buf[i] != ' '){
            if (++i >= size){
             good_input = 0;
            }
        }    

        if (++i >= size || buf[i] != '"'){
           good_input = 0;
        }

        if (++i >= size){
            good_input = 0;
        }
        
        while (buf[i] != '"'){
            if (++i >= size){
                good_input = 0;
            }
        }

        if (++i >= size || buf[i] != ' '){        
            good_input = 0;
        }

        if (++i >= size || buf[i] != '"'){
            good_input = 0;
        }
    
        if (++i >= size){
          good_input = 0;
        }
      
        while (buf[i] != '"'){
            if (++i >= size){
                good_input = 0;
            }       
        }

        if (++i != size){
            good_input = 0;
        }
            
        if (good_input == 0) {
            printf("Podano bledne dane wejsciowe!\n");
            continue;
        }
        
	//CZYTANIE CO TRZEBA ZROBIC KOLEJKIE
        int q=0, j=0;
        
        while (buf[q] != ' '){
            rec[j++] = buf[q++];  
        }

        q += 2;
        j = 0;
        while (buf[q] != '"'){
            cmd[j++] = buf[q++];
        }
            
        q += 3;
        j = 0;
        while (buf[q] != '"'){
            fifo[j++] = buf[q++];
        }

        printf("\nOdbiornik: %s,\nProces: %s,\nOdpowiedz kolejki FIFO: %s.\n\n", rec, cmd, fifo);

        // TWORZENIE KOLEJKI
        if (mkfifo(fifo, 0666) < 0) {
            perror("Blad podczas tworzenia kolejki FIFO");
            continue;
        }

        mes.type = 1;
        strcpy(mes.cmd, cmd);
        strcpy(mes.fifo, fifo);

	// ODPOWIEDZ
    int qkey = 0;

    int ff = open("users_for_projekt.txt", O_RDONLY);
    if (ff < 1)
    {
        perror("Blad otworzania pliku:");
        qkey = -1;
    }

    char buf[500];
     i = 0;

   
    while (read(ff, &buf[i], 1) > 0)
        i++;


    char *p = strstr(buf, rec);
    if (p == NULL)
    {
        printf("Nie ma takiego procesu\n");
        close(ff);
        qkey = -1;
    }

    if (buf[p - buf + strlen(rec)] != ' ' || (p - buf - 1 >= 0 && buf[p - buf - 1] != '\n'))
    {
        printf("Zla nazwa procesu\n");
        close(ff);
        qkey = -1;
    }

    i = p - buf + strlen(rec) + 3; // beginning of the QKEY
    
    while (buf[i] != '\n')
    {
        qkey *= 10;
        qkey += buf[i] - '0';
        i++;
    }

    close(ff);
    

        int rec_msg_q_key = qkey, rec_msg_q_id = msgget(rec_msg_q_key, IPC_CREAT | 0666);
        if (rec_msg_q_key < 0 || rec_msg_q_id < 0) {
            printf("Blad podczas znajdowania klucza kolejki komunikatow odbiorcy\n");
            unlink(fifo);
            continue;
        }

        if (msgsnd(rec_msg_q_id, &mes, sizeof(mes), 0) < 0) {
            perror("Blad podczas wysylania wiadomosci");
            unlink(fifo);
            continue;
        }

        int fifo_fd = open(fifo, O_RDONLY);
        if (fifo_fd < 0) {
            perror("Blad podczas otwierania kolejki FIFO");
            unlink(fifo);
            continue;
        }

        if (read(fifo_fd, ans, max_ans_size) < 0) {
            perror("Blad podczas odczytu z kolejki FIFO");
            unlink(fifo);
            continue;
        }

        close(fifo_fd);
        unlink(fifo);

        printf("\nOdpowiedz kolejki FIFO: \n%s\n", ans);
    }
}

void receiver(int qid) {
    while (true) {
        if (msgrcv(qid, &mes, sizeof(mes), 1, 0) < 0) {
            perror("Blad podczas odbierania wiadomosci");
            continue;
        }

        printf("\nPoczatek\n: Otrzymal proces %s\n", mes.cmd);

        char *argv1[50] = {}, *argv2[50] = {};
        char *p = strtok(mes.cmd, " ");
        int i = 0;

        while (p != NULL && strcmp(p, "|") != 0) {
            argv1[i++] = p;
            p = strtok(NULL, " ");
        }
        argv1[i] = NULL;

        i = 0;
        p = strtok(NULL, " ");
        while (p != NULL && strcmp(p, "|") != 0) {
            argv2[i++] = p;
            p = strtok(NULL, " ");
        }
        argv2[i] = NULL;

        int fifo_fd = open(mes.fifo, O_WRONLY);
        if (fifo_fd < 0) {
            perror("Blad otworzenia kolejki FIFO");
            continue;
        }

        if (fork() == 0) {
            printf("Zrobiobe\n");

            if (argv2[0] == NULL)  {
                close(1);        
                dup2(fifo_fd, 1);
                execvp(argv1[0], argv1);
            }

            int pdesk[2];
            pipe(pdesk);
            if (fork() == 0){ 
                close(pdesk[0]);   
                close(1);          
                dup2(pdesk[1], 1); 
                execvp(argv1[0], argv1);
            } else {
                close(pdesk[1]);  
                close(0);        
                dup2(pdesk[0], 0); 
                close(1);       
                dup2(fifo_fd, 1);
                execvp(argv2[0], argv2);
            }
        }
        wait(0);

        printf("Koniec\n");
    }
}

int main(int argc, char *argv[]){

    printf("\nLiczba podanych argumentow: %d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("Argument No.%d:%s\n", i, argv[i]);
    printf("\n");

    if (argc != 2){
        printf("Podano niewlasciwa liczbe argumentow\n");
        return 1;
    }

    // ODTWORZENIE PLIKU TXT
    int queue_key=0;
    
    int fd = open("users_for_projekt.txt", O_RDONLY);
    if (fd < 1) {
        perror("Blad odtworzenia pliku txt:");
        return -1;
    }
    char buf[500];
    int i = 0;

    while (read(fd, &buf[i], 1) > 0){
        i++;
    }

    char *p = strstr(buf, argv[1]);
    if (p == NULL) {
        printf("Nie znaleziono procesu\n");
        close(fd);
        return -1;
    }

    if (buf[p - buf + strlen(argv[1])] != ' ' || (p - buf - 1 >= 0 && buf[p - buf - 1] != '\n')) {
        printf("Zla nazwa procesu\n");
        close(fd);
        return -1;
    }

    i = p - buf + strlen(argv[1]) + 3;

    while (buf[i] != '\n') {
        queue_key *= 10;
        queue_key += buf[i] - '0';
        i++;
    }

    close(fd);
    
// TWORZENIE KOLEJEK
    if (queue_key < 0){
        printf("Blad podczas wyszukiwania klucza kolejki\n");
        return 1;
    }

    int qid = msgget(queue_key, IPC_CREAT | 0666);
    if (qid < 0){
        perror("Blad podczas tworzenia kolejki");
        return 1;
    }
    printf("Kolejka jest zrobina. Numer kolejki: %d\n\n", qid);

    int pid = fork();
    if (pid == -1 ){
        perror("Blad podczas procesu fork");
        msgctl(qid, IPC_RMID, NULL);
        return 1;
    } else if (pid == 0){
        receiver(qid);
    } else {
        sender();
    }


    msgctl(qid, IPC_RMID, NULL);
    kill(pid, SIGKILL);

    return 0;
}
