// server program for udp connection
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>


// Sensor port at which sensor socket binds
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 6000
#define IPV4_ADDR_SIZE 20
#define SOCKETADDR struct sockaddr

#define BUFFLEN 200
#define MAXCHARS_IN_A_LINE 256
#define BASE_TEN 10

//Restricting random function in this limit
#define RANDUPPER 10
#define RANDLOWER 2

#define SIXTY 60


typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef int int32_t;

#define RANDNUMBER (rand() % (RANDUPPER - RANDLOWER + 1)) + RANDLOWER

// This structure can hold probe report along with frequence/sampling rate w.r.t sensor
typedef struct sensorstr_t {
    uint32_t freq_of_val_generation;
    uint32_t device_id;
    uint32_t measurement_id;
} sensor;

// probe report structure as per given task
typedef struct probe_report_t {
    uint32_t probe_id;
    uint32_t measurement_id;
    int32_t  value;
} probe_report;

typedef struct sensor_config_read_t {
    char server_ip[IPV4_ADDR_SIZE];
    uint32_t server_port;

} sensor_config_parameters;

//Default values of sensor config parameters
sensor_config_parameters sensor_config_params = {DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT};

//pipe ids
int pipe_fd[2];

uint8_t count_of_probe_reports_in_fifo = 0;

struct sockaddr_in serveraddr, clientaddr;

//This function sanitizes lines read from config
void sanitize_buffer_read_from_sensor_config( char buff[], char in_buff[]) {
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

//This function parses sensor config data read from file and puts in a global structure
void parse_sensor_config_value ( FILE* file_handler) {
    char in_buff[MAXCHARS_IN_A_LINE], buff[MAXCHARS_IN_A_LINE];
    if ( file_handler) {
        char* token = NULL;
        int i = 0;
        char config_param_name [ BUFFLEN], config_param_value [ BUFFLEN];
        uint32_t temp;

        while ( fgets ( in_buff, MAXCHARS_IN_A_LINE, file_handler)) {
            if ( strstr( in_buff, "=")) {
                sanitize_buffer_read_from_sensor_config ( buff, in_buff);

                token = strtok ( buff, "=");
                strcpy ( config_param_name, token);
                i++;
                
                token = strtok ( NULL, "=") ;
                strcpy ( config_param_value, token);
                i++;

                if ( !strcmp ( "SERVER_IP", config_param_name)) {

                    strcpy ( sensor_config_params.server_ip, config_param_value);
                }
                else if ( !strcmp ( "SERVER_PORT", config_param_name)) {

                    temp = strtoul ( config_param_value, 0, BASE_TEN);
                    sensor_config_params.server_port = temp;

                }
                else {
                    //Add more parsing if there are more config params
                }
            }
        }
    }
}


/* This function reads probe report/sensor value and sends it to server */
void* send_probe_reports ( void* arg) {

    printf ( "Sending sensor data to server \n");

    int sockfd = *( int*)arg;

    probe_report probe_report_read;
    uint8_t len = sizeof ( probe_report);
    while ( 1) {

        while ( count_of_probe_reports_in_fifo) {

            if ( read ( pipe_fd[0], &probe_report_read, len) == -1) {
                perror("read ");
                pthread_exit(NULL);
            }
            if ( sendto ( sockfd, ( const unsigned char*)&probe_report_read, len, 0, ( SOCKETADDR*)&serveraddr, sizeof ( serveraddr)) == -1) {
                perror ( "sendto ");

                pthread_exit ( NULL);
            }

            count_of_probe_reports_in_fifo--;

        }
        //Waiting for 1 s to check if next data has arrived
        sleep ( 1);
    }

}


/* This function generates some customized random value for each individual sensor and writes it to pipe */
int produce_and_write_sensor_values ( int sockfd, sensor sensor_var){

    probe_report report;

    report.probe_id = sensor_var.device_id;
    report.measurement_id = sensor_var.measurement_id;

    uint32_t wait_time = SIXTY / sensor_var.freq_of_val_generation;

    printf("\n------generate sensor value in given frequency and write in pipe; type cntrl-c for exit------\n");
    uint8_t len = sizeof ( report);

    while ( 1) {

        report.value = RANDNUMBER;

        if ( write(pipe_fd[1], ( const unsigned char *)&report, len) < 0) {
            perror ( "write ");
            
            return -1;
        }

        count_of_probe_reports_in_fifo = count_of_probe_reports_in_fifo + 1;
        sleep ( wait_time);
    }

    return 0;
}

int main ( int argc, char* argv[]) {
    //Update config parameters

    FILE* file_handler = fopen ( argv[1], "r");

    int sockfd;

    sensor sensor_var;

    if ( file_handler){

        parse_sensor_config_value ( file_handler);
    }
    else {

        printf("Problem with config read, Using default value instead of config");
    }

    sensor_var.freq_of_val_generation = strtoul ( argv[2], NULL, BASE_TEN);
    sensor_var.device_id = strtoul ( argv[3], NULL, BASE_TEN);
    sensor_var.measurement_id = strtoul ( argv[4], NULL, BASE_TEN);

    memset ( &serveraddr, 0, sizeof ( serveraddr));

    // Create a UDP Socket
    sockfd = socket ( AF_INET, SOCK_DGRAM, 0);
    if ( sockfd < 0){
        perror("socket ");
        
        return -1;
    }
    serveraddr.sin_addr.s_addr = inet_addr ( sensor_config_params.server_ip);
    serveraddr.sin_port = htons ( sensor_config_params.server_port);
    serveraddr.sin_family = AF_INET;

    // connect to server
    if ( connect(sockfd, ( SOCKETADDR *)&serveraddr, sizeof ( serveraddr)) < 0) {

        perror("socket connect  ");
        return -1;
    }

    if ( pipe ( pipe_fd) < 0){
        perror("pipe ");
        
        exit(1);
    }
    pthread_t tid;

    if ( pthread_create(&tid, NULL, send_probe_reports, &sockfd) != 0){
        perror ( "thread creat() error ");
        pthread_exit ( NULL);
    }
    produce_and_write_sensor_values ( sockfd, sensor_var);

    if ( pthread_join ( tid, NULL) != 0) {
        perror ( "pthread_join() error");
        pthread_exit ( NULL);
    }

    return 0;
}
