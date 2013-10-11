/*
 ============================================================================
 Name        : santa.c

 Author      : xkaras27
 Version     : the last one
 Copyright   : GPL
 Description : Santa & his elves
 ============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>    //shared memory
#include <semaphore.h> 
#include <sys/mman.h>   //semaphore memory mapping
#include <sys/stat.h>
#include <errno.h>      //chybove zpravy
#include <signal.h>
#include <sys/wait.h>   //waitpid
#include <ctype.h>
#include <string.h>
#include <time.h>

int shm_id_1;
int shm_id_2;
int shm_id_3;
int *action;  // number of action
int *elves;   // number of elves
int *waiting_elves;
sem_t *mutex;  //shared memory access control
sem_t *sem_santa;
sem_t *sem_elf;
sem_t *elfmutex;
sem_t *finish;
FILE *fileout = NULL;

void print_help(void)
{
    printf("##############################################################\n"
            "santa.c --> Synchronizace procesu pro predmet IOS, 2012/2013\n"
            "autor:\txkaras27@stud.fit.vutbr.cz\n"
            "\n"
            "Parametry programu:\n"
            "\n"
            "$ ./santa C E H S nebo $ ./santa -h nebo $ ./santa --help\n"
            "C - pocet navstev skritka u Santy\n"
            "E - pocet skritku\n"
            "H - max doba vyroby jedne hracky skritkem\n"
            "S - max doba obsluhy skritka Santou\n"
            "##############################################################\n");
}

/*
 * Overeni argumentu, pri chybe vraci -1, jinak 0
 */
int check_params(char *argv[])
{
    if( ! isdigit(*argv[1]) || atoi(argv[1]) < 1)
    {
        fprintf(stderr, "Chyba: argv[1] - pocet navstev skritka u Santy\n");
        return -1;
    }
    else if( ! isdigit(*argv[2]) || atoi(argv[2]) < 1)
    {
        fprintf(stderr, "Chyba: argv[2] - pocet pracujicich skritku\n");
        return -1;
    }
    else if( ! isdigit(*argv[3]) || atoi(argv[3]) < 0)
    {
        fprintf(stderr, "Chyba: argv[3] - skritkuv cas\n");
        return -1;
    }
    else if( ! isdigit(*argv[4]) || atoi(argv[4]) < 0)
    {
        fprintf(stderr, "Chyba: argv[4] - Santuv cas\n");
        return -1;
    }
    else
        return 0;
}
/*
 * Ziskani semaforu, pri chybe vraci -1, jinak 0
 */
