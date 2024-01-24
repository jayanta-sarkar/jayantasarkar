#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>


#define SOCKETADDR struct sockaddr
#define DEFAULT_UDP_PORT 6000
#define DEFAULT_TCP_PORT 8080
#define MAX_BUFF 200
#define LENGTH_OF_AVG_SEQ 10
#define LISTEN_BACKLOG 10
#define MAX_PROBE_REPORT 100
#define MAX_NO_OF_SENSORS 100
#define MAX_THREAD_COUNT 11
#define MAX_TCP_CLIENTS 20

#define IPV4_ADDR_SIZE 20
#define DEFAULT_SERVER_IP  "127.0.0.1"
#define MAXCHARS_IN_A_LINE 256
#define BASE_TEN 10
// Maxm No of threads supported

pthread_mutex_t lock;

typedef struct tm tm_t;

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef int int32_t;

// This is the format in which probe report would be received from sensor simulator
typedef struct probe_report_t {
    uint32_t probe_id;
    uint32_t measurement_id;
    int32_t  value;
} probe_report;

// config structure to be read from config file
typedef struct server_config_read_t {
    uint32_t length_of_averaging_seq;
    char server_ip[IPV4_ADDR_SIZE];
    uint32_t udp_server_port;
    uint32_t tcp_server_port;
    uint32_t maxm_probe_report_supported;
} server_config_parameters;

// As per given task client/user will consume data in this format
typedef struct sensor_data_st_t {
    uint32_t sensor_id;
    int32_t average_val;
    double time_interval;
} sensor_data_st;

typedef struct sensor_data_node_st_t {
    sensor_data_st sensor_data;
    struct sensor_data_node_st_t* next;
} sensor_data_node_st;

//Temporary structure to accumulate total value received from sensor
typedef struct sensor_tmp_st_t {
    uint32_t sensor_id;
    int32_t total_val;
    int32_t average_val;
    uint32_t no_of_samples;
    time_t start_time;
} sensor_tmp_st;

//Hashmap table which keeps average value to consume
typedef struct hashmap_storage_st_t {
    uint32_t capacity;
    uint32_t count_of_sensors;
    sensor_data_node_st** head_node_addr;
} hashmap_storage_st;

//Temporary hashmap to keep accumulated nodes(in calculate nodes)
typedef struct hashmap_tmp_st_t {
    uint32_t capacity;
    uint32_t count_of_sensors;
    sensor_tmp_st sensor_tmp_data[MAX_NO_OF_SENSORS];
} hashmap_tmp_st;

hashmap_storage_st* ht_storage = NULL;
hashmap_tmp_st* ht_tmp = NULL;

//Default values of config parameters
server_config_parameters server_config_params = { LENGTH_OF_AVG_SEQ, DEFAULT_SERVER_IP, DEFAULT_TCP_PORT, DEFAULT_UDP_PORT};

pthread_t tid;

//This function sanitizes lines read from config
void sanitize_buffer_read_from_server_config( char buff[], char in_buff[]) {
    int j = 0;
    //At first remove any spaces
    for(int i = 0; i < strlen(in_buff);i++) {
        if(in_buff[i] == ' ') {
            continue;
        }
        in_buff[j] = in_buff[i];
        j++;
    }
    //Remove new line characters
    char* token = strtok(in_buff, "\n");
    strcpy(buff, token);
}
//This function parses config data read from file and puts in a global structure
void parse_server_config_value( FILE* file_handler) {
    char in_buff[MAXCHARS_IN_A_LINE], buff[MAXCHARS_IN_A_LINE];
    if( file_handler) {
        char* token = NULL;
        int i = 0;
        char config_param_name[MAX_BUFF], config_param_value[MAX_BUFF];
        uint32_t temp;

        while ( fgets ( in_buff, MAXCHARS_IN_A_LINE, file_handler)) {
            if ( strstr( in_buff, "=")) {

                sanitize_buffer_read_from_server_config ( buff, in_buff);

                token = strtok ( buff, "=");
                strcpy ( config_param_name, token);
                i++;
                token = strtok ( NULL, "=") ;
                strcpy ( config_param_value, token);
                i++;

                if( !strcmp ( "LENGTH_OF_AVERAGE_SEQUENCE", config_param_name)) {
                    temp = strtoul ( config_param_value, 0, BASE_TEN);
                    server_config_params.length_of_averaging_seq = temp;
                }
                else if ( !strcmp ( "SERVER_IP", config_param_name)) {
                    strcpy(server_config_params.server_ip, config_param_value);
                }
                else if ( !strcmp ( "TCP_SERVER_PORT", config_param_name)) {
                    temp = strtoul ( config_param_value, 0, BASE_TEN);
                    server_config_params.tcp_server_port = temp;
                }
                else if ( !strcmp ( "UDP_SERVER_PORT", config_param_name)) {
                    temp = strtoul ( config_param_value, 0, BASE_TEN);
                    server_config_params.udp_server_port = temp;
                }
                else {
                    //Add more parsing if there are more config params
                }
            }
        }
    }
}



