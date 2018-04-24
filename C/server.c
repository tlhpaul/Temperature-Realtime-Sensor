/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>


void configure(int);
void parse_message( char* , int );
void update_temperature();
void send_default_page();
void* create_new_thread(void*);
void* shutdownMiddleware(void* );

/* Global var for arduino and client file descriptors */
int arduino_fd;
int client_fd;
pthread_mutex_t lock;

//the char array to be printed in the user interface
char bufToPrint[300];
int stdby = 0 ;//stand by mode no
int check;


// Variables for what the 
int cold_threshold = 23;
int hot_threshold = 26;

// If the fan should be on or not.
// 1 = fan on
// 0 = fan off
int fan_on = 0;

// Whether the arduino should display F or C
// 0 = C
// 1 = F
int show_F = 0;


// Global values for hot/cold/avg value
float most_recent_temp;
float global_high;
float global_low;
float total_readings = 0;
float num_readings = 0;

/*
 *Get the html page
 */
char* get_html() {
  FILE *html_file;
  char *buffer = NULL;
  int string_size, read_size;
  char *filename = "index.html";

  html_file = fopen(filename, "r");

  if (html_file) {
    // Get last byte of file
    fseek(html_file, 0, SEEK_END);

    // Offset from first to last byte (get filesize)
    string_size = ftell(html_file);

    // Return to start of file
    rewind(html_file);

    // Allocate string to hold file contents
    buffer = (char*) malloc(sizeof(char) * (string_size + 1) );

    // Read file contents
    read_size = fread(buffer, sizeof(char), string_size, html_file);

    // Cap with a null
    buffer[string_size] = '\0';

    if (string_size != read_size) {
      free(buffer);
      buffer = NULL;
      printf("ERROR: Failed to read full file '%s'\n", filename);
    }

    // Close file
    fclose(html_file);
  }
  else {
    printf("ERROR: Could not locate file '%s'\n", filename);
    exit(1);
  }

  return buffer;
}


/*
* Start server
*/
void* start_server(void* p) {

  // structs to represent the server and client
  struct sockaddr_in server_addr,client_addr;    

  // socket descriptor      
  int sock;

  // 1. socket: creates a socket descriptor that you later use to make other system calls
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Socket");
    exit(1);
  }

  int temp;
  if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
    perror("Setsockopt");
    exit(1);
  }

  // configure the server
  int PORT_NUMBER = *(int*) p;
  server_addr.sin_port = htons(PORT_NUMBER); // specify port number
  server_addr.sin_family = AF_INET;         
  server_addr.sin_addr.s_addr = INADDR_ANY; 
  bzero(&(server_addr.sin_zero),8); 
      
  // 2. bind: use the socket and associate it with the port number
  if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
    perror("Unable to bind");
    exit(1);
  }

  // 3. listen: indicates that we want to listen to the port to which we bound; second arg is number of allowed connections
  if (listen(sock, 1) == -1) {
    perror("Listen");
    exit(1);
  }
  
  // once you get here, the server is set up and about to start listening
  printf("[+] Server configured to listen on port %d\n", PORT_NUMBER);
  fflush(stdout);


  // 4. accept: wait here until we get a connection on that port
  printf("[+] Server waiting for request\n");
  int sin_size = sizeof(struct sockaddr_in);
  int thread_num = 0;
  while(1) {

    client_fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);

    //Thread pool to handle multithreading
    pthread_t thread_pool[100];
    if (client_fd != -1) {
      printf("[+] Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
      printf("[+] Accepted request from client. FD: %d\n", client_fd);

      // 5. recv: create new thread to handle new incoming message (request) into buffer
      printf("thread num: %d\n", thread_num);
      int ret_val;
      ret_val = pthread_create(&thread_pool[thread_num%100], NULL, &create_new_thread, &client_fd);
      if (ret_val == 1) {
        printf("[-] Issue starting new thread to accept request\n");
        return 0;
      }
      //increment thread number by one every time
      thread_num++;
    }

  }
  return p;
} 

/*
* Check the disctionnection every 5 seconds.
*/
void* checkDisconnected(void* path){
  while (1){
    check = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    printf("%d\n", check);
    sleep(5);
  }
  return path;
}

/*
* Create new thread when new request arrives. 
*/
void* create_new_thread(void* p){
  printf("%s\n", "new thread created");
  int* client = (int*) p;
  char request[1024];
  int bytes_received = recv(*client,request,1024,0);
  request[bytes_received] = '\0';
  pthread_mutex_lock(&lock);
  parse_message( request, *client);
  pthread_mutex_unlock(&lock);
  return p;
}

