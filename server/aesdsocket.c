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
#define MAX_SESSIONS 1000
pthread_mutex_t readMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER;
const char* outputFile = "/var/tmp/aesdsocketdata";
int totalBuffSize = 0;
bool sigint = false;
bool sigterm = false;
void sigint_handler(int sig)
{
    printf("SIGINT received. Exiting...\n");
    sigint = true;
}
void sigterm_handler(int sig)
{
    printf("SIGTERM received. Exiting...\n");
    sigterm = true;
}
void* handleClient(void* connfd)
{
    //printf("handling client.. \n");
    //int failedToLock = pthread_mutex_lock(&readMutex);
    // if (failedToLock)
    // {
    //     //printf("Failed to lock mutex \n");
    // }
    // else
    // {
    //     //printf("mutex locked successfully\n");
    // }
    char* recvBuff;
    recvBuff = (char*)malloc(sizeof(char));
    int n = recv(*((int*)connfd), recvBuff, 1, 0);
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
        close(*((int*)connfd));
        exit(0);
    }
    int incremental = 0;
    //printf("Received %d Byte(s) of value %c on %d \n", n, recvBuff[incremental], *((int*)connfd));
    while (recvBuff[incremental] != '\n')
    {
        recvBuff = (char*)realloc(recvBuff, (++incremental) * sizeof(char));
        n = recv(*((int*)connfd), (recvBuff + incremental), 1, 0);
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
            close(*((int*)connfd));
            exit(0);
        }
        else if (n == 0)
        {
            close(*((int*)connfd));
            //printf("Client closed connection\n");
            break;
        }
    }

    //int failedToUnlock = pthread_mutex_unlock(&readMutex);

    int failedToLock = pthread_mutex_lock(&writeMutex);

    totalBuffSize += incremental + 1;

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
    if (write(fd, recvBuff, incremental + 1) == -1)
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

    // Set the file offset to the beginning of the file
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        perror("Error seeking to the beginning of file");
        exit(EXIT_FAILURE);
    }

    char* totalBuff;
    totalBuff = (char*)malloc(totalBuffSize * sizeof(char));

    int bytesRead = read(fd, totalBuff, totalBuffSize);
    //printf("%d bytes read\n", bytesRead);

    int sent = send(*((int*)connfd), totalBuff, totalBuffSize, 0);
    //printf("%d bytes sent\n", sent);

    // Close the file
    if (close(fd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
    //free(recvBuff);
    //free(totalBuff);
    close(*((int*)connfd));
    int failedToUnlock = pthread_mutex_unlock(&writeMutex);
    //exit(0);
    return NULL;
}
int main(int argc, char** argv)
{
    int reuseaddr = 1;
    int listenfd = 0;
    int connfd[MAX_SESSIONS];
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
            //printf("Running in daemon mode...");
            pid = fork();
            if (pid > 0)
            {
                exit(EXIT_SUCCESS);
            }
            //Create a new session and detach from the terminal
            // if (setsid() < 0)
            // {
            //     // Setsid failed
            //     fprintf(stderr, "Setsid failed\n");
            //     exit(EXIT_FAILURE);
            // }

            //Fork again to ensure the process is not a session leader
            // pid = fork();

            // if (pid < 0)
            // {
            //     // Fork failed
            //     fprintf(stderr, "Second fork failed\n");
            //     exit(EXIT_FAILURE);
            // }

            // // If we got a good PID, then we can exit the parent process
            // if (pid > 0) {
            //     // Exit the parent process
            //     exit(EXIT_SUCCESS);
            // }

            // // Change the current working directory to root
            // if (chdir("/") < 0)
            // {
            //     // chdir failed
            //     fprintf(stderr, "chdir failed\n");
            //     exit(EXIT_FAILURE);
            // }

            // // Close all open file descriptors
            // for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
            //     close(fd);
            // }
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
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa_term, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    int activeConnections = 0;

    while(!sigint && !sigterm)
    {
        listen(listenfd, 10);
        //printf("Received a connection request \n");
        connfd[activeConnections] = accept(listenfd, (struct sockaddr*)NULL, NULL);

        //printf("Accepted a connection request on %d, forking a thread... \n", connfd);
        pthread_t thread;
        int failedToCreateThread = pthread_create(&thread, NULL, handleClient, (void*) &connfd[activeConnections]);
        if (failedToCreateThread)
        {
            //printf("Failed to create a thread... \n");
        }
        if(activeConnections == MAX_SESSIONS - 1)
        {
            activeConnections = 0;
        }
        else
        {
            activeConnections++;
        }
    }
    close(listenfd);
    return 0;
}