uint32_t get_bucket_index_from_tmp_hashtbl( uint32_t device_id) {

    int bucket_index = device_id % ht_tmp->capacity;

    return bucket_index;
}

uint32_t get_bucket_index_from_storage_hashtbl( uint32_t device_id) {

    int bucket_index = device_id % ht_storage->capacity;

    return bucket_index;
}

//This function initializes hash tables
int initialize_hashtbl() {

    ht_tmp->capacity = MAX_NO_OF_SENSORS;
    ht_tmp->count_of_sensors = 0;

    int i;

    for(i = 0; i < ht_tmp->capacity; i++){

        ht_tmp->sensor_tmp_data[i].total_val = 0;
        ht_tmp->sensor_tmp_data[i].no_of_samples = 0;
        ht_tmp->sensor_tmp_data[i].sensor_id = 0;
        ht_tmp->sensor_tmp_data[i].start_time = 0;
    }

    ht_storage->capacity = MAX_NO_OF_SENSORS;
    ht_storage->count_of_sensors = 0;

    ht_storage->head_node_addr = ( sensor_data_node_st**) malloc ( sizeof( sensor_data_node_st*) * ht_storage->capacity);

    for(i = 0; i < ht_storage->capacity; i++){

        ht_storage->head_node_addr[i] = NULL;
    }

    return 0;
}

// Traversing the hash table, just for display purpose
int traverse_hash_table () {

    int i = 0;
    int j = 0;
    printf( "\n-------------------In storage------------------\n");
    if ( ht_storage) {
        while( j < ht_storage->count_of_sensors) {

            if( ht_storage->head_node_addr[i] == NULL) {

                i++;
            }
            else {

                printf("\n------For Sensor %u----------------------\n",ht_storage->head_node_addr[i]->sensor_data.sensor_id);

                sensor_data_node_st* temp = ht_storage->head_node_addr[i];
                while ( temp){
                    printf( "sensor_id is : %u \n", temp->sensor_data.sensor_id);
                    printf( "average_val is : %d \n", temp->sensor_data.average_val);
                    printf( "time interval : %f \n", temp->sensor_data.time_interval);
                    temp = temp->next;

                }
                i++;
                j++;
            }

        }
    }
    else {
        printf("Empty hash table! \n");
    }

    return 0;
}

//This function erases sensor data after it is read by a client/user
int delete_linked_list(sensor_data_node_st* sensor_data_node) {

    if ( sensor_data_node == NULL) {
        printf("Empty list \n");
    }

    sensor_data_node_st* head = sensor_data_node;

    sensor_data_node_st* temp = head;

    while( temp) {

        head = temp->next;
        free(temp);
        temp = head;
    }

    return 0;
}

//Delete hash table & its contents
int delete_hash_table () {
    if ( ht_storage) {
        for ( int i = 0; i < ht_storage->capacity; i++) {

            if ( ht_storage->head_node_addr[i]) {

                delete_linked_list(ht_storage->head_node_addr[i]);
            }
        }

        free ( ht_storage);
    }
    if ( ht_tmp) {
        free ( ht_tmp);
    }

    return 0;
}

