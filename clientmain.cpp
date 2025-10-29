#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
/* You will have to add includes here */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

// Enable if you want debugging to be printed.
// Alternative, pass CFLAGS=-DDEBUG to make
// #define DEBUG

// Included to get the support library
#include <calcLib.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <host:port>\n", argv[0]);
    return 1;
  }

  char delim_address[] = ":";
  char *Desthost = strtok(argv[1], delim_address);
  char *Destport = strtok(NULL, delim_address);

  if (!Desthost || !Destport)
  {
    fprintf(stderr, "ERROR: bad address format, expected host:port\n");
    return 1;
  }

  printf("Host %s, and port %s.\n", Desthost, Destport);

  char buffer[BUFFER_SIZE];
  memset(buffer, 0, sizeof(buffer));

  struct addrinfo hints;
  struct addrinfo *server_addr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  int rv = getaddrinfo(Desthost, Destport, &hints, &server_addr);
  if (rv != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    printf("ERROR: RESOLVE ISSUE\n");
    return 1;
  }

  int internal_socket = socket(server_addr->ai_family, server_addr->ai_socktype, server_addr->ai_protocol);
  if (internal_socket < 0)
  {
    perror("socket");
    freeaddrinfo(server_addr);
    return 2;
  }

  if (connect(internal_socket, server_addr->ai_addr, server_addr->ai_addrlen) < 0)
  {
    perror("connect");
    close(internal_socket);
    freeaddrinfo(server_addr);
    return 3;
  }

  freeaddrinfo(server_addr);

  /* --- Read initial greeting (or whatever the server first sends) --- */
  int bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
  if (bytes_received < 0)
  {
    perror("recv");
    close(internal_socket);
    return 4;
  }
  if (bytes_received == 0)
  {
    printf("Server closed.\n");
    close(internal_socket);
    return 0;
  }
  /* null-terminate safely */
  {
    int used = (bytes_received < (int)sizeof(buffer)) ? bytes_received : (int)sizeof(buffer) - 1;
    buffer[used] = '\0';
  }

#ifdef DEBUG
  printf("BUFFER (first recv, len=%d): ", bytes_received);
  for (int i = 0; i < bytes_received; i++)
  {
    printf("[%02X]", (unsigned char)buffer[i]);
  }
  printf("\n");
#endif

  /* send a short acknowledgment if your protocol expects it (use strlen, not sizeof) */
  char first_message[] = "OK\n";
  int bytes_sent = send(internal_socket, first_message, strlen(first_message), 0);
  if (bytes_sent < 0)
  {
    perror("send OK");
    close(internal_socket);
    return 5;
  }

  /* --- Wait for the actual assignment from server (second recv) --- */
  memset(buffer, 0, sizeof(buffer));
  bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
  if (bytes_received < 0)
  {
    perror("recv");
    close(internal_socket);
    return 6;
  }
  if (bytes_received == 0)
  {
    printf("Server closed.\n");
    close(internal_socket);
    return 0;
  }
  {
    int used = (bytes_received < (int)sizeof(buffer)) ? bytes_received : (int)sizeof(buffer) - 1;
    buffer[used] = '\0';
  }

#ifdef DEBUG
  printf("BUFFER (assignment, len=%d): ", bytes_received);
  for (int i = 0; i < bytes_received; i++)
  {
    printf("[%02X]", (unsigned char)buffer[i]);
  }
  printf("\n");
#endif

  /* strip CR/LF */
  buffer[strcspn(buffer, "\r\n")] = 0;

  /* detect valid operation prefix before parsing */
  if (strncmp(buffer, "add", 3) != 0 &&
      strncmp(buffer, "sub", 3) != 0 &&
      strncmp(buffer, "mul", 3) != 0 &&
      strncmp(buffer, "div", 3) != 0 &&
      strncmp(buffer, "fadd", 4) != 0 &&
      strncmp(buffer, "fsub", 4) != 0 &&
      strncmp(buffer, "fmul", 4) != 0 &&
      strncmp(buffer, "fdiv", 4) != 0)
  {
    printf("ERROR\n");
    close(internal_socket);
    return 1;
  }

  char delim_operation[] = " ";
  char *Operation = strtok(buffer, delim_operation);
  char *First_number = strtok(NULL, delim_operation);
  char *Second_number = strtok(NULL, delim_operation);

  if (!Operation || !First_number || !Second_number)
  {
    printf("ERROR\n");
    close(internal_socket);
    return 1;
  }

  char result_string[64];
  memset(result_string, 0, sizeof(result_string));

  if (Operation[0] == 'f')
  {
    double First_fnumber = strtod(First_number, NULL);
    double Second_fnumber = strtod(Second_number, NULL);
    double fresult = 0.0;

    if (strcmp(Operation, "fadd") == 0)
      fresult = First_fnumber + Second_fnumber;
    else if (strcmp(Operation, "fsub") == 0)
      fresult = First_fnumber - Second_fnumber;
    else if (strcmp(Operation, "fmul") == 0)
      fresult = First_fnumber * Second_fnumber;
    else if (strcmp(Operation, "fdiv") == 0)
      fresult = First_fnumber / Second_fnumber;

    printf("ASSIGNMENT: %s %8.8g %8.8g\n", Operation, First_fnumber, Second_fnumber);

    int r = snprintf(result_string, sizeof(result_string), "%8.8g\n", fresult);
    if (r < 0)
    {
      perror("snprintf");
      close(internal_socket);
      return 7;
    }

    bytes_sent = send(internal_socket, result_string, strlen(result_string), 0);
    if (bytes_sent < 0)
    {
      perror("send");
      close(internal_socket);
      return 8;
    }
  }
  else
  {
    int First_inumber = atoi(First_number);
    int Second_inumber = atoi(Second_number);
    int iresult = 0;

    if (strcmp(Operation, "add") == 0)
      iresult = First_inumber + Second_inumber;
    else if (strcmp(Operation, "sub") == 0)
      iresult = First_inumber - Second_inumber;
    else if (strcmp(Operation, "mul") == 0)
      iresult = First_inumber * Second_inumber;
    else if (strcmp(Operation, "div") == 0)
      iresult = First_inumber / Second_inumber;

    printf("ASSIGNMENT: %s %d %d\n", Operation, First_inumber, Second_inumber);

    int r = snprintf(result_string, sizeof(result_string), "%d\n", iresult);
    if (r < 0)
    {
      perror("snprintf");
      close(internal_socket);
      return 9;
    }

    bytes_sent = send(internal_socket, result_string, strlen(result_string), 0);
    if (bytes_sent < 0)
    {
      perror("send");
      close(internal_socket);
      return 10;
    }
  }

  /* --- final response from server (OK or ERROR) --- */
  memset(buffer, 0, sizeof(buffer));
  int last_bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
  if (last_bytes_received < 0)
  {
    perror("recv");
    close(internal_socket);
    return 11;
  }
  if (last_bytes_received == 0)
  {
    printf("Server closed.\n");
  }
  else
  {
    int used = (last_bytes_received < (int)sizeof(buffer)) ? last_bytes_received : (int)sizeof(buffer) - 1;
    buffer[used] = '\0';
    buffer[strcspn(buffer, "\r\n")] = 0;
    result_string[strcspn(result_string, "\r\n")] = 0;
    printf("%s (myresult=%s)\n", buffer, result_string);
  }

  close(internal_socket);
  return 0;
}