/*
* Start arduino connection
*/ 
void* start_arduino(void* path) {

  // get the name from the command line
  char* file_name = (char*)path;
  
  // try to open the file for reading and writing
  // you may need to change the flags depending on your platform
  arduino_fd = 0;
  arduino_fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
  
  if (arduino_fd < 0) {
    return path;
  }

  configure(arduino_fd);

  // successfully connected 
  printf("[+] Successfully connected to arduino at '%s'\n", file_name);


  /*
    Write the rest of the program below, using the read and write system calls.
  */
  int bytes_read = 0;
  char buf[300];
  int loc = 0;
  check = 1;

  //create a new thread to check connection periodically 
  pthread_t check_thread;
  int ret_val = pthread_create(&check_thread, NULL, &checkDisconnected, path);

  while(1){

    bytes_read = read( arduino_fd, &buf[loc], 100);
    if (bytes_read < 0) continue;

    loc += bytes_read;
    bytes_read = 0;

    // If there is a \n in the buffer
    for( int ii = 0; ii < 100; ii++){
      if( buf[ii] == '\n' && ii != 0 ){

        // Update temperature values based on new reading
        update_temperature();
        //Copy buffer to global bufToPrint array
        strcpy(bufToPrint, buf);
        loc = 0;
        for( int jj = 0; jj < 100; jj++ ){
          buf[jj] = '\0';
        }
      }
    }
  }
  return path;
}


/*
 * Loads html for default page, sends corresponding reply to client
 */
