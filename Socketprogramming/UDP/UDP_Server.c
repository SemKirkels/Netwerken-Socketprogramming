#ifdef _WIN32

    #define _WIN32_WINNT _WIN32_WINNT_WIN7 //Select minimal legacy support, needed for inet_pton, inet_ntop
    #include <winsock2.h> //for all socket programming
    #include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
    #include <stdio.h> //for fprintf
    #include <unistd.h> //for close
    #include <stdlib.h> //for exit
    #include <string.h> //for memset
    #include <time.h>

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
void execution(int internet_socket);
FILE *startCSV(); //Maakt of opent een CSV file
FILE *startStats();
void cleanup(int internet_socket);
void calcPacketloss(int ontvangenPakketten, int verwachttePakketten);

int main(int argc, char *argv[])
{
    //////////////////
    //Initialization//
    //////////////////

    OSInit();

    int internet_socket = initialization();

    /////////////
    //Execution//
    /////////////

    execution(internet_socket);

    ///////////
    //Cleanup//
    ///////////

    cleanup(internet_socket);

    OSCleanup();

    return 0;
}


int initialization()
{
    /*
    *Het programma geeft het server IP en de poort weer
    */
    printf("De server luistert op poort 24042. In onderstaand venster wordt het ip adres vermeld bij IPv4\n");
    system("ipconfig");

    //Step 1.1
    struct addrinfo internet_address_setup; //Stack variable
    struct addrinfo *internet_address_result; //Stack variable
    memset(&internet_address_setup, 0, sizeof(internet_address_setup)); //initialiseert de struct op 0
    internet_address_setup.ai_family = AF_UNSPEC; //ai_family -> ipv4 of ipv6 --> geen waarde meegegeven
    internet_address_setup.ai_socktype = SOCK_DGRAM; // -> socket type
    internet_address_setup.ai_flags = AI_PASSIVE; // de server mag van elk ip adres een verbinding verwachten
    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result); //NULL -> de server kan van elk ip adres worden benaderd -> NULL is server
    
    int timeout = 1000;
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
            //Step 1.3 (BIND -> Step1.3 server is niet gelijk aan Step 1.3 client)
            int bind_return = bind(internet_socket, internet_address_result_iterator -> ai_addr, internet_address_result_iterator -> ai_addrlen);
            if(bind_return == -1)
            {
                close(internet_socket);
                perror("bind");
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
    
    printf("Stel de time out in (ms): ");
    scanf("%d", &timeout);
    
    if (setsockopt (internet_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed\n");
        exit(-1);
    }
    
    printf("\nStart nu de sensorstream app of UDP client\n\n");

    return internet_socket;
}


void execution(int internet_socket)
{
    //Step 2.1
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof(client_internet_address);
    int number_of_bytes_received = 0;
    char buffer[1000]; //te ontvangen data

    char numberOfPackets[1000]; //Aantal te ontvangen pakketten (char)
    int packetsToReceive = 0; //Aantal te ontvangen pakketten (int)
    int packetCounter = 0; //Aantal ontvangen pakketten
    char keuze;
    clock_t t;

    FILE *fpCSV = startCSV(); //Maakt of opent een CSV file
    FILE *fpStats = startStats(); //Maakt of opent CSV file voor statistische gegevens
    

    /*
    *Waarden die gebruikt worden om de data van de sensorstream app te parcen
    *[0] -> Accelerometer
    *[1] -> Gyroscope
    *[2] -> Magnetic Field
    */
    double min_X[2] = {0, 0, 0,};
    double avg_X[2] = {0, 0, 0,};
    double max_X[2] = {0, 0, 0,};

    double min_Y[2] = {0, 0, 0,};
    double avg_Y[2] = {0, 0, 0,};
    double max_Y[2] = {0, 0, 0,};

    double min_Z[2] = {0, 0, 0,};
    double avg_Z[2] = {0, 0, 0,};
    double max_Z[2] = {0, 0, 0,};

    while(1)
    {
        printf("Wordt het aantal te ontvangen pakketten mee gestuurd? [y/n] ");
        scanf(" %c", &keuze);

        if(keuze == 'y' || keuze == 'Y')
        {   
            recvfrom(internet_socket, numberOfPackets, (sizeof(numberOfPackets)) - 1, 0, (struct sockaddr *) &client_internet_address, &client_internet_address_length); 
            //Leest het eerste pakket om er achter te komen hoeveel pakketten er gaan volgen
            packetsToReceive = atoi(numberOfPackets); //Converteert de string met het aantal te ontvangen pakketten
            break;
        }
        else if(keuze == 'n' || keuze == 'N')
        {
            printf("Geef het aantal te ontvangen pakketten op: ");
            scanf("%d", &packetsToReceive);
            break;
        }
        else
        {
            //Doe niets
        }
    }   

    printf("Aantal te ontvangen pakketten: %d\n", packetsToReceive);

    for(int i = 0; i < packetsToReceive; i++) //Wacht op pakketten tot dat het aantal opgegeven pakketten is ontvangen
    {
        number_of_bytes_received = recvfrom(internet_socket, buffer, (sizeof(buffer)) - 1, 0, (struct sockaddr *) &client_internet_address, &client_internet_address_length);
        if(number_of_bytes_received == -1)
        {
            perror("recvfrom");
            packetCounter--;
        }
        else
        {
            if(i == 0)
            {
                t = clock(); //Timer start
                printf("Timer start\n");
            }
            buffer[number_of_bytes_received] = '\0';
            printf("Received: %s\n", buffer);
            fprintf(fpCSV, "%s, \n", buffer);
        }
        packetCounter++;
    }
    t = clock() - t;
    double timeTaken = ((double)t) / CLOCKS_PER_SEC;

    printf("Aantal ontvangen pakketten: %d in %f seconden\n", packetCounter, timeTaken);              //print het aantal ontvangen pakketten
    fprintf(fpStats, "\n%d Pakketten ontvangen in %.2f seconden.\n", packetCounter, timeTaken);       //print het aantal ontvangen pakketten en de tijd in de csv
    calcPacketloss(packetCounter, packetsToReceive);                                                       //Berekend packetloss
}


FILE *startCSV()
{
    FILE *fp = NULL; //Maakt een file pointer aan naar de CSV file

    fp = fopen("UDP_CSV.csv", "w"); //Opent de CSV file met write premission

    if(fp == NULL) //Geeft een foutmelding als het openen van het programma mislukt
    {
        printf("Unable to open or create file.\n");
        exit(EXIT_FAILURE);
    }
    return fp;
}

FILE *startStats()
{
    FILE *fp = NULL;

    fp = fopen("UDP_StaticData.csv", "w");

    if(fp == NULL)
    {
        printf("Unable to open or create file\n");
        exit(EXIT_FAILURE);
    }
    return fp;
}


void calcPacketloss(int ontvangenPakketten, int verwachttePakketten)
{
    double packetLoss = 0;
    
    packetLoss = 1.0 - ((double) ontvangenPakketten / verwachttePakketten);
    packetLoss = packetLoss * 100;

    printf("Packetloss: %.2f%%", packetLoss);
}


void cleanup(int internet_socket)
{
    //Step 3.1
    close(internet_socket);
}