//This function adds a calculated sensor average value(node) in hash table
int add_into_hash_storage( sensor_data_node_st* sensor_data_node) {

    uint32_t index = get_bucket_index_from_storage_hashtbl ( sensor_data_node->sensor_data.sensor_id);

    if( ht_storage->head_node_addr[index] == NULL) {
        //New sensor insert the first node
        ht_storage->head_node_addr[index] = sensor_data_node;
        ht_storage->count_of_sensors = ht_storage->count_of_sensors + 1;

        return 0;
    }
    if( ht_storage->head_node_addr[index]->sensor_data.sensor_id != sensor_data_node->sensor_data.sensor_id) {
        printf("Warning! Collision! data can't be stored for sensor_id %u  - For now please ensure given probe_id below 100 \n", sensor_data_node->sensor_data.sensor_id);

        return 0;
    }
    printf("Sensor exists - add the new average at the front \n");
    sensor_data_node->next = ht_storage->head_node_addr[index];
    ht_storage->head_node_addr[index] = sensor_data_node;

    return 0;
}


//This function fetches linked list for matching sensor
int fetch_sensor_data_from_hash_storage ( sensor_data_node_st** sensor_data, uint32_t sensor_id) {

    if ( ht_storage->count_of_sensors == 0){
        printf(" Empty hastable!  - please wait for sensor value accumulation also pls ensure sensor simulator is running \n");

        * sensor_data = NULL;

        return -1;
    }

    uint32_t index = get_bucket_index_from_storage_hashtbl ( sensor_id);

    if ( ht_storage->head_node_addr[index] == NULL) {

        //This sensor data is not yet accumulated
        printf( "This Sensor data not accumulated yet \n");

        * sensor_data = NULL;

        return -1;
    }

    *sensor_data = ( sensor_data_node_st *) malloc ( sizeof( sensor_data_node_st));

    *sensor_data = ht_storage->head_node_addr[index];

    //Don't yet delete the entry in hashtable - let's send it first

    ht_storage->head_node_addr[index] = NULL;

    ht_storage->count_of_sensors = ht_storage->count_of_sensors - 1;

    return 0;
}

