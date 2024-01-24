#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#define MAXSIZE 100
#define BASE_TEN 10
#define SOCKETADR struct sockaddr

typedef unsigned int uint32_t;

//Measured value obtained in this format from server
typedef struct sensor_data_st_t {
    uint32_t sensor_id;
    int32_t average_val;
    double time_interval;
} sensor_data_st;

void get_sensor_data ( int sockfd) {
    char buff [ MAXSIZE];
    int n;
    char ch = ' ';
    while ( 1) {

        printf ( "Needed sensor results ? y/n \n");
        scanf ( " %c", &ch);
        if ( ch == 'y') {
            memset ( buff, 0, sizeof(buff));
            printf ( "Enter the sensor id in unsigned integer whose data is needed : ");
            uint32_t sensor_id;

            scanf ( "%u", &sensor_id);
            strcpy ( buff, ( char*)&sensor_id);

            write ( sockfd, buff, sizeof ( buff));
            memset ( buff, 0, sizeof ( buff));

            n = read ( sockfd, buff, sizeof ( buff));
            if ( n < 0) {

                perror("read ");
				break;
            }
            if ( !strcmp( "n/a", buff)) {
                puts(buff);
                continue;
            }

            uint32_t intervals = *( uint32_t*) buff;

            printf ( "----Received from Server : There are %u intervals data accumulated for the sensor #%u ---- \n",
                     intervals, sensor_id);

           
            while( intervals) {
                memset ( buff, 0, sizeof(buff));

                n = read ( sockfd, buff, sizeof( buff));
                if ( n < 0 ) {

                    perror ( "recv ");
                }
                
                sensor_data_st* data = ( sensor_data_st*) buff;                
                printf ( "sensor_id : %u average_val : %d time interval %f seconds\n", data->sensor_id, data->average_val, data->time_interval);
                intervals = intervals - 1;

            }
           

        }
        else if ( ch == 'n') {
            break;
        }
        else {
            printf("\n Wrong choice entered: please provide corect choice");
        }
    }
}

int main(int argc, char* argv[])
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
    char buff[MAXSIZE];

    // socket create and verification
    sockfd = socket ( AF_INET, SOCK_STREAM, 0);
    if ( sockfd == -1) {
        printf( "socket creation failed...\n");
        exit( 0);
    }
    else
        printf( "Socket successfully created..\n");
    memset( &servaddr, 0, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr( argv[1]);
    uint32_t server_port = strtoul( argv[2], 0, BASE_TEN);
    servaddr.sin_port = htons( server_port);

    // connect the client socket to server socket
    if ( connect( sockfd, (SOCKETADR*)&servaddr, sizeof(servaddr))
         != 0){
        printf ( "connection with the server failed...\n");
        exit(0);
    }
    else {
        read ( sockfd, buff, sizeof(buff));
        puts ( buff);
    }

    // function for chat
    get_sensor_data ( sockfd);

    // close the socket
    close ( sockfd);
}
