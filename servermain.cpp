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

#include <calcLib.h>

using namespace std;

/* Read a line terminated by '\n' with a total timeout of timeout_seconds.
   Returns:
    >0 : bytes read (including '\n')
     0 : timeout
    -1 : error / connection closed
*/
static ssize_t recv_line_with_timeout(int fd, string &out, int timeout_seconds) {
    out.clear();
    fd_set rset;
    struct timeval tv;
    char c;
    ssize_t n;
    time_t start = time(NULL);

    while (true) {
        time_t now = time(NULL);
        int elapsed = (int)(now - start);
        int remain = timeout_seconds - elapsed;
        if (remain <= 0) return 0; // timeout

        FD_ZERO(&rset);
        FD_SET(fd, &rset);
        tv.tv_sec = remain;
        tv.tv_usec = 0;
        int rv = select(fd + 1, &rset, NULL, NULL, &tv);
        if (rv < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (rv == 0) {
            return 0; // timeout
        } else {
            n = recv(fd, &c, 1, 0);
            if (n == 0) return -1; // closed
            if (n < 0) {
                if (errno == EINTR) continue;
                return -1;
            }
            out.push_back(c);
            if (c == '\n') break;
        }
    }
    return (ssize_t)out.size();
}

/* trim */
static inline string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s host:port\n", argv[0]);
        return 1;
    }

    initCalcLib();

    char delim_address[] = ":";
    char *Desthost = strtok(argv[1], delim_address);
    char *Destport = strtok(NULL, delim_address);

    if (!Desthost || !Destport) {
        fprintf(stderr, "ERROR: bad address format, expected host:port\n");
        return 1;
    }

    struct addrinfo hints, *res, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(Desthost, Destport, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int listen_fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd < 0) continue;
        int yes = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* bound */
        }
        close(listen_fd);
        listen_fd = -1;
    }

    if (listen_fd < 0) {
        fprintf(stderr, "Could not bind to %s:%s\n", Desthost, Destport);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    /* backlog 5: five clients may queue, sixth will be rejected by OS */
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("Server listening on %s:%s (backlog=5)\n", argv[1], Destport);

    /* main accept loop - one client at a time */
    while (1) {
        struct sockaddr_storage cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&cliaddr, &clilen);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        /* log client */
        char hostbuf[NI_MAXHOST], portbuf[NI_MAXSERV];
        if (getnameinfo((struct sockaddr*)&cliaddr, clilen, hostbuf, sizeof(hostbuf),
                        portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
            printf("Accepted connection from %s:%s\n", hostbuf, portbuf);
        } else {
            printf("Accepted connection (address unknown)\n");
        }

        /* send protocol advertisement "TEXT TCP 1.0\n" + extra '\n' */
        const char *protoAd = "TEXT TCP 1.0\n\n";
        if (send(client_fd, protoAd, strlen(protoAd), 0) < 0) {
            perror("send");
            close(client_fd);
            continue;
        }

        /* wait for client's acceptance ("OK\n") with 5s timeout */
        string line;
        ssize_t rv = recv_line_with_timeout(client_fd, line, 5);
        if (rv <= 0) {
            const char *to = "ERROR TO\n";
            send(client_fd, to, strlen(to), 0);
            close(client_fd);
            printf("Client did not accept protocol in time -> closed\n");
            continue;
        }
        string clientResp = trim(line);
        if (clientResp != "OK") {
            printf("Client did not accept protocol (got '%s'), closing\n", clientResp.c_str());
            close(client_fd);
            continue;
        }

        /* generate assignment using calcLib */
        char *op = randomType();
        int iv1=0, iv2=1;
        double fv1=0.0, fv2=1.0;

        if (op[0] == 'f') {
            fv1 = randomFloat();
            fv2 = randomFloat();
            /* avoid exact zero for division */
            if (strcmp(op, "fdiv") == 0 && fv2 == 0.0) fv2 = 1.0;
        } else {
            iv1 = randomInt();
            iv2 = randomInt();
            if (strcmp(op, "div") == 0 && iv2 == 0) iv2 = 1;
        }

        char msg[256];
        memset(msg, 0, sizeof(msg));
        if (op[0] == 'f') {
            snprintf(msg, sizeof(msg), "%s %8.8g %8.8g\n", op, fv1, fv2);
        } else {
            snprintf(msg, sizeof(msg), "%s %d %d\n", op, iv1, iv2);
        }

        /* send assignment */
        if (send(client_fd, msg, strlen(msg), 0) < 0) {
            perror("send assignment");
            close(client_fd);
            continue;
        }
        printf("ASSIGNMENT SENT: %s", msg);

        /* wait for client's solution with 5s timeout */
        string replyLine;
        ssize_t got = recv_line_with_timeout(client_fd, replyLine, 5);
        if (got == 0) {
            /* timeout */
            const char *to = "ERROR TO\n";
            send(client_fd, to, strlen(to), 0);
            close(client_fd);
            printf("Client timed out solving assignment -> sent ERROR TO and closed\n");
            continue;
        } else if (got < 0) {
            close(client_fd);
            printf("Error while reading client reply\n");
            continue;
        }

        string replyTrim = trim(replyLine);
        printf("Client reply: '%s'\n", replyTrim.c_str());

        bool correct = false;

        /* Compare results */
        if (op[0] == 'f') {
            /* floating point expected - parse double */
            char *endptr = NULL;
            const char *cstr = replyTrim.c_str();
            double clientValue = strtod(cstr, &endptr);
            if (endptr == cstr) {
                correct = false;
            } else {
                double ref = 0.0;
                if (strcmp(op, "fadd") == 0) ref = fv1 + fv2;
                else if (strcmp(op, "fsub") == 0) ref = fv1 - fv2;
                else if (strcmp(op, "fmul") == 0) ref = fv1 * fv2;
                else if (strcmp(op, "fdiv") == 0) ref = fv1 / fv2;

                float precision = 0.0001;
                if (((ref - precision) < clientValue) && 
                    ((ref + precision) > clientValue))
                    correct = true;
                else
                  correct = false;
                  
                printf("REF = %8.8g, CLIENT = %8.8g", ref, clientValue);
            }
        } else {
            /* integer expected */
            char *endptr = NULL;
            const char *cstr = replyTrim.c_str();
            long val = strtol(cstr, &endptr, 10);
            if (endptr == cstr) {
                correct = false;
            } else {
                long ref = 0;
                if (strcmp(op, "add") == 0) ref = (long)iv1 + (long)iv2;
                else if (strcmp(op, "sub") == 0) ref = (long)iv1 - (long)iv2;
                else if (strcmp(op, "mul") == 0) ref = (long)iv1 * (long)iv2;
                else if (strcmp(op, "div") == 0) ref = (long)(iv1 / iv2); 
                if (val == ref) correct = true;
                else correct = false;
                printf("REF = %ld, CLIENT = %ld\n", ref, val);
            }
        }

        const char *okmsg = "OK\n";
        const char *errormsg = "ERROR\n";
        if (correct) {
            send(client_fd, okmsg, strlen(okmsg), 0);
            printf("Sent OK to client\n");
        } else {
            send(client_fd, errormsg, strlen(errormsg), 0);
            printf("Sent ERROR to client\n");
        }

        close(client_fd);
        /* loop back to accept next client */
    }

    close(listen_fd);
    return 0;
}
