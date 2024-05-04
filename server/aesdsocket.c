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
#define BUFFER_SIZE 1024
pthread_mutex_t readMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER;
const char* outputFile = "/var/tmp/aesdsocketdata";
void* handleClient(void* connfd)
{
    printf("handling client.. \n");
    //int failedToLock = pthread_mutex_lock(&readMutex);
    // if (failedToLock)
    // {
    //     printf("Failed to lock mutex \n");
    // }
    // else
    // {
    //     printf("mutex locked successfully\n");
    // }
    char* recvBuff;
    recvBuff = (char*)malloc(sizeof(char));
    int n = recv(*((int*)connfd), recvBuff, 1, 0);
    if (n == -1)
    {
        perror("recv");

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            printf("Socket would block\n");
        }
        else if (errno == ECONNRESET)
        {
            printf("Connection reset by peer\n");
        } 
        else 
        {
            printf("Error: %s\n", strerror(errno));
        }
        close(*((int*)connfd));
        exit(0);
    }
    int incremental = 0;
    printf("Received %d Byte(s) of value %c on %d \n", n, recvBuff[incremental], *((int*)connfd));
    while (recvBuff[incremental] != '\n')
    {
        recvBuff = (char*)realloc(recvBuff, (++incremental) * sizeof(char));
        n = recv(*((int*)connfd), (recvBuff + incremental), 1, 0);
        printf("Received %d Byte(s) of value %c on %d \n", n, recvBuff[incremental], *((int*)connfd));
        if (n == -1)
        {
            perror("recv");

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("Socket would block\n");
            }
            else if (errno == ECONNRESET)
            {
                printf("Connection reset by peer\n");
            } 
            else 
            {
                printf("Error: %s\n", strerror(errno));
            }
            close(*((int*)connfd));
            exit(0);
        }
        else if (n == 0)
        {
            close(*((int*)connfd));
            printf("Client closed connection\n");
            break;
        }
    }

    //int failedToUnlock = pthread_mutex_unlock(&readMutex);

    int failedToLock = pthread_mutex_lock(&writeMutex);

    int fd = open(outputFile, O_WRONLY|O_APPEND|O_CREAT, 0644);
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

    // Close the file
    if (close(fd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
    // recvBuff[incremental] = '\0';
    // char cmd[BUFFER_SIZE] = "echo ";
    // strcat(cmd, recvBuff);
    // strcat(cmd, " >> ");
    // strcat(cmd, outputFile);
    // system(cmd);
    // printf("%s\n", cmd);
    close(*((int*)connfd));
    int failedToUnlock = pthread_mutex_unlock(&writeMutex);
    //exit(0);
    return NULL;
}
int main(int argc, char** argv)
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(9000); 

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

    while(1)
    {
        listen(listenfd, 10);
        printf("Received a connection request \n");
        int connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        printf("Accepted a connection request on %d, forking a thread... \n", connfd);
        pthread_t thread;
        int failedToCreateThread = pthread_create(&thread, NULL, handleClient, (void*) &connfd);
        if (failedToCreateThread)
        {
            printf("Failed to create a thread... \n");
        }
     }
    return 0;
}