//This function processes incoming data from all sensors and calculates average and puts the data in a hash table memory
int process_report(const char* buffer) {

    probe_report* probe_report_val = ( probe_report*)buffer;

    uint32_t index = get_bucket_index_from_tmp_hashtbl( probe_report_val->probe_id);


    //Check if it is a new sensor data
    if ( ht_tmp->sensor_tmp_data[index].no_of_samples == 0) {

        //New Sensor, insert data
        ht_tmp->sensor_tmp_data[index].sensor_id = probe_report_val->probe_id;
        ht_tmp->sensor_tmp_data[index].total_val = ht_tmp->sensor_tmp_data[index].total_val + probe_report_val->value;

        //Note down start time
        time_t start_time;
        time(&start_time);
        ht_tmp->sensor_tmp_data[index].start_time = start_time;
        ht_tmp->sensor_tmp_data[index].no_of_samples = ht_tmp->sensor_tmp_data[index].no_of_samples + 1;

        ht_tmp->count_of_sensors = ht_tmp->count_of_sensors + 1;

        return 0;
    }
    //Check if the sensor id doesn't match but hashed to the same index-> new sensor colliding with stored one!
    else if( ht_tmp->sensor_tmp_data[index].sensor_id != probe_report_val->probe_id) {
        printf( "Warning! Collision! data can't be processed for probe_id %u  - For now please ensure given probe_id below 100 \n", probe_report_val->probe_id);
        return 0;
    }
    else {
        //Already this sensor's value is getting accumulated
        if( ht_tmp->sensor_tmp_data[index].no_of_samples > 0 && ht_tmp->sensor_tmp_data[index].no_of_samples < server_config_params.length_of_averaging_seq) {

            //Update the entry in the hashmap
            ht_tmp->sensor_tmp_data[index].total_val = ht_tmp->sensor_tmp_data[index].total_val + probe_report_val->value;
            ht_tmp->sensor_tmp_data[index].no_of_samples = ht_tmp->sensor_tmp_data[index].no_of_samples + 1;
        }
        //Check now if samples collected reached till length of averaging sequence
        if( ht_tmp->sensor_tmp_data[index].no_of_samples == server_config_params.length_of_averaging_seq){

            //Measurement has been completed for this interval for this sensor->Data needed to be stored in main hash table
            //Find out current time
            time_t end_time;
            time( &end_time);
            //Calculate time interval to accumulate 'length_of_averaging_seq' of particular sensor's value
            double time_interval = difftime( end_time, ht_tmp->sensor_tmp_data[index].start_time);

            //create a node holding sensor's average data
            sensor_data_node_st* sensor_data_node = (sensor_data_node_st*)malloc(sizeof(sensor_data_node_st));

            sensor_data_node->sensor_data.sensor_id = ht_tmp->sensor_tmp_data[index].sensor_id;
            sensor_data_node->sensor_data.average_val = ht_tmp->sensor_tmp_data[index].total_val / server_config_params.length_of_averaging_seq;
            sensor_data_node->sensor_data.time_interval = time_interval;
            sensor_data_node->next = NULL;

            //Add sensor data in main hashtable
            //Lock the critical section
            pthread_mutex_lock( &lock);
            add_into_hash_storage( sensor_data_node);
            //Display the hashtable
            traverse_hash_table();
            //Lock the critical section
            pthread_mutex_unlock(&lock);

            //Reset all measurement params to accumulate further from now on
            ht_tmp->sensor_tmp_data[index].total_val = 0;
            ht_tmp->sensor_tmp_data[index].no_of_samples = 0;
            ht_tmp->sensor_tmp_data[index].sensor_id = 0;
            ht_tmp->sensor_tmp_data[index].start_time = 0;

            ht_tmp->count_of_sensors = ht_tmp->count_of_sensors - 1;

        }
    }

    return 0;
}

// This function reads probe report/ sensor data from sensor simulator
int sensor_data_receive_through_udp() {
    char buffer[MAX_BUFF];
    int sockfd;
    struct sockaddr_in udpserveraddr, udpclientaddr;

    socklen_t addrlen;

    // clear sensoraddr
    memset( &udpserveraddr, 0, sizeof( udpserveraddr));

    // create datagram socket
    sockfd = socket( AF_INET, SOCK_DGRAM, 0);
    if( sockfd < 0){
        perror( "socket creation ");
        return -1;
    }

    int optval = 1;
    setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR,
                ( const void *)&optval , sizeof( int));

    udpserveraddr.sin_addr.s_addr = inet_addr( server_config_params.server_ip);
    udpserveraddr.sin_port = htons( server_config_params.udp_server_port);
    udpserveraddr.sin_family = AF_INET;

    // bind udp server address to socket descriptor
    if( bind( sockfd, ( SOCKETADDR*)&udpserveraddr, sizeof( udpserveraddr)) < 0){
        perror( "socket bind ");
        return -1;
    }
    printf( "\n--Server is ready, please start the sensor now to consume data and wait for sensor measurements to be accumulated---\n");

    addrlen = sizeof( udpclientaddr);

    //Read sensor report
    memset( buffer, '\0', MAX_BUFF);
    while ( 1) {

        if( recvfrom( sockfd, buffer, sizeof( buffer), 0, ( SOCKETADDR*)&udpclientaddr, &addrlen) < 0) {
            perror( "receive ");

            return -1;
        }

        process_report (( const char*)buffer);

        memset ( buffer, '\0', MAX_BUFF);

    }


    // close the descriptor
    close ( sockfd);

    return 0;

}

