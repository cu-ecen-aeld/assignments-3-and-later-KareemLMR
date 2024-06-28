#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

pthread_mutex_t readMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER;

#if USE_AESD_CHAR_DEVICE
const char* outputFile = "/dev/aesdchar";
#else
const char* outputFile = "/var/tmp/aesdsocketdata";
#endif

int totalBuffSize = 0;
bool sigint = false;
bool sigterm = false;

struct aesd_seekto {
    /**
     * The zero referenced write command to seek into
     */
    uint32_t write_cmd;
    /**
     * The zero referenced offset within the write
     */
    uint32_t write_cmd_offset;
};

#define AESD_IOC_MAGIC 0x16

// Define a write command from the user point of view, use command number 1
#define AESDCHAR_IOCSEEKTO _IOWR(AESD_IOC_MAGIC, 1, struct aesd_seekto)
/**
 * The maximum number of commands supported, used for bounds checking
 */
#define AESDCHAR_IOC_MAXNR 1


typedef struct handler
{
    pthread_t thread;
    int connfd;
    bool done;
    SLIST_ENTRY(handler) entries;

}handler_t;

void sigint_handler(int sig)
{
    //printf("SIGINT received. Exiting...\n");
    sigint = true;
}
void sigterm_handler(int sig)
{
    //printf("SIGTERM received. Exiting...\n");
    sigterm = true;
}

