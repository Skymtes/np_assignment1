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

using namespace std;

/* Read a line terminated by '\n' with a total timeout (seconds).
   returns >0 bytes read (including '\n'), 0 timeout, -1 error/closed.
   The returned std::string includes the terminating '\n' if present. */
static ssize_t recv_line_with_timeout(int fd, string &out, int timeout_seconds)
{
    out.clear();
    fd_set rset;
    struct timeval tv;
    char c;
    ssize_t n;
    time_t start = time(NULL);

    while (true)
    {
        time_t now = time(NULL);
        int elapsed = (int)(now - start);
        int remain = timeout_seconds - elapsed;
        if (remain <= 0)
            return 0; // timeout

        FD_ZERO(&rset);
        FD_SET(fd, &rset);
        tv.tv_sec = remain;
        tv.tv_usec = 0;
        int rv = select(fd + 1, &rset, NULL, NULL, &tv);
        if (rv > 0)
        {
            n = recv(fd, &c, 1, 0);
            if (n < 0)
            {
                return -1;
            }
            out.push_back(c);
            if (c == '\n')
                break;
        }
    }
    return (ssize_t)out.size();
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s host:port\n", argv[0]);
        return 1;
    }

    initCalcLib();

    char delim_address[] = ":";
    char *Desthost = strtok(argv[1], delim_address);
    char *Destport = strtok(NULL, delim_address);
    if (!Desthost || !Destport)
    {
        fprintf(stderr, "ERROR: bad address format, expected host:port\n");
        return 1;
    }

    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(Desthost, Destport, &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return 1;
    }

    int listen_fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next)
    {
        if ((listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) < 0)
        {
            perror("server: socket");
            continue;
        }
        int yes = 1;
        if ((setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) < 0)
        {
            perror("setsockopt");
            exit(1);
        }
        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) < 0)
        {
            close(listen_fd);
            perror("server: bind");
            continue;
        }
    }

    freeaddrinfo(res);

    if (listen(listen_fd, 5) < 0)
    {
        perror("listen");
        close(listen_fd);
        return 1;
    }

#ifdef DEBUG
    printf("Server listening on %s:%s (backlog=5)\n", argv[1], Destport);
#endif

    while (1)
    {
        struct sockaddr_storage cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&cliaddr, &clilen);
        if (client_fd < 0)
        {
            perror("accept");
            break;
        }

        char hostbuf[NI_MAXHOST], portbuf[NI_MAXSERV];
        if (getnameinfo((struct sockaddr *)&cliaddr, clilen, hostbuf, sizeof(hostbuf),
                        portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
        {
#ifdef DEBUG
            printf("Accepted connection from %s:%s\n", hostbuf, portbuf);
#endif
        }
        else
        {
#ifdef DEBUG
            printf("Accepted connection (address unknown)\n");
#endif
        }

        /* send protocol */
        const char *proto = "TEXT TCP 1.0\n\n";
        if (send(client_fd, proto, strlen(proto), 0) < 0)
        {
            perror("send");
            close(client_fd);
            continue;
        }

        /* wait for client "OK\n" (5s) */
        string line;
        ssize_t got = recv_line_with_timeout(client_fd, line, 5);
        if (got <= 0)
        {
            const char *to = "ERROR TO\n";
            send(client_fd, to, strlen(to), 0);
            close(client_fd);
            printf("Client did not accept protocol in time -> closed\n");
            continue;
        }
        if (!(line.size() >= 2 && line[0] == 'O' && line[1] == 'K'))
        {
            printf("Client did not accept protocol (got '%s'), closing\n", line.c_str());
            close(client_fd);
            continue;
        }

        /* generate assignment */
        char *op = randomType();
        int iv1 = 0, iv2 = 1;
        double fv1 = 0.0, fv2 = 1.0;
        if (op[0] == 'f')
        {
            fv1 = randomFloat();
            fv2 = randomFloat();
            if (strcmp(op, "fdiv") == 0 && fv2 == 0.0)
                fv2 = 1.0;
        }
        else
        {
            iv1 = randomInt();
            iv2 = randomInt();
            if (strcmp(op, "div") == 0 && iv2 == 0)
                iv2 = 1;
        }

        char msg[256];
        if (op[0] == 'f')
        {
            snprintf(msg, sizeof(msg), "%s %8.8g %8.8g\n", op, fv1, fv2);
        }
        else
        {
            snprintf(msg, sizeof(msg), "%s %d %d\n", op, iv1, iv2);
        }

        if (send(client_fd, msg, strlen(msg), 0) < 0)
        {
            perror("send assignment");
            close(client_fd);
            continue;
        }

#ifdef DEBUG
        printf("ASSIGNMENT SENT: %s", msg);
#endif

        /* wait for client's solution (5s) */
        string reply;
        ssize_t r = recv_line_with_timeout(client_fd, reply, 5);
        if (r == 0)
        {
            const char *to = "ERROR TO\n";
            send(client_fd, to, strlen(to), 0);
            close(client_fd);
            printf("Client timed out solving assignment -> sent ERROR TO and closed\n");
            continue;
        }
        else if (r < 0)
        {
            close(client_fd);
            printf("Error while reading client reply\n");
            continue;
        }

#ifdef DEBUG
        printf("Client reply: '%s'\n", reply.c_str());
#endif

        bool correct = false;
        const char *cstr = reply.c_str();

        if (op[0] == 'f')
        {
            double clientValue = strtod(cstr, NULL); // stray newline is fine
            double ref = 0.0;
            if (strcmp(op, "fadd") == 0)
                ref = fv1 + fv2;
            else if (strcmp(op, "fsub") == 0)
                ref = fv1 - fv2;
            else if (strcmp(op, "fmul") == 0)
                ref = fv1 * fv2;
            else if (strcmp(op, "fdiv") == 0)
                ref = fv1 / fv2;
            float precision = 0.0001;
            if (((ref - precision) < clientValue) &&
                ((ref + precision) > clientValue))
                correct = true;
            else
                correct = false;

#ifdef DEBUG
            printf("REF = %8.8g, CLIENT = %8.8g\n", ref, clientValue);
#endif
        }
        else
        {
            long val = strtol(cstr, NULL, 10);
            long ref = 0;
            if (strcmp(op, "add") == 0)
                ref = (long)iv1 + (long)iv2;
            else if (strcmp(op, "sub") == 0)
                ref = (long)iv1 - (long)iv2;
            else if (strcmp(op, "mul") == 0)
                ref = (long)iv1 * (long)iv2;
            else if (strcmp(op, "div") == 0)
                ref = (long)(iv1 / iv2); // integer division
            if (val == ref)
                correct = true;

#ifdef DEBUG
            printf("REF = %ld, CLIENT = %ld\n", ref, val);
#endif
        }

        if (correct)
        {
            send(client_fd, "OK\n", 3, 0);

#ifdef DEBUG
            printf("Sent OK to client\n");
#endif
        }
        else
        {
            send(client_fd, "ERROR\n", 6, 0);
            printf("Sent ERROR to client\n");
        }

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