//This functions handles all tcp clients, it reads the request and reply with sensor's data asked for
int handle_incoming_tcp_client_command ( int tcp_client_sockfd, char* buff) {

    int len, len1, result = 0;
    uint32_t count = 0, sensor_id = *( uint32_t*)buff;
    sensor_data_node_st *sensor_data_node = NULL, *temp = NULL;


    char writebuff[MAX_BUFF] = {'\0'};

    printf ( "---Data requested for %u sensor----\n", sensor_id);

    //Lock the critical section before entering into it
    pthread_mutex_lock ( &lock);

    if ( fetch_sensor_data_from_hash_storage ( &sensor_data_node, sensor_id) == - 1) {

        //There is no data found, so send n/a
        if( write ( tcp_client_sockfd, "n/a", sizeof("n/a")) < 0){

            perror("write ");

        }

    }
    else {

        //Data found, send it
        temp = sensor_data_node;

        //Calculate how many nodes are there in the linked list, since data for same sensor can be collected for multiple intervals
        while ( temp) {

            count = count + 1;
            temp = temp->next;
        }

        //send at first no of data to be sent
        if ( write( tcp_client_sockfd, ( char*)&count, sizeof( count)) < 0) {

            //The data couldn't be sent at the moment  - so, better keep the same in the memory(hashtable)

            perror ( "write ");
        }

        temp = sensor_data_node;

        //usleep( 10);

        //Now send the data

        while ( count) {

            usleep( 10);

            //clear the buffer first
            memset ( writebuff, '\0', MAX_BUFF);

            len1 = sizeof ( temp->sensor_data);

            memcpy (( void*)writebuff, ( void*)&temp->sensor_data, len1);

            if (( len =  write( tcp_client_sockfd, ( char*)writebuff, len1)) < 0) {

                //The data couldn't be sent at the moment  to the concerned client - so, better keep the same in the memory(hashtable)
                perror ( "write ");
                break;

            }
            else {
                temp = temp->next;
                count = count - 1;

            }
        }

    }
    //Delete the fetched sensor data
    delete_linked_list ( sensor_data_node);

    //After deletion have look at the hashtable
    traverse_hash_table ();


    pthread_mutex_unlock ( &lock);


    return result;
}

