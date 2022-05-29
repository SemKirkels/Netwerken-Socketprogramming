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

int initialization();
int connection(int internet_socket);
void execution(int internet_socket);
void cleanup(int internet_socket, int client_internet_socket);

int main(int argc, char *argv[])
{
    //////////////////
    //Initialization//
    //////////////////

    OSInit();

    int internet_socket = initialization();

    //////////////
    //Connection//
    //////////////

    int client_internet_socket = connection(internet_socket);

    /////////////
    //Execution//
    /////////////

    execution(client_internet_socket);

    ///////////
    //Cleanup//
    ///////////

    cleanup(internet_socket, client_internet_socket);

    OSCleanup();

    return 0;
}

int initialization()
{
    //Step 1.1
    struct addrinfo internet_address_setup; //Stack variable
    struct addrinfo *internet_address_result; //Stack variable
    memset(&internet_address_setup, 0, sizeof(internet_address_setup)); //initialiseert de struct op 0
    internet_address_setup.ai_family = AF_UNSPEC; //ai_family -> ipv4 of ipv6 --> geen waarde meegegeven
    internet_address_setup.ai_socktype = SOCK_STREAM; // -> socket type -> (UDP -> DGRAM) / (TCP -> STRAM)
    internet_address_setup.ai_flags = AI_PASSIVE; //AI_PASSIVE -> Server
    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result); //NULL -> de server kan van elk ip adres worden benaderd -> NULL is server
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
            int bind_return = bind(internet_socket, internet_address_result_iterator -> ai_addr, internet_address_result_iterator -> ai_addrlen);
            if(bind_return == -1)
            {
                perror("Bind");
                close(internet_socket);
            }
            else
            {
                //Step 1.4
                int listen_return = listen(internet_socket, 1);
                if(listen_return == -1)
                {
                    perror("Listen");
                    close(internet_socket);
                }
                else
                {
                    break;
                }
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

int connection(int internet_socket)
{
    //Step 2.1
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof(client_internet_address);
    int client_socket = accept(internet_socket, (struct sockaddr *) &client_internet_address, &client_internet_address_length);
    if(client_socket == -1)
    {
        perror("Accept");
        close(internet_socket);
        exit(3);
    }
    return client_socket;
}

void execution(int internet_socket)
{
    //Step 3.1
    int number_of_bytes_received = 0;
    char buffer[1000];
    number_of_bytes_received = recv(internet_socket, buffer, (sizeof(buffer)) - 1, 0);
    if(number_of_bytes_received == -1)
    {
        perror("Recv");
    }
    else //De else is nodig omdat er anders op de plaats van de fout -1 een \0 wordt gezet
    {
        // Ontvangt data via internet socket met een maximale grootte van buffer - 1 -> 999
        buffer[number_of_bytes_received] = '\0'; // -> laatste teken in de buffer is \0
        printf("Received: %s\n", buffer);
    }  

    //Step 3.2
    int number_of_bytes_send = 0;
    number_of_bytes_send = send(internet_socket, "Hello TCP world!", 16, 0);
    //sendto(internet_socket, Te sturen data, lengte van de data, flags
    if(number_of_bytes_send == -1)
    {
        perror("Send");
    }

    //Bij de server zijn Step 2.1 en 2.2 omgewisseld -> eerst recv dan send (client -> send dan recv)
}

void cleanup(int internet_socket, int client_internet_socket)
{
    //Step 4.2
    int shutdown_return = shutdown(client_internet_socket, SD_RECEIVE);
    if(shutdown_return == -1)
    {
        perror("Shutdown");
    }

    //Step 4.1
    close(internet_socket);
    close(client_internet_socket);
}