void send_default_page(int client) {

  // load html content, write header
  char* html = get_html();
  int html_size = strlen(html);
  char header[100];
  sprintf(header, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n", html_size);

  // construct reply
  char reply[html_size + 100];
  strcpy(reply, header);
  strcat(reply, html);
  send(client, reply, strlen(reply), 0);
  free(html);
}

/*
* For error handling
*/
void file_not_found(int client) {
  char* html = "404 Not Found";
  int html_size = strlen(html);
  char header[100];
  sprintf(header, "HTTP/1.1 404 Not Found\nContent-Type: text/html\nContent-Length: %d\n\n", html_size);

  // construct reply
  char reply[html_size + 100];
  strcpy(reply, header);
  strcat(reply, html);
  send(client, reply, strlen(reply), 0);
}

/*
 * Sends response to client with most recent temperature
 */
void send_temp_update(int client) {
  // get temperature units
  char degree[50];
  if (show_F == 0) {
    strcpy(degree, "degrees C");
  }
  else {
    strcpy(degree, "degrees F");
  }

  char temperature[50];
  sprintf(temperature, "%f %s i.", most_recent_temp, degree);
  int size = strlen(temperature);

  char header[100];
  sprintf(header, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n", size - 1);

  // this is the message that we'll send back
  char reply[strlen(header) + size];
  strcpy(reply, header);
  strcat(reply, temperature);

  send(client, reply, strlen(reply), 0);

}

/*
* Arduino is disconnected
*/
void disconnected(int client) {
  char* html = "Arduino is disconnected";
  int html_size = strlen(html);
  char header[100];
  sprintf(header, "HTTP/1.1 200 Not Found\nContent-Type: text/html\nContent-Length: %d\n\n", html_size);

  // construct reply
  char reply[html_size + 100];
  strcpy(reply, header);
  strcat(reply, html);
  send(client, reply, strlen(reply), 0);
}

void convert_temperature(int f) {

  // if f = 0, convert to C
  if (f == 0) {
    printf("Converting to degrees in C\n");
    show_F = 0;
    char arduino_signal[5];
    strcpy(arduino_signal, "CCCC");
    write( arduino_fd, arduino_signal, 4);
  }

  // if f = 1, convert to F
  else if (f == 1) {
    printf("Converting to degrees in F\n");
    show_F = 1;
    char arduino_signal[5];
    strcpy(arduino_signal, "FFFF");
    write( arduino_fd, arduino_signal, 4);
  }
}

/*
* Parses the response from browser
*/
void parse_message( char* request, int client){

  if (arduino_fd < 0) {
    disconnected(client);
    return;
  }

  // if request is null, load main page by default
  if (request[0] == '\0') {
    send_default_page(client);
    return;
  }

  // Gets first token
  char* tok = strtok( request, " " );

  // GET request
  if( strcmp(tok, "GET" )== 0 ){
    tok = strtok( NULL, " " );

    printf("tok = %s\n", tok);

    // if first time client loads, send default page
    if (strcmp(tok, "/") == 0){
      convert_temperature(show_F);
      send_default_page(client);
      return;
    }

    //testing
    else if (strcmp(tok, "/sleep") == 0){
      printf("%s\n", "Thread is sleeping");
      sleep(5);

      file_not_found(client);
      return;
    }

    // updating most_recent_temp
    else if (strcmp(tok, "/update") == 0){
      convert_temperature(show_F);
      send_temp_update(client);
      return;
    }

    // setting thresholds
    else if (strncmp(tok, "/action?", 8) == 0) {
    	char ghetto_string[60];
    	char cold_threshold[4];
      	char hot_threshold[4];
    	strcpy( ghetto_string, tok );

    	printf("threshold = %s\n", tok);

    	strtok( ghetto_string, "=");
    	tok = strtok( NULL, "&");
    	strcpy(hot_threshold, tok);
    	printf("hot=%s\n", hot_threshold);

    	strtok( NULL, "=");
    	tok = strtok( NULL, " ");
    	strcpy(cold_threshold, tok);
    	printf("hot=%s\n", tok);
      char arduino_signal[30];
      strcpy(arduino_signal, "THRESHOLD");
      strcat(arduino_signal, " ");
      strcat(arduino_signal, cold_threshold);
      strcat(arduino_signal, " ");
      strcat(arduino_signal, hot_threshold);
      write( arduino_fd, arduino_signal, strlen(arduino_signal));
      printf("Wrote out:%s\n");
    }
    else file_not_found(client);
  }

  // POST request
  else if( strcmp(tok, "POST" )== 0 ){

    tok = strtok( NULL, " " );
    // Switch units buttons
    if (strcmp(tok, "/action?disp=F") == 0){
      convert_temperature(1);
    }
    else if (strcmp(tok, "/action?disp=C") == 0){
      convert_temperature(0);
    }

    // Activate fan button
    else if (strcmp(tok, "/action?fan=on") == 0){ 
      printf("turning on fan\n");
      fan_on = 1;
      char arduino_signal[10];
      strcpy(arduino_signal, "FAN=ON");
      write( arduino_fd, arduino_signal, strlen(arduino_signal));
    }
    else if ( strcmp(tok, "/action?fan=off") == 0 ){ 
      fan_on = 0;
      char arduino_signal[10];
      strcpy(arduino_signal, "FAN=OFF");
      write( arduino_fd, arduino_signal, strlen(arduino_signal));
      printf("here\n");
    }
    // Standby mode button
    else if (strcmp(tok, "/action?stdby=on") == 0){ 
      stdby = 1;
    }
    else if (strcmp(tok, "/action?stdby=off") == 0){ 
      stdby = 0;
    }
    else {
      printf("File not found\n");
      file_not_found(client);
    }
  }
}

/*
 * If new reading from arduino updates:
 * - most_recent_temp
 * - global_high
 * - global_low
 * - total_readings
 * - num_readings
 */
void update_temperature() {

  // Parse bufToPrint to get temeperature
  char* tok = strtok(bufToPrint, " ");
  tok = strtok(NULL, " ");
  tok = strtok(NULL, " ");
  tok = strtok(NULL, " ");

  // If there's a new temperature
  if (tok != NULL) {
    most_recent_temp = atof(tok);
    total_readings += most_recent_temp;
    num_readings++;

    // if this is the first reading
    if (num_readings <= 2) {
      global_high = most_recent_temp;
      global_low = most_recent_temp;
    }

    // Update high/low
    if (global_high < most_recent_temp) {
      global_high = most_recent_temp;
    }
    if (global_low > most_recent_temp) {
      global_low = most_recent_temp;
    }
  }
}

/**
* Shut down middleware when user enters "q"
*/
void* shutdownMiddleware(void* p){
  while (1){
    char str[100];
    fgets(str, 100, stdin);
    if (strcmp(str, "q\n") == 0) {
      break;
    }
  }
  exit(1);
  return p;
}


int main(int argc, char *argv[])
{

  // check the number of arguments
  if (argc != 3) {
      printf("Usage: %s [port_number] [device_file]\n", argv[0]);
      exit(-1);
  }

  int port_number = atoi(argv[1]);
  if (port_number <= 1024) {
    printf("Please specify a port number greater than 1024\n");
    exit(-1);
  }

  //create threads for server, arduino and shutdown
  pthread_t arduino_thread;
  pthread_t server_thread;
  pthread_t shutdown_thread;
  int ret_val;

  // connect to arduino
  ret_val = pthread_create(&arduino_thread, NULL, &start_arduino, argv[2]);
  if (ret_val == 1) {
    printf("[-] Issue connecting to arduino\n");
    return 0;
  }

  // initialize server
  ret_val = pthread_create(&server_thread, NULL, &start_server, &port_number);
  if (ret_val == 1) {
    printf("[-] Issue starting server\n");
    return 0;
  }

  //Shut down the server
  ret_val = pthread_create(&shutdown_thread, NULL, &shutdownMiddleware, NULL);
  if (ret_val == 1) {
    printf("[-] Issue shutting down thread \n");
    return 0;
  }


  //If check is smaller than 0, then create a new thread to restart arduino 
  while (1){
    if (check < 0) {
      printf("create new ardunio\n");
      ret_val = pthread_create(&arduino_thread, NULL, &start_arduino, argv[2]);
      if (ret_val == 1) {
        printf("[-] Issue connecting to arduino\n");
      } 
      sleep(5);
    } 
  }

  // wait on threads to complete
  void* arduino_ret;
  void* server_ret;
  void* shutdown_ret;
  ret_val = pthread_join(arduino_thread, &arduino_ret);
  if (ret_val == 1) return 0;
  ret_val = pthread_join(server_thread, &server_ret);
  if (ret_val == 1) return 0;
  ret_val = pthread_join(shutdown_thread, &shutdown_ret);
  if (ret_val == 1) return 0;

  return 1;

}