//This function handles tcp request from each client
void* tcp_connection_module ( void* arg) {

    int tcp_server_sockfd, new_tcp_conn_socket, max_sockfd, activity, i;
    int tcp_client_socket [ MAX_TCP_CLIENTS] = { 0};
    char* message = "Welcome to sensor measurement server!";
    char buff [ MAX_BUFF];
    fd_set readfds;
    struct sockaddr_in servaddr, clientaddr;
    socklen_t addrlen;

    // socket create and verification
    tcp_server_sockfd = socket ( AF_INET, SOCK_STREAM, 0);
    if ( tcp_server_sockfd == -1) {
        perror ( "socket ");

        pthread_exit ( NULL);
    }
    else
        printf ( "Server tcp socket successfully created..\n");
    memset ( &servaddr, 0, sizeof ( servaddr));

    //set socket to reuse IP address
    int optval = 1;
    if ( setsockopt ( tcp_server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof ( optval))) {
        perror ( "setsockopt ");

        pthread_exit ( NULL);
    }

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr ( server_config_params.server_ip);
    servaddr.sin_port = htons ( server_config_params.tcp_server_port);

    // Binding newly created socket to given IP and verification
    if (( bind ( tcp_server_sockfd, ( SOCKETADDR*)&servaddr, sizeof ( servaddr))) != 0) {
        perror ( "bind ");

        pthread_exit ( NULL);
    }
    else
        printf ( "Server tcp socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen ( tcp_server_sockfd, LISTEN_BACKLOG)) != 0) {
        perror ( "listen ");

        pthread_exit ( NULL);
    }

    addrlen = sizeof ( clientaddr);

    while ( 1) {

        //reset the socket set
        FD_ZERO ( &readfds);

        //Server socket is also added in the set
        FD_SET ( tcp_server_sockfd, &readfds);

        //Findout highest socket fd
        max_sockfd = tcp_server_sockfd;

        for( i = 0; i < MAX_TCP_CLIENTS; i++){

            if (( tcp_client_socket[i]) > 0){
                FD_SET ( tcp_client_socket[i], &readfds);
            }
            if ( tcp_client_socket[i] > max_sockfd){
                max_sockfd = tcp_client_socket[i];
            }

        }
        //wait indefinitely for an activity on one of the sockets
        activity = select ( max_sockfd + 1 , &readfds , NULL , NULL , NULL);

        if (( activity < 0) && ( errno != EINTR)) {
            printf ( "select error \n");
        }

        //Check if something happened in the server socket, possibly an incoming connection
        if ( FD_ISSET ( tcp_server_sockfd, &readfds)){

            printf ( "Try to accept a new client connection \n");
            new_tcp_conn_socket = accept ( tcp_server_sockfd, ( SOCKETADDR*)&clientaddr, &addrlen);
            if ( new_tcp_conn_socket < 0) {

                perror( "server accept failed \n");

                pthread_exit ( NULL);
            }

            printf ( "----New client connected from port no : %d IP: %s and sockfd : %d-----\n",
                     ntohs ( clientaddr.sin_port),inet_ntoa ( clientaddr.sin_addr), new_tcp_conn_socket);

            //send welcome message
            if ( write ( new_tcp_conn_socket, message, sizeof( *message)) < 0) {

                perror ( "send ");
            }

            //add new client socket to array of client sockets
            for( i = 0; i < MAX_TCP_CLIENTS; i++) {

                if( tcp_client_socket[i] == 0 )
                    {
                        tcp_client_socket[i] = new_tcp_conn_socket;

                        break;
                    }
            }
        }
        else {
            //Otherwise it's an event on another socket
            for( i = 0; i < MAX_TCP_CLIENTS; i++) {

                if ( FD_ISSET ( tcp_client_socket[i], &readfds)) {

                    //check if this is for a client disconnection
                    if( read ( tcp_client_socket[i], buff, sizeof( buff)) <= 0) {

                        getpeername ( tcp_client_socket[i], ( struct sockaddr*)&clientaddr, ( socklen_t*)&addrlen);
                        printf ( "Client disconnected!, ip: %s , port: %d \n" ,
                                 inet_ntoa(clientaddr.sin_addr) , ntohs(clientaddr.sin_port));
                        //close fd
                        close ( tcp_client_socket[i]);
                        tcp_client_socket[i] = 0;
                    }

                    else {

                        getpeername ( tcp_client_socket[i], ( struct sockaddr*) &clientaddr, &addrlen);
                        printf ( "------Data requested by client, ip: %s , port: %d , tcp_client_sockfd is %d ----------\n",
                                 inet_ntoa( clientaddr.sin_addr) , ntohs( clientaddr.sin_port), tcp_client_socket[i]);
                        handle_incoming_tcp_client_command ( tcp_client_socket[i], buff);


                    }
                }
            }
        }
    }

    close ( tcp_server_sockfd);
    pthread_exit ( NULL);
}

void signal_handler(int signal_number) {
    printf("Handle signal \n");
    delete_hash_table ();

    exit(0);
}

int main ( int argc, char* argv[]){

    /* Assign signal handlers to signals. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    FILE* file_handler = fopen ( argv[1], "r");

    if( file_handler){

        parse_server_config_value ( file_handler);
    }
    else {
        perror ( "error ");
        printf ( "Problem with config read, Using default value instead of config");
    }

    ht_storage = ( hashmap_storage_st*) malloc( sizeof ( hashmap_storage_st));
    ht_tmp = ( hashmap_tmp_st*) malloc( sizeof ( hashmap_tmp_st));

    initialize_hashtbl();

    if ( pthread_mutex_init ( &lock, NULL) != 0){
        perror ( "mutex_init ");

        return -1;
    }

    if ( pthread_create ( &tid, NULL, tcp_connection_module, NULL) != 0){
        perror ( "pthread_create() error ");
        pthread_exit ( NULL);
    }

    //Receive sensor data through udp
    sensor_data_receive_through_udp ();

    if ( pthread_join ( tid, NULL) != 0) {
        perror ( "pthread_join() error");
        pthread_exit ( NULL);
    }

    printf("Delete hastable \n");
    delete_hash_table ();

    return 0;
}
