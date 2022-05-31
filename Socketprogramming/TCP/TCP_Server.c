#ifdef _WIN32

    #define _WIN32_WINNT _WIN32_WINNT_WIN7 //Select minimal legacy support, needed for inet_pton, inet_ntop
    #include <winsock2.h> //for all socket programming
    #include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
    #include <stdio.h> //for fprintf
    #include <unistd.h> //for close
    #include <stdlib.h> //for exit
    #include <string.h> //for memset

    void OSInit(void)
    {
        //Step 1.0
        WSADATA wsaData;
        int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
        if(WSAError != 0)
        {
            fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
            exit(-1);
        }
    }

    void OSCleanup(void)
    {
        //Step 3.0
        WSACleanup();
    }
    #define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())

#else

    #include <sys/socket.h> //for sockaddr, socket, socket
    #include <sys/types.h> //for size_t
    #include <netdb.h> //for getaddrinfo
    #include <netinet/in.h> //for sockaddr_in
    #include <arpa/inet.h> //for htons, htonl, inet_pton, inet_ntop
    #include <errno.h> //for errno
    #include <stdio.h> //for fprintf, perror
    #include <unistd.h> //for close
    #include <stdlib.h> //for exit
    #include <string.h> //for memset

    void OSInit(void) {}
    void OSCleanup(void) {}

#endif

#define PORT "24042" 

char bufferMessage[255];
char historyMessages[16 * 255]; //16 * 255 omdat er 16 berichten van maximaal 255 bytes worden gehaald van de server
int lengthOfBuffer;

void *get_in_addr(struct sockaddr *sa);

int init();
void messageHistory();
void execution(int internet_socket);
void exMessage();
void cleanup(int internet_socket);

int main(int argc, char *argv[])
{
    OSInit();
    messageHistory();

    /*
    Onderstaande code komt uit een Youtube video
    */
    fd_set master;
    fd_set read_fds;
    int fdmax;
    int listener;
    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    char remoteIP[INET6_ADDRSTRLEN];
    int yes = 1;
    int i, j, rv;
    struct addrinfo hints, *ai, *p;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "Selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(listener < 0)
        {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if(bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }
        break;
    }

    if(p == NULL)
    {
        fprintf(stderr, "Selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai);

    puts("Binding sucessful!");

    if(listen(listener, 10) == -1)
    {
        perror("Listen");
        exit(3);
    }

    FD_SET(listener, &master);

    fdmax = listener;

    while(1)
    {
        read_fds = master;
        if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Select");
            exit(4);
        }

        for(i = 0; i <= fdmax; i++)
        {
            if(FD_ISSET(i, &read_fds))
            {
                if(i == listener)
                {
                    addrlen = sizeof(remoteaddr);
                    newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);
                    send(newfd, historyMessages, strlen(historyMessages + 1), 0); //Verstuurt de laatste 16 berichten naar de niewe gebruiker

                    if(newfd == -1)
                    {
                        perror("Accept");
                    }
                    else
                    {
                        FD_SET(newfd, &master);
                        if(newfd > fdmax)
                        {
                            fdmax = newfd;
                        }
                        printf("Selectserver: Nieuwe connectie van IP: [ %s ] op" "socket [ %d] \n", inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN), newfd);
                        char messageNewUser[256];
                        sprintf(messageNewUser, "Nieuwe verbinding ");
                        strcat(messageNewUser, inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*) &remoteaddr), remoteIP, INET6_ADDRSTRLEN));
                        strcat(messageNewUser, "op socket ");

                        char newfdString[4];
                        itoa(newfd, newfdString, 10);
                        strcat(messageNewUser, newfdString);
                        messageNewUser[strlen(messageNewUser)] = '\r';

                        for(j = 0; j <= fdmax; j++)
                        {
                            if(FD_ISSET(j, &master))
                            {
                                if(j != listener && j != i)
                                {
                                    if(send(j, messageNewUser, lengthOfBuffer, 0) == -1)
                                    {
                                        perror("Send");
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    lengthOfBuffer = recv(i, bufferMessage, sizeof(bufferMessage), 0);
                    if(lengthOfBuffer <= 0)
                    {
                        if(lengthOfBuffer == 0)
                        {
                            printf("Selectserver: F in chatbox socket [ %d ] heeft de chat verlaten\n", i);
                        }
                        else
                        {
                            perror("Recv");
                        }
                        close(i);
                        FD_CLR(i, &master);
                    }
                    else
                    {
                        for(j = 0; j <= fdmax; j++)
                        {
                            if(FD_ISSET(j, &master))
                            {
                                if(j != listener && j != i)
                                {
                                    if(send(j, bufferMessage, lengthOfBuffer, 0) == -1)
                                    {
                                        perror("Send");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /*
    Einde code Youtube video
    */

    ///////////
    //Cleanup//
    ///////////

    OSCleanup();

    return 0;
}

/////////////////////////////////////
//Start functies code Youtube video//
/////////////////////////////////////
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
////////////////////////////
//Einde code Youtube video//
////////////////////////////

///////////////////
//Code uit de les//
///////////////////
int init()
{
    struct addrinfo internet_address_setup;
    struct addrinfo *internet_address_result;
    memset(&internet_address_setup, 0 , sizeof(internet_address_setup));
    internet_address_setup.ai_family = AF_INET;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    int getaddrinfo_return = getaddrinfo("student.pxl-ea-ict.be", "80", &internet_address_setup, &internet_address_result);
    if(getaddrinfo_return != 0)
    {
        fprintf(stderr, "Getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1);
    }
    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while(internet_address_result_iterator != NULL)
    {
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);
        if(internet_socket == -1)
        {
            perror("Socket");
        }
        else
        {
            int connect_return = connect(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen);
            if(connect_return == -1)
            {
                perror("Connect");
                close(internet_socket);
            }
            else
            {
                break;
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }
    freeaddrinfo(internet_address_result);

    if(internet_socket == -1)
    {
        fprintf(stderr, "Socket, no valid socket address found\n");
        exit(2);
    }
    return internet_socket;
}

void execution(int internet_socket)
{
    int strlength = strlen("GET /history.php?i=12102824 HTTP/1.0\r\nHost: student.pxl-ea-ict.be\r\n\r\n");
    printf("Test strlen: %d\n", strlength);
    int number_of_bytes_send = send(internet_socket, "GET /history.php?i=12102824 HTTP/1.0\r\nHost: student.pxl-ea-ict.be\r\n\r\n", strlength + 1, 0);
    if(number_of_bytes_send == -1)
    {
        perror("Send");
    }

    char buffer[1000];
    int recvBytes = recv(internet_socket, buffer, sizeof (buffer) - 1, 0);
    if(recvBytes == -1)
    {
        perror("Recv");
    }
    else
    {
        buffer[recvBytes] = '\0';
        printf("Received1:\n%s\n", buffer);
        sprintf(historyMessages, "\n%s \n\n", buffer);
    }
}

void cleanup(int internet_socket)
{
    if(shutdown(internet_socket, SD_SEND) == -1)
    {
        perror("Shutdown");
    }
    
    close(internet_socket);
}
/////////////////////////
//Einde code uit de les//
/////////////////////////

void messageHistory()
{
    int internet_socket = init();
    execution(internet_socket);
    cleanup(internet_socket);
}

void exMessage()
{
    bufferMessage[lengthOfBuffer] = '\0';
    char message[255];

    for(int i = 0, j; bufferMessage[i] != '\0'; ++i)
    {

        for(j = i; bufferMessage[j] != '\0'; ++j)
        {
            bufferMessage[j] = message[j + 1];
        }
        message[j] = '\0';
    }
}