int get_sem(void)
{
    if((mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED)
        return -1;
    if((sem_santa = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED)
        return -1;
    if((sem_elf = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED)
        return -1;
    if((elfmutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED)
        return -1;
    if((finish = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED)
        return -1;

    if(sem_init(mutex, 1, 1) == -1)
        return -1;
    else if(sem_init(sem_santa, 1, 0) == -1)
        return -1;
    else if(sem_init(sem_elf, 1, 0) == -1)
        return -1;
    else if(sem_init(elfmutex, 1, 1) == -1)
        return -1;
    else if(sem_init(finish, 1, 0) == -1)
        return -1;
    else
        return 0;
}
/*
 * Ziskani sdilene pameti, pri chybe vraci -1, jinak 0
 * Na konci inicializuje promenne na implicitni hodnoty
 */
int get_shm(char *argv[])
{
    if( (shm_id_1 = shmget(IPC_PRIVATE, sizeof(int), SHM_R|SHM_W)) == -1)
        return -1;
    if( (shm_id_2 = shmget(IPC_PRIVATE, sizeof(int), SHM_R|SHM_W)) == -1)
        return -1;
    if( (shm_id_3 = shmget(IPC_PRIVATE, sizeof(int), SHM_R|SHM_W)) == -1)
        return -1;

    if( (action = (int *)shmat(shm_id_1, NULL, 0)) == NULL)
        return -1;
    else if( (elves = (int *)shmat(shm_id_2, NULL, 0)) == NULL)
        return -1;
    else if( (waiting_elves = (int *)shmat(shm_id_3, NULL, 0)) == NULL)
        return -1;
    else
    {
        *action = 0;
        *elves = atoi(argv[2]);
        *waiting_elves = 0;
        return 0;
    }
}
/*
 * Uvolneni zdroju, selhani se nebere v potaz.
 */
void tidy_up(int *elfove_pid)
{
    (void) shmdt(action);
    (void) shmdt(elves);
    (void) shmdt(waiting_elves);
    (void) shmctl(shm_id_1, IPC_RMID, NULL);
    (void) shmctl(shm_id_2, IPC_RMID, NULL);
    (void) shmctl(shm_id_3, IPC_RMID, NULL);
    (void) sem_destroy(mutex);
    (void) sem_destroy(sem_santa);
    (void) sem_destroy(sem_elf);
    (void) sem_destroy(elfmutex);
    (void) sem_destroy(finish);
    if(fileout != NULL)
        (void) fclose(fileout);
    if(elfove_pid != NULL)
        (void) free(elfove_pid);
}
/*
 * Vlastni proces elfa, void.
 */
void proces_elfa(char *argv[], int elf)
{
    //Elf vykonav praci - ceka
    srand(time(NULL));
    int x = (rand() % atoi(argv[3])) * 1000;
    usleep(x);

    sem_wait(mutex);
    (*action)++;
    fprintf(fileout, "%d: elf: %d: needed help\n", *action, elf);
    sem_post(mutex);

    sem_wait(elfmutex);
        sem_wait(mutex);
            (*action)++;
            fprintf(fileout, "%d: elf: %d: asked for help\n", *action, elf);
            (*waiting_elves)++;
            if(*waiting_elves == 3 && *elves > 3)
            {   // 3 elfove probudi Santu
                sem_post(sem_santa);
            }
            else if(*waiting_elves == 1 && *elves <= 3)
            {   // 1 elf probudi Santu
                sem_post(sem_santa);
            }
            else // jinak pusti dalsiho elfa
                sem_post(elfmutex);
        sem_post(mutex);

    sem_wait(sem_elf);

    sem_wait(mutex);
    (*action)++;
    fprintf(fileout, "%d: elf: %d: got help\n", *action, elf);
    (*waiting_elves)--;
    //Pokud jsem odesel jako posledni, necham pristup otevreny
    if(*waiting_elves == 0)
        sem_post(elfmutex);
    sem_post(mutex);
}
/*
 * Vlastni proces Santy, void.
 */
void proces_santy(char *argv[])
{
    int x = 0; //pouzito pro zachyceni nahodneho cisla
    while(1)  //infinite loop
    {
        sem_wait(sem_santa);
        sem_wait(mutex);
        (*action)++;
        fprintf(fileout, "%d: santa: checked state: %d: %d\n", *action, *elves, *waiting_elves);
        if(*elves == 0)
        {   //pokud jsou vsichni elfove na dovolene, take koncim
            (*action)++;
            fprintf(fileout, "%d: santa: finished\n", *action);
            sem_post(mutex);
            break;
        }
        if(*waiting_elves == 3 && *elves > 3)
        {   //obsluha 3 elfu zaraz
            (*action)++;
            fprintf(fileout, "%d: santa: can help\n", *action);
            srand(time(NULL));
            x = (rand() % atoi(argv[4])) * 1000;
            usleep(x);
            for(int i=0; i<3; i++)
            {
                sem_post(sem_elf);
            }
        }
        else if(*waiting_elves == 1 && *elves <= 3)
        {   //obsluha elfu po jednom
            (*action)++;
            fprintf(fileout, "%d: santa: can help\n", *action);
            srand(time(NULL));
            x = (rand() % atoi(argv[4])) * 1000;
            usleep(x);
            sem_post(sem_elf);
        }
        sem_post(mutex);
    }
}

/*
 * ########### MAIN ####################
 */
int main(int argc, char *argv[])
{
    // Overeni argumentu
    if(argc != 5)
    {
        if(argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help" ) == 0))
        {
            print_help();
            return 0;
        }
        else
        {
            fprintf(stderr, "Chyba: nezadali jste spravny pocet argumentu\n");
            return 1;
        }
    }
    else if(check_params(argv) == -1)
        return 1;

    // Let's start!
    int fork_error = 0; //pokud selze fork, nastavi se na 1, na konci return 2
    int counter = atoi(argv[2]);  //pocatecni pocet elfu
    int pid_santa = -1;
    int pid_elf = -1;
    int cislo_elf = 0;  //INTERNI IDENTIFIKATOR
    int vacation = atoi(argv[1]);
    int *elfove_pid = NULL;  //POLE e PROCESU
    if( (elfove_pid = malloc(atoi(argv[2]) * sizeof(int))) == NULL)
    {
        fprintf(stderr, "Chyba: nepovedla se alokace pameti\n");
        tidy_up(elfove_pid);
        return 2;
    }

    if(get_shm(argv) == -1)
    {
        fprintf(stderr, "Chyba: Ziskani sdilene pameti\n");
        tidy_up(elfove_pid);
        return 2;
    }
    if(get_sem() == -1)
    {
        fprintf(stderr, "Chyba: ziskani semaforu\n");
        tidy_up(elfove_pid);
        return 2;
    }
    if((fileout = fopen("santa.out", "w+")) == NULL)
    {
        fprintf(stderr, "Chyba: otevreni souboru\n");
        tidy_up(elfove_pid);
        return 1;
    }
    setbuf(fileout, NULL);


    // Vytvoreni procesu Santy
    pid_santa = fork();
    if(pid_santa < 0)
    {
        perror("santa - fork()");
        tidy_up(elfove_pid);
        exit(2);
    }
    else if(pid_santa == 0)
    {
        // PROCES SANTY

        sem_wait(mutex);
            (*action)++;
            fprintf(fileout, "%d: santa: started\n", *action);
        sem_post(mutex);
        proces_santy(argv);
    }
    else if(pid_santa > 0)
    {
        // HLAVNI PROCES

        //TVORENI ELFU
        for(int i=0; i < atoi(argv[2]); i++)
        {
            pid_elf = fork();
            if(pid_elf < 0)
            {
                perror("elf - fork()");
                counter = i;
                fork_error = 1;
                for(; i>=0; i--)
                {
                    if(i == 0)
                        kill(pid_santa, SIGTERM);
                    else
                        kill(elfove_pid[i-1], SIGTERM);
                }
                break;
            }
            else if(pid_elf > 0)
            {
                //HLAVNI PROCES
                elfove_pid[i] = pid_elf;
            }
            else
            {
                //PROCESY ELFU
                cislo_elf = i+1;
                break;
            }
        }
    }
    if(pid_elf == 0)
    {
        sem_wait(mutex);
            (*action)++;
            fprintf(fileout, "%d: elf: %d: started\n", *action, cislo_elf);
        sem_post(mutex);

        while(vacation > 0)
        {   //dokud elf nema pravo na dovolenou, zacne dalsi pracovni cyklus
            proces_elfa(argv, cislo_elf);
            vacation--;
        }

        sem_wait(mutex);
            (*action)++;
            (*elves)--;
            fprintf(fileout, "%d: elf: %d: got a vacation\n", *action, cislo_elf);
            if(*elves == 0)
                sem_post(sem_santa);
        sem_post(mutex);

        //zde se ceka na santu, aby se hlaska "finished" vypsala pekne konkurencne
        sem_wait(finish);
        sem_wait(mutex);
            (*action)++;
            fprintf(fileout, "%d: elf: %d: finished\n", *action, cislo_elf);
        sem_post(mutex);
        tidy_up(elfove_pid); //kazdy proces po sobe musi uklidit!!!!!!!!!
        kill(getpid(), SIGTERM);
    }
    if(pid_santa == 0)
    {
        for(int i=0; i<atoi(argv[2]); i++)
        {
            sem_post(finish);
        }
        tidy_up(elfove_pid);  //kazdy proces po sobe musi uklidit!!!!!!!!!
        kill(getpid(), SIGTERM);
    }
    else
    {
        for(int i=0; i < counter; i++)
        {
            (void) waitpid(elfove_pid[i], NULL, 0);
        }
        (void) waitpid(pid_santa, NULL, 0);
    }
    tidy_up(elfove_pid);
    if(fork_error == 1)
        return 2;
    return 0;
}
