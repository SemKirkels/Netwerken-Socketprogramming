#ifdef _WIN32

    #define _WIN32_WINNT _WIN32_WINNT_WIN7 //Select minimal legacy support, needed for inet_pton, inet_ntop
    #include <winsock2.h> //for all socket programming
    #include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
    #include <stdio.h> //for fprintf
    #include <unistd.h> //for close
    #include <stdlib.h> //for exit
    #include <string.h> //for memset
    #include <pthread.h>

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

int initialization();
void runThread();
void *sendThread();
void *recvThread();
void cleanup(int internet_socket);

int internet_socket;

int main(int argc, char *argv[])
{
    //////////////////
    //Initialization//
    //////////////////

    OSInit();

    internet_socket = initialization();

    /////////////
    //Execution//
    /////////////

    pthread_t threadSend, threadRecv;
    /*
    *Maakt een thread voor Send en Recv
    */
    if(pthread_create(&threadSend, NULL, &sendThread, NULL) != 0)
    {
        perror("Error create threadSend");
    }
    if(pthread_create(&threadRecv, NULL, &recvThread, NULL) != 0)
    {
        perror("Error create threadRecv");
    }

    if(pthread_join(threadSend, NULL) != 0)
    {
        perror("Error join threadRecv");
    }
    if(pthread_join(threadRecv, NULL) != 0)
    {
        perror("Error join threadRecv");
    }

    ///////////
    //Cleanup//
    ///////////

    cleanup(internet_socket);

    OSCleanup();

    return 0;
}

int initialization()
{
    //Step 1.1
    struct addrinfo internet_address_setup; //Stack variable
    struct addrinfo *internet_address_result; //Stack variable
    memset(&internet_address_setup, 0, sizeof(internet_address_setup)); //initialiseert de struct op 0
    internet_address_setup.ai_family = AF_INET; //ai_family -> ipv4 of ipv6 --> geen waarde meegegeven
    internet_address_setup.ai_socktype = SOCK_STREAM; // -> socket type -> (UDP -> DGRAM) / (TCP -> STRAM)
    int getaddrinfo_return = getaddrinfo("127.0.0.1", "24042", &internet_address_setup, &internet_address_result);
    //Als getaddrinfo niet gelijk is aan 0 is er een fout in de functie getaddrinfo (vb. IP adres, poort of foute pointer).
    if(getaddrinfo_return != 0) //Geeft een foutmelding als er iets mis is met "getaddrinfo"
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1); //Exit code 1 is nu gereserveerd voor een fout in de getaddrinfo
    }

    //Step 1.2
    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while(internet_address_result_iterator != NULL)
    {
        internet_socket = socket(internet_address_result_iterator -> ai_family, internet_address_result_iterator -> ai_socktype, internet_address_result_iterator -> ai_protocol);
        if(internet_socket == -1) //Geeft een foutmelding als er iets mis is met "internet_socket"
        {
            perror("socket");
        }
        else
        {
            int connect_return = connect(internet_socket, internet_address_result_iterator -> ai_addr, internet_address_result_iterator -> ai_addrlen);
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
        internet_address_result_iterator = internet_address_result_iterator -> ai_next;
    }
    
    freeaddrinfo(internet_address_result);

    if(internet_socket == -1) //Geeft een foutmelding als er iets mis is met "internet_socket"
    {
        fprintf(stderr, "Socket: no valid socket address found\n");
        exit(2); //Exit code 2 is nu gereserveerd voor een fout in socket
    }

    return internet_socket;
}

void *sendThread()
{
    while(1)
    {
        int packetLength = 0;
        char packetData[255];

        gets(packetData);

        packetLength = strlen(packetData);
        packetData[packetLength] = '\n';

        int number_of_bytes_send = send(internet_socket, packetData, packetLength, 0);
        if(number_of_bytes_send == -1)
        {
            perror("Send");
        }
    }
}

void *recvThread()
{
    while(1)
    {
        char buffer[1000];
        int number_of_bytes_recv = recv(internet_socket, buffer, sizeof(buffer) -1, 0);

        if(number_of_bytes_recv == -1)
        {
            perror("Recv");
        }
        else
        {
            buffer[number_of_bytes_recv] = '\0';
            printf("Received: %s\n", buffer);
        }
    }
}

void cleanup(int internet_socket)
{
    //Step3.2
    int shutdown_return = shutdown(internet_socket, SD_SEND);
    if(shutdown_return == -1)
    {
        perror("Shutdown");
    }

    //Step 3.1
    close(internet_socket);
}

