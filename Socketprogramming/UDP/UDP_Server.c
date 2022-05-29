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

void calcPacketloss(int ontvangenPakketten, int verwachttePakketten, FILE *fpStats);
void cleanup(int internet_socket);


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
    int timeout = 1000;
    struct addrinfo internet_address_setup; //Stack variable
    struct addrinfo *internet_address_result; //Stack variable
    memset(&internet_address_setup, 0, sizeof(internet_address_setup)); //initialiseert de struct op 0
    internet_address_setup.ai_family = AF_INET; //ai_family -> ipv4 of ipv6 --> geen waarde meegegeven
    internet_address_setup.ai_socktype = SOCK_DGRAM; // -> socket type
    internet_address_setup.ai_flags = AI_PASSIVE; // de server mag van elk ip adres een verbinding verwachten
    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result); //NULL -> de server kan van elk ip adres worden benaderd -> NULL is server
    
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
    double unknown; //Hier wordten de onbekende en onbelangrijke waarden in opgeslagen
    clock_t t;

    FILE *fpCSV = startCSV(); //Maakt of opent een CSV file
    FILE *fpStats = startStats(); //Maakt of opent CSV file voor statistische gegevens
    
    /*
    *Waarden die gebruikt worden om de data van de sensorstream app te parsen
    *[0] -> Accelerometer
    *[1] -> Gyroscope
    *[2] -> Magnetic Field
    * unknown, unknown, Accelerometer x, Accelerometer y, Accelerometer Z, unknown, Gyroscope X, Gyroscope Y, Gyroscope Z, unknown, Magnetic field X, Magnetic field Y, Magnetic field Z
    */
    
    double min_X[3] = {0, 0, 0};
    double avg_X[3] = {0, 0, 0};
    double max_X[3] = {0, 0, 0};

    double min_Y[3] = {0, 0, 0};
    double avg_Y[3] = {0, 0, 0};
    double max_Y[3] = {0, 0, 0};

    double min_Z[3] = {0, 0, 0};
    double avg_Z[3] = {0, 0, 0};
    double max_Z[3] = {0, 0, 0};

    double tempAccelerometer[3] = {0, 0, 0};
    double tempGyroscope[3] = {0, 0, 0};
    double tempMagnetic[3] = {0, 0, 0};

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

            sscanf(buffer, "%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf", &unknown, &unknown, &tempAccelerometer[0], &tempAccelerometer[1], &tempAccelerometer[2], &unknown, &tempGyroscope[0], &tempGyroscope[1], &tempGyroscope[2], &unknown, &tempMagnetic[0], &tempMagnetic[1], &tempMagnetic[2]);

            /////////////////////////
            //Parsing accelerometer//
            /////////////////////////

            if(tempAccelerometer[0] < min_X[0]) //Als de X waarde van de accelerometer kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                min_X[0] = tempAccelerometer[0];
            }
            else if(tempAccelerometer[0] > max_X[0]) //Als de X waarde van de accelerometer groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_X[0] = tempAccelerometer[0];
            }
            else
            {
                //Doe niets
            }

            if(tempAccelerometer[1] < min_Y[0]) //Als de Y waarde van de accelerometer kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslage
            {
                min_Y[0] = tempAccelerometer[1];
            }
            else if(tempAccelerometer[1] > max_Y[0]) //Als de Y waarde van de accelerometer groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_Y[0] = tempAccelerometer[1];
            }
            else
            {
                //Doe niets
            }

            if(tempAccelerometer[2] < min_Z[0]) //Als de Z waarde van de accelerometer kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslage
            {
                min_Z[0] = tempAccelerometer[2];
            }
            else if(tempAccelerometer[2] > max_Z[0]) //Als de Z waarde van de accelerometer groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_Z[0] = tempAccelerometer[2];
            }
            else
            {
                //Doe niets
            }
            
            /////////////////////
            //Parsing gyroscope//
            /////////////////////

            if(tempGyroscope[0] < min_X[1]) //Als de X waarde van de gyroscope kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                min_X[1] = tempGyroscope[0];
            }
            else if(tempGyroscope[0] > max_X[1]) //Als de X waarde van de gyroscope groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_X[1] = tempGyroscope[0];
            }
            else
            {
                //Doe niets
            }

            if(tempGyroscope[1] < min_Y[1]) //Als de Y waarde van de gyroscope kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslage
            {
                min_Y[1] = tempGyroscope[1];
            }
            else if(tempGyroscope[1] > max_Y[1]) //Als de Y waarde van de gyroscope groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_Y[1] = tempGyroscope[1];
            }
            else
            {
                //Doe niets
            }

            if(tempGyroscope[2] < min_Z[1]) //Als de Z waarde van de gyroscope kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslage
            {
                min_Z[1] = tempGyroscope[2];
            }
            else if(tempGyroscope[2] > max_Z[1]) //Als de Z waarde van de gyroscope groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_Z[1] = tempGyroscope[2];
            }
            else
            {
                //Doe niets
            }

            //////////////////////////
            //Parsing magnetic field//
            //////////////////////////

            if(tempMagnetic[0] < min_X[2]) //Als de X waarde van de magnetic field  kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                min_X[2] = tempMagnetic[0];
            }
            else if(tempMagnetic[0] > max_X[2]) //Als de X waarde van de magnetic field groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_X[2] = tempMagnetic[0];
            }
            else
            {
                //Doe niets
            }

            if(tempMagnetic[1] < min_Y[2]) //Als de Y waarde van de magnetic field  kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslage
            {
                min_Y[2] = tempMagnetic[1];
            }
            else if(tempMagnetic[1] > max_Y[2]) //Als de Y waarde van de magnetic field  groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_Y[2] = tempMagnetic[1];
            }
            else
            {
                //Doe niets
            }

            if(tempMagnetic[2] < min_Z[2]) //Als de Z waarde van de magnetic field  kleiner is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslage
            {
                min_Z[2] = tempMagnetic[2];
            }
            else if(tempMagnetic[2] > max_Z[2]) //Als de Z waarde van de magnetic field  groter is als de vorige bekende kleinste waarde wordt de nieuwe waarde opgeslagen
            {
                max_Z[2] = tempMagnetic[2];
            }
            else
            {
                //Doe niets
            }
        }
        packetCounter++;
    }
    t = clock() - t;
    double timeTaken = ((double)t) / CLOCKS_PER_SEC;

    printf("Aantal ontvangen pakketten: %d in %f seconden\n", packetCounter, timeTaken);                                            //print het aantal ontvangen pakketten
    fprintf(fpStats, "\n%d Pakketten van de %d ontvangen in %.2f seconden.\n", packetCounter, packetsToReceive, timeTaken);         //print het aantal ontvangen pakketten en de tijd in de csv
    
    //Accelerometer

    fprintf(fpStats, "\n Accelerometer X min: %lf\n", min_X[0]);
    fprintf(fpStats, "\n Accelerometer y min: %lf\n", min_Y[0]);
    fprintf(fpStats, "\n Accelerometer z min: %lf\n", min_Z[0]);

    fprintf(fpStats, "\n Accelerometer X avg: %lf\n", avg_X[0]);
    fprintf(fpStats, "\n Accelerometer y avg: %lf\n", avg_Y[0]);
    fprintf(fpStats, "\n Accelerometer z avg: %lf\n", avg_Z[0]);

    fprintf(fpStats, "\n Accelerometer X max: %lf\n", max_X[0]);
    fprintf(fpStats, "\n Accelerometer y max: %lf\n", max_Y[0]);
    fprintf(fpStats, "\n Accelerometer z max: %lf\n", max_Z[0]);

    //Gyroscope

    fprintf(fpStats, "\n Gyroscope X min: %lf\n", min_X[1]);
    fprintf(fpStats, "\n Gyroscope y min: %lf\n", min_Y[1]);
    fprintf(fpStats, "\n Gyroscope z min: %lf\n", min_Z[1]);

    fprintf(fpStats, "\n Gyroscope X avg: %lf\n", avg_X[1]);
    fprintf(fpStats, "\n Gyroscope y avg: %lf\n", avg_Y[1]);
    fprintf(fpStats, "\n Gyroscope z avg: %lf\n", avg_Z[1]);

    fprintf(fpStats, "\n Gyroscope X max: %lf\n", max_X[1]);
    fprintf(fpStats, "\n Gyroscope y max: %lf\n", max_Y[1]);
    fprintf(fpStats, "\n Gyroscope z max: %lf\n", max_Z[1]);

    //Magnetic field

    fprintf(fpStats, "\n Magnetic field X min: %lf\n", min_X[2]);
    fprintf(fpStats, "\n Magnetic field y min: %lf\n", min_Y[2]);
    fprintf(fpStats, "\n Magnetic field z min: %lf\n", min_Z[2]);

    fprintf(fpStats, "\n Magnetic field X avg: %lf\n", avg_X[2]);
    fprintf(fpStats, "\n Magnetic field y avg: %lf\n", avg_Y[2]);
    fprintf(fpStats, "\n Magnetic field z avg: %lf\n", avg_Z[2]);

    fprintf(fpStats, "\n Magnetic field X max: %lf\n", max_X[2]);
    fprintf(fpStats, "\n Magnetic field y max: %lf\n", max_Y[2]);
    fprintf(fpStats, "\n Magnetic field z max: %lf\n", max_Z[2]);

    calcPacketloss(packetCounter, packetsToReceive, fpStats);                                                                       //Berekend packetloss
}


FILE *startCSV()
{
    FILE *fp = NULL; //Maakt een file pointer aan naar de CSV file

    fp = fopen("UDP_CSV.csv", "w"); //Opent de CSV file met write premission

    if(fp == NULL) //Geeft een foutmelding als het openen van het programma mislukt
    {
        printf("Unable to open or create CSV file.\n");
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
        printf("Unable to open or create StaticData file\n");
        exit(EXIT_FAILURE);
    }
    return fp;
}

void calcPacketloss(int ontvangenPakketten, int verwachttePakketten, FILE *fpStats)
{
    double packetLoss = 0;
    
    packetLoss = 1.0 - ((double) ontvangenPakketten / verwachttePakketten);
    packetLoss = packetLoss * 100;

    printf("Packetloss: %.2f%%", packetLoss);
    fprintf(fpStats, "Packetloss: %.2f%%\n", packetLoss); 
}

void cleanup(int internet_socket)
{
    //Step 3.1
    close(internet_socket);
}


