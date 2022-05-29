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

int initialization(struct sockaddr **internet_address, socklen_t *internet_address_length);
void execution(int internet_socket, struct sockaddr *internet_address, socklen_t internet_address_length);
void cleanup(int internet_socket, struct sockaddr *internet_address);

int main(int argc, char *argv[])
{
    //////////////////
    //Initialization//
    //////////////////

    OSInit();

    struct sockaddr *internet_address = NULL;
    socklen_t internet_address_length = 0;
    int internet_socket = initialization(&internet_address, &internet_address_length);

    /////////////
    //Execution//
    /////////////

    execution(internet_socket, internet_address, internet_address_length);

    ///////////
    //Cleanup//
    ///////////

    cleanup(internet_socket, internet_address);

    OSCleanup();

    return 0;
}

int initialization(struct sockaddr **internet_address, socklen_t *internet_address_length)
{
    //Step 1.1
    struct addrinfo internet_address_setup; //Stack variable
    struct addrinfo *internet_address_result; //Stack variable
    memset(&internet_address_setup, 0, sizeof(internet_address_setup)); //initialiseert de struct op 0
    internet_address_setup.ai_family = AF_INET; //ai_family -> ipv4 of ipv6 --> geen waarde meegegeven
    internet_address_setup.ai_socktype = SOCK_DGRAM; // -> socket type
    int getaddrinfo_return = getaddrinfo("::1", "24042", &internet_address_setup, &internet_address_result); // ::1 --> local host IPV6 formaat --> 127.0.0.1 is hetzelfde 
    //Als getaddrinfo niet gelijk is aan 0 is er een fout in de functie getaddrinfo (vb. IP adres, poort of foute pointer).
    if(getaddrinfo_return != 0) //Geeft een foutmelding als er iets mis is met "getaddrinfo"
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1); //Exit code 1 is nu gereserveerd voor een fout in de getaddrinfo
    }

    //Step 1.2
    int internet_socket;
    internet_socket = socket(internet_address_result -> ai_family, internet_address_result -> ai_socktype, internet_address_result -> ai_protocol);
    if(internet_socket == -1) //Geeft een foutmelding als er iets mis is met "internet_socket"
    {
        perror("Socket");
        freeaddrinfo(internet_address_result);
        exit(2); //Exit code 2 is nu gereserveerd voor een fout in socket
    }

    //Step 1.3
    *internet_address_length = internet_address_result -> ai_addrlen;
    *internet_address = (struct sockaddr *) malloc(internet_address_result -> ai_addrlen);
    memcpy(*internet_address, internet_address_result -> ai_addr, internet_address_result -> ai_addrlen);

    freeaddrinfo(internet_address_result);

    return internet_socket;
}

void execution(int internet_socket, struct sockaddr *internet_address, socklen_t internet_address_length)
{
    //Step 2.1
    int number_of_bytes_send = 0;
    int numberOfPacketsToSend = 0; //Aantal te verzenden pakketten (int)
    char buffer[20]; //Volgnummer pakket (char)
    char keuze;

    printf("Geef het aantal pakketten in: ");                               //Vraagt het aantal te verzenden pakketten
    scanf("%d", &numberOfPacketsToSend);

    itoa(numberOfPacketsToSend, buffer, 10);                                //Converteerd het aantal te verzenden pakketten van int naar char
    printf("Aantal te verzenden pakketten: %s\n", buffer);

    printf("Wilt u het aantal te ontvangen pakketten mee sturen? [y/n] ");
    scanf(" %c", &keuze);

    if(keuze == 'y' || keuze == 'Y')
    {
        sendto(internet_socket, itoa(numberOfPacketsToSend, buffer, 10), strlen(buffer), 0, internet_address, internet_address_length); //Verzend aantal te ontvangen pakketten
    }

    for(int i = 0; i < numberOfPacketsToSend; i++)
    {   
        number_of_bytes_send = sendto(internet_socket, itoa(i, buffer, 10), strlen(buffer), 0, internet_address, internet_address_length);
        //sendto(internet_socket, Te sturen data, lengte van de data, flags, adres waar de data heen moet, lengte van het adres);
        if(number_of_bytes_send == -1)
        {
            perror("Sendto");
        }
    }
}

void cleanup(int internet_socket, struct sockaddr *internet_address)
{
    //Step 3.2
    free(internet_address); //Vrij maken van de malloc

    //Step 3.1
    close(internet_socket);
}
