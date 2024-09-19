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
#define DEBUG

// Included to get the support library
#include <calcLib.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port).
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'.
  */
  char delim[] = ":";
  char *Desthost = strtok(argv[1], delim);
  char *Destport = strtok(NULL, delim);
  // *Desthost now points to a string holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter.

  /* Do magic */
  int port = atoi(Destport);
#ifdef DEBUG
  printf("Host %s, and port %d.\n", Desthost, port);
#endif

  char buffer[BUFFER_SIZE] = {0};

  int internal_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (internal_socket < 0)
  {
    printf("Socket could not be created.\n");
    return 1;
  }

  struct sockaddr_in server_addr;
  // struct hostent *h;
  // h = gethostbyname(Desthost);

  // memset(&server_addr, 0, sizeof(server_addr));
  // server_addr.sin_family = AF_INET;
  // memcpy(&server_addr.sin_addr.s_addr, h->h_addr, h->h_length);
  // server_addr.sin_port = htons(*Destport);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(*Destport);
  if (inet_pton(AF_INET, Desthost, &server_addr.sin_addr) <= 0)
  {
    printf("Invalid address/ Address not supported \n");
    return 2;
  }

  int connection = connect(internal_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (connection < 0)
  {
    printf("Could not connect to server.\n");
    return 3;
  }

  int bytes_received = recv(internal_socket, buffer, sizeof(buffer), 0);
  if (bytes_received < 0)
  {
    printf("No message received.\n");
    return 4;
  }
  printf(buffer, bytes_received);

  close(internal_socket);
  return 0;
}
