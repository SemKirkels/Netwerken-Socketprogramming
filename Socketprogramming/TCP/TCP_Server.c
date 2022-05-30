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

#define PORT "24045" 

int initHttpRequest(); //initialize HTTP request
void exHttpRequest(); //executes HTTP request
void cleanupHttpRequest();

int main(int argc, char *argv[])
{

    ////////////////
    //HTTP Request//
    ////////////////

    OSInit();
    int internet_socket = initHttpRequest();
    exHttpRequest(internet_socket);
    cleanupHttpRequest(internet_socket);
    char messageHistory[16 * 255]; //Grootte van de laatste 16 berichten -> 16 * 255

    fd_set master;
    fd_set read_fds;
    int fdmax;

    int listener;
    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;

    char buffer[255];   //Groote van een bericht max 255 karakters
    int lengthOfBuffer = 0;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1;
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
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
        }

        for(i = 0; i <= fdmax; i++)
        {
            if(FD_ISSET(i, &read_fds))
            {
                if(i == listener)
                {
                    addrlen = sizeof(remoteaddr);
                    newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

                    if(newfd == -1)
                    {
                        
                    }
                }
            }
        }
    }

    ///////////
    //Cleanup//
    ///////////

    OSCleanup();

    return 0;
}

//////////////////////
//Start HTTP Request//
//////////////////////

int initHttpRequest()
{
    //Step 1.1
    struct addrinfo internet_address_setup; //Stack variable
    struct addrinfo *internet_address_result; //Stack variable
    memset(&internet_address_setup, 0, sizeof(internet_address_setup)); //initialiseert de struct op 0
    internet_address_setup.ai_family = AF_INET; //ai_family -> ipv4 of ipv6 --> geen waarde meegegeven
    internet_address_setup.ai_socktype = SOCK_STREAM; // -> socket type -> (UDP -> DGRAM) / (TCP -> STREAM)
    int getaddrinfo_return = getaddrinfo("student.pxl-ea-ict.be", "80", &internet_address_setup, &internet_address_result);
    //Als getaddrinfo niet gelijk is aan 0 is er een fout in de functie getaddrinfo (vb. IP adres, poort of foute pointer).
    if(getaddrinfo_return != 0) //Geeft een foutmelding als er iets mis is met "getaddrinfo"
    {
        fprintf(stderr, "Getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1); //Exit code 1 is nu gereserveerd voor een fout in de getaddrinfo
    }
    
    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while(internet_address_result_iterator != NULL)
    {
        //Step 1.2
        internet_socket = socket(internet_address_result_iterator -> ai_family, internet_address_result_iterator -> ai_socktype, internet_address_result_iterator -> ai_protocol);
        if(internet_socket == -1) //Geeft een foutmelding als er iets mis is met "internet_socket"
        {
            perror("Socket");
        }
        else
        {
            //Step 1.3 
            int bind_return = connect(internet_socket, internet_address_result_iterator -> ai_addr, internet_address_result_iterator -> ai_addrlen);
            if(bind_return == -1)
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

void exHttpRequest(int internet_socket)
{
    char buffer[1000];

    int number_of_bytes_send = send(internet_socket, "Get /history.php?i=12102824 HTTP/1.0\r\nHost: student.pxl-ea-ict.be\r\n\r\n", strlen("Get /history.php?i=12102824 HTTP/1.0\r\nHost: student.pxl-ea-ict.be\r\n\r\n"), 0);
    if(number_of_bytes_send == -1)
    {
        perror("Send");
    }

    int number_of_bytes_received = recv(internet_socket, buffer, sizeof(buffer) - 1, 0);
    if(number_of_bytes_received == -1)
    {
        perror("Recv");
    }
    else
    {
        buffer[number_of_bytes_received] = '\0';
        printf("%s\n", buffer);
    }
}

void cleanupHttpRequest();
{
    if(shutdown(internet_socket, SD_SEND) == -1)
    {
        perror("Shutdown HTTP Request");
    }
    close(internet_socket);
}

//////////////////////
//Einde HTTP Request//
//////////////////////