void* updateTime(void*)
{
    while (!sigint && !sigterm)
    {
        sleep(10);
        int failedToLock = pthread_mutex_lock(&writeMutex);

        struct timespec current_time;
        struct tm *local_time;
        char outstr[200];
        char timestamp[200] = "timestamp:";
        // Get the current real-time clock
        if (clock_gettime(CLOCK_REALTIME, &current_time) == -1)
        {
            perror("clock_gettime");
            exit(EXIT_FAILURE);
        }
        local_time = localtime(&current_time.tv_sec);
        if (local_time == NULL)
        {
            perror("localtime");
            exit(EXIT_FAILURE);
        }
        if (strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", local_time) == 0)
        {
            fprintf(stderr, "strftime returned 0");
            exit(EXIT_FAILURE);
        }
        strcat(outstr, "\n");
        strcat(timestamp, outstr);
        totalBuffSize += strlen(timestamp) + 1;

        int fd = open(outputFile, O_RDWR|O_APPEND|O_CREAT, 0644);
        if (fd == -1)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // Position the file pointer to the end of the file
        if (lseek(fd, 0, SEEK_END) == -1)
        {
            perror("lseek");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Write the received data to the file
        if (write(fd, timestamp, strlen(timestamp) + 1) == -1)
        {
            perror("write");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Sync the file to disk
        if (fsync(fd) == -1)
        {
            perror("fsync");
            close(fd);
            exit(EXIT_FAILURE);
        }

        int failedToUnlock = pthread_mutex_unlock(&writeMutex);
    }
    return NULL;
}
void* handleClient(void* connData)
{
    handler_t* threadConnData = (handler_t *) connData;
    //printf("Handling client\n");
    char* recvBuff = NULL;
    recvBuff = (char*)malloc(sizeof(char));
    if (recvBuff == NULL)
    {
        //printf("unable to allocate memory\n");
        exit(0);
    }
    //printf("recvBuff allocated\n");
    int n = recv(threadConnData->connfd, recvBuff, 1, 0);
    //printf("recv called\n");
    if (n == -1)
    {
        perror("recv");

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            ////printf("Socket would block\n");
        }
        else if (errno == ECONNRESET)
        {
            ////printf("Connection reset by peer\n");
        } 
        else 
        {
            ////printf("Error: %s\n", strerror(errno));
        }
        close(threadConnData->connfd);
        exit(0);
    }
    int incremental = 0;
    //printf("Received %d Byte(s) of value %c on %d \n", n, recvBuff[incremental], *((int*)connfd));
    while (recvBuff[incremental] != '\n')
    {
        recvBuff = (char*)realloc(recvBuff, (++incremental + 1) * sizeof(char));
        n = recv(threadConnData->connfd, (recvBuff + incremental), 1, 0);
        //printf("Received %d Byte(s) of value %c on %d \n", n, recvBuff[incremental], *((int*)connfd));
        if (n == -1)
        {
            perror("recv");

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                //printf("Socket would block\n");
            }
            else if (errno == ECONNRESET)
            {
                //printf("Connection reset by peer\n");
            } 
            else 
            {
                //printf("Error: %s\n", strerror(errno));
            }
            close(threadConnData->connfd);
            exit(0);
        }
        else if (n == 0)
        {
            close(threadConnData->connfd);
            //printf("Client closed connection\n");
            break;
        }
    }
    //printf("Loop exited\n");
    //int failedToUnlock = pthread_mutex_unlock(&readMutex);
    int fd = open(outputFile, O_RDWR|O_APPEND|O_CREAT, 0644);
    if (fd == -1)
    {
        close(threadConnData->connfd);
        perror("open");
        exit(EXIT_FAILURE);
    }
    char cmd[25];
    strncpy(cmd, recvBuff, 19);
    cmd[19] = '\0';
    printf("cmd = %s, recvBuff = %s\n", cmd, recvBuff);
    if (!strcmp(cmd, "AESDCHAR_IOCSEEKTO:"))
    {
        char write_cmd[3];
        char write_cmd_offset[3];
        strncpy(write_cmd, recvBuff + 19, 2);
        if (write_cmd[1] == ',')
        {
            write_cmd[1] = '\0';
            strncpy(write_cmd_offset, recvBuff + 21, 2);
            //write_cmd_offset[strlen(cmd) - 23] = '\0';
        }
        else
        {
            write_cmd[2] = '\0';
            strncpy(write_cmd_offset, recvBuff + 22, 2);
            //write_cmd_offset[strlen(cmd) - 24] = '\0';
        }
        printf("write_cmd = %s, write_cmd_offset = %s\n", write_cmd, write_cmd_offset);
        int x, y;
        x = atoi(write_cmd);
        y = atoi(write_cmd_offset);
        printf("x = %d, y = %d", x, y);
        struct aesd_seekto seek_params;
        seek_params.write_cmd = x;
        seek_params.write_cmd_offset = y;
        printf("seek_params.write_cmd = %d, seek_params.write_cmd_offset = %d\n", seek_params.write_cmd, seek_params.write_cmd_offset);
        int failedToLock = pthread_mutex_lock(&writeMutex);
        long int ret = ioctl(fd, AESDCHAR_IOCSEEKTO, &seek_params);

        char* totalBuff;
        totalBuff = (char*)malloc(totalBuffSize * sizeof(char));

        int bytesRead = read(fd, totalBuff, totalBuffSize);
        ////printf("%d bytes read\n", bytesRead);

        int sent = send(threadConnData->connfd, totalBuff, totalBuffSize, 0);
        ////printf("%d bytes sent\n", sent);

        // Close the file
        if (close(fd) == -1)
        {
            close(threadConnData->connfd);
            perror("close");
            exit(EXIT_FAILURE);
        }
        free(totalBuff);

        int failedToUnlock = pthread_mutex_unlock(&writeMutex);
    }
    else
    {
        int failedToLock = pthread_mutex_lock(&writeMutex);

        totalBuffSize += incremental + 1;

        // Position the file pointer to the end of the file
        // if (lseek(fd, 0, SEEK_END) == -1)
        // {
        //     close(threadConnData->connfd);
        //     perror("lseek");
        //     close(fd);
        //     exit(EXIT_FAILURE);
        // }

        // Write the received data to the file
        if (write(fd, recvBuff, incremental + 1) == -1)
        {
            close(threadConnData->connfd);
            perror("write");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Sync the file to disk
        // if (fsync(fd) == -1)
        // {
        //     close(threadConnData->connfd);
        //     perror("fsync");
        //     close(fd);
        //     exit(EXIT_FAILURE);
        // }

        // Set the file offset to the beginning of the file
        if (lseek(fd, 0, SEEK_SET) == -1)
        {
            close(threadConnData->connfd);
            perror("Error seeking to the beginning of file");
            exit(EXIT_FAILURE);
        }

        char* totalBuff;
        totalBuff = (char*)malloc(totalBuffSize * sizeof(char));

        int bytesRead = read(fd, totalBuff, totalBuffSize);
        ////printf("%d bytes read\n", bytesRead);

        int sent = send(threadConnData->connfd, totalBuff, totalBuffSize, 0);
        ////printf("%d bytes sent\n", sent);

        // Close the file
        if (close(fd) == -1)
        {
            close(threadConnData->connfd);
            perror("close");
            exit(EXIT_FAILURE);
        }
        free(totalBuff);
        int failedToUnlock = pthread_mutex_unlock(&writeMutex);
    }
    
    free(recvBuff);
    close(threadConnData->connfd);
    threadConnData->done = true;

    return NULL;
}
int main(int argc, char** argv)
{
    int reuseaddr = 1;
    int listenfd = 0;
    struct sockaddr_in serv_addr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(9000);

    // Set SO_REUSEADDR option
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid;
    if (argc > 1)
    {
        if (!strcmp(argv[1], "-d"))
        {
            ////printf("Running in daemon mode...");
            pid = fork();
            if (pid > 0)
            {
                exit(EXIT_SUCCESS);
            }
        }
    }

    struct sigaction sa_int, sa_term;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    sa_term.sa_handler = sigterm_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;

    // Register the signal handlers for SIGINT and SIGTERM
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa_term, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    SLIST_HEAD(headWorker, handler) workers = SLIST_HEAD_INITIALIZER(workers);

    bool timerStarted = false;
    while(!sigint && !sigterm)
    {
        listen(listenfd, 10);
        ////printf("Received a connection request \n");
        handler_t* newWorker;
        newWorker = (handler_t*)malloc(sizeof(handler_t));
        newWorker->done = false;
        newWorker->connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        SLIST_INSERT_HEAD(&workers, newWorker, entries);

        ////printf("Accepted a connection request on %d, forking a thread... \n", connfd);
        //pthread_t thread;

        #if !(USE_AESD_CHAR_DEVICE)
        if (!timerStarted)
        {
            pthread_t timer;
            int failedToCreateThread = pthread_create(&timer, NULL, updateTime, (void*) NULL);
            timerStarted = true;
        }
        #endif

        int failedToCreateThread = pthread_create(&(newWorker->thread), NULL, handleClient, (void*) newWorker);
        if (failedToCreateThread)
        {
            ////printf("Failed to create a thread... \n");
        }
        handler_t* currentWorker;
        SLIST_FOREACH(currentWorker, &workers, entries)
        {
            if (currentWorker->done)
            {
                //printf("Removing handler\n");
                void *thread_result;
                pthread_join(currentWorker->thread, &thread_result);
                SLIST_REMOVE(&workers, currentWorker, handler, entries);
            }
            //printf("%d ", current_node->data);
        }
    }
    #if !(USE_AESD_CHAR_DEVICE)
    remove(outputFile);
    #endif
    close(listenfd);
    return 0;
}