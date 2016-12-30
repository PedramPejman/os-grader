#include "server.h"

const char RCV_AOK[] = "RCV_AOK";

/*
** Parses the header of the request and returns an array of tokens
*/
int parse_arguments(char **args, char *line) {
  int i = 0;
  args[i] = strtok(line, ARG_DELIM);
  while ((args[++i] = strtok(NULL, ARG_DELIM)) != NULL);
  return i - 1;
}

/*
** Fills newfile buffer with new filename of format <user>_<ass_num>_XXXXX.c
** Returns a pointer to newfile
*/
char *generate_filename(char *newfile, char *user, char *ass_num) {
  int k=0;
  
  // Add username
  for (int i=0;i<strlen(user);) { 
    newfile[k++] = user[i++];
  }

  newfile[k++] = '_';

  // Add assignment number
  for (int i=0; i<strlen(ass_num);) {
    newfile[k++] = ass_num[i++];
  }

  newfile[k++] = '_';

  // Seed random generator and produce "random" 5 digit number
  time_t t;
  srand((unsigned) time(&t)); 
  int r = rand() % 10000;

  // Add random number to filename
  sprintf(newfile + k, "%05d.c", r);

  return newfile;
}

/*
** Sets up listen queue socket and returns its file descriptor
*/
int set_up_server() {
  
  // Create socket to listen for incoming connections
  int listen_queue_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_queue_socket < 0) {
    printf("Cannot create socket\n");
    close(listen_queue_socket);
    return 1;
  }
  printf("Created socket\n");

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);

  // Bind the socket to server_address
  if (bind(listen_queue_socket, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    printf("Cannot bind socket\n");
    close(listen_queue_socket);
    return 1;
  }
  printf("Bound socket\n\n");
  return listen_queue_socket;
}

/*
** Blocks and waits for a client to make a request
** Upon receipt, moves the connection to a new socket and returns its fd
*/
int listen_for_requests(int listen_queue_socket) {
  int connection_socket;
  socklen_t client_address_len;
  struct sockaddr_in client_address; 
  
  // Listen for new connections
  printf("Listening for connections on port %d...\n", PORT);
  if (listen(listen_queue_socket, MAX_WAITING_CONNECTIONS) < 0) {
    printf("Could not listen on port %d - Exiting\n", PORT);
    exit(EXIT_FAILURE);
  }

  // Accept new connections, move them to connection_socket
  client_address_len = sizeof(client_address);
  connection_socket =
      accept(listen_queue_socket, (struct sockaddr *)&client_address,
             &client_address_len);

  return connection_socket;
}

/* 
** Actually does the job of receiving the request data from the client
** Returns 0 for no errors and 1 if any errors occur.
*/
int receive_request(Request *request) {

  int connection_socket = request->socket_fd;

  // Make sure connection socket descriptor is valid
  if (connection_socket < 0) {
    return 1;
  }

  printf("Valid connection has been established - retrieving header\n");

  // Receive the header_tokens and write the file
  char receive_buffer[TCP_PACKET_SIZE];
  if (recv(connection_socket, receive_buffer, sizeof(receive_buffer), 0)) {
    
    char *header_tokens[TCP_PACKET_SIZE];
    char new_filename[MAX_FILENAME_LEN];
    int bytes_remaining, bytes_received;

    // Make sure the first packet is the header data
    if (!strncmp(receive_buffer, HEADER_PREFIX, strlen(HEADER_PREFIX))) {

      // Parse the header and capture data
      parse_arguments(header_tokens, receive_buffer);
      strcpy(request->user, header_tokens[1]);
      strcpy(request->ass_num, header_tokens[2]);
      bytes_remaining = atoi(header_tokens[3]);
      strcpy(request->filename, generate_filename(&new_filename, request->user, request->ass_num));
    
    } else {
      // Print error message and exit
      printf("Failed to received header - first packet content: [%s]\n",
             receive_buffer);
      close(connection_socket);
      return 1;
    }

    printf("Retreived and parsed header information\n");

    // Compute path of file to write
    char filepath[MAX_FILENAME_LEN];
    strcpy(&filepath, TEMP);
    strcpy(&filepath[strlen(TEMP)], request->filename);

    // Open file to write to and
    FILE *file_to_write = open(filepath, O_WRONLY | O_TRUNC | O_CREAT, 00664);

    printf("Opened file to write to\n");

    // Listen until file is received or no more packets are being sent
    while ((bytes_remaining > 0) && 
        ((bytes_received = recv(connection_socket, receive_buffer, TCP_PACKET_SIZE, 0)) > 0)) {

      // Write the received packet of data to the file
      write(file_to_write, receive_buffer, bytes_received);

      bytes_remaining -= bytes_received;
    }

    // Make sure we received the number of bytes we expected
    if (!bytes_remaining) {
      printf("Succesfully retrieved file\n\n");
    } else {
      printf("Warning - Expected %d bytes, received %d bytes\n\n");
    }

    close(file_to_write);

    // Send ack to client
    send(connection_socket, RCV_AOK, strlen(RCV_AOK), 0);

    // Close client connection
    close(connection_socket);
  } else {
    printf("Client dropped connection\n\n");
  }
}


int prev_main() { 

  // Set up initial socket for listen queue
  int listen_queue_socket = set_up_server();

  while (1) {
    
    // Block and listen for connections - upon receipt, move connection to new socket
    int connection_socket = listen_for_requests(listen_queue_socket);

    Request request;
    request.socket_fd = connection_socket;

    // Let the handler receive request data
    // TODO: Move this to a new thread
    receive_request(&request);
  }
}