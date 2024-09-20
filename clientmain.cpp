#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
/* You will have to add includes here */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
// #define DEBUG

// Included to get the support library
#include <calcLib.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port).
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'.
  */
  char delim_address[] = ":";
  char *Desthost = strtok(argv[1], delim_address);
  char *Destport = strtok(NULL, delim_address);
  // *Desthost now points to a string holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter.

  /* Do magic */
  // int port = atoi(Destport);

  printf("Host %s, and port %s.\n", Desthost, Destport);

  char buffer[BUFFER_SIZE];
  memset(buffer, 0, sizeof(buffer));

  struct addrinfo hints;
  struct addrinfo *server_addr;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  int rv = getaddrinfo(Desthost, Destport, &hints, &server_addr);
  if (rv != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    printf("Invalid address/ Address not supported \n");
    return 1;
  }

  int internal_socket = socket(server_addr->ai_family, server_addr->ai_socktype, server_addr->ai_protocol);
  if (internal_socket < 0)
  {
    printf("Socket could not be created.\n");
    return 2;
  }

  int connection = connect(internal_socket, server_addr->ai_addr, server_addr->ai_addrlen);
  if (connection < 0)
  {
    printf("Could not connect to server.\n");
    return 3;
  }

  freeaddrinfo(server_addr);
  char first_message[] = "OK\n";

  while (1)
  {
    int bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
      printf("No message received.\n");
      close(internal_socket);
      return 4;
    }

    if (bytes_received == 0)
    {
      printf("Server closed.\n");
      close(internal_socket);
      return 0;
    }

    if (bytes_received > 0)
    {
      break;
    }
  }

  int bytes_sent = send(internal_socket, first_message, sizeof(first_message), 0);
  if (bytes_sent < 0)
  {
    printf("No message sent.\n");
    close(internal_socket);
    return 5;
  }

  while (1)
  {
    int bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
      printf("No message received.\n");
      close(internal_socket);
      return 6;
    }

    else if (bytes_received == 0)
    {
      printf("Server closed.\n");
      break;
    }

    else if (bytes_received > 0)
    {
      break;
    }
  }

  char delim_operation[] = " ";
  char *Operation = strtok(buffer, delim_operation);
  char *First_number = strtok(NULL, delim_operation);
  char *Second_number = strtok(NULL, delim_operation);

  char result_string[64];

  if (Operation[0] == 'f')
  {
    double First_fnumber = strtod(First_number, NULL);
    double Second_fnumber = strtod(Second_number, NULL);
    double fresult;

    if (strcmp(Operation, "fadd") == 0)
    {
      fresult = First_fnumber + Second_fnumber;
    }

    else if (strcmp(Operation, "fsub") == 0)
    {
      fresult = First_fnumber - Second_fnumber;
    }

    else if (strcmp(Operation, "fmul") == 0)
    {
      fresult = First_fnumber * Second_fnumber;
    }

    else if (strcmp(Operation, "fdiv") == 0)
    {
      fresult = First_fnumber / Second_fnumber;
    }

    printf("Assignment: %s %8.8g %8.8g\n", buffer, First_fnumber, Second_fnumber);

    int rv = sprintf(result_string, "%.8g\n", fresult);
    if (rv < 0)
    {
      fprintf(stderr, "sprintf float: %s\n", gai_strerror(rv));
      return 7;
    }

    int bytes_sent = send(internal_socket, result_string, strlen(result_string), 0);
    if (bytes_sent < 0)
    {
      printf("No message sent.\n");
      close(internal_socket);
      return 8;
    }
  }

  else
  {
    int First_inumber = atoi(First_number);
    int Second_inumber = atoi(Second_number);
    int iresult;

    if (strcmp(Operation, "add") == 0)
    {
      iresult = First_inumber + Second_inumber;
    }

    else if (strcmp(Operation, "sub") == 0)
    {
      iresult = First_inumber - Second_inumber;
    }

    else if (strcmp(Operation, "mul") == 0)
    {
      iresult = First_inumber * Second_inumber;
    }

    else if (strcmp(Operation, "div") == 0)
    {
      iresult = First_inumber / Second_inumber;
    }

    printf("Assignment: %s %d %d\n", buffer, First_inumber, Second_inumber);

    int rv = sprintf(result_string, "%d\n", iresult);
    if (rv < 0)
    {
      fprintf(stderr, "sprintf int: %s\n", gai_strerror(rv));
      return 9;
    }

    int bytes_sent = send(internal_socket, result_string, strlen(result_string), 0);
    if (bytes_sent < 0)
    {
      printf("No message sent.\n");
      close(internal_socket);
      return 10;
    }
  }

  memset(buffer, 0, sizeof(buffer));

  while (1)
  {
    int last_bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
    if (last_bytes_received < 0)
    {
      printf("No message received.\n");
      close(internal_socket);
      return 11;
    }

    if (last_bytes_received == 0)
    {
      printf("Server closed.\n");
      break;
    }

    if (last_bytes_received > 0)
    {
      printf("%s (myresult=%s)\n", buffer, result_string);
      break;
    }
  }

  close(internal_socket);
  return 0;
}