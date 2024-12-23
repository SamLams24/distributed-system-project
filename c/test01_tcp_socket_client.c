#ifdef WIN32 /* Windows */

#include <winsock2.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /*fermer */
#include <netdb.h> /* gethostbyname*/
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

static void init(void) {
    #ifdef WIN32
        WSADATA wsa;
        int err = WSAStartup(MAKEWORD(2, 2) , &wsa);
        if(err < 0) {
            puts("WSAStartup failed !")
            exit(EXIT_FAILURE);
        }

    #endif    
}

static void end(void) {
    #ifdef WIN32
        WSACleanup();

    #endif    
}

SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
if(sock == INVALID_SOCKET) {

    perror("socket()");
    exit(errno);
}

stuct hostent *hostinfo = NULL;
SOCKADDR_IN sin = { 0 }; //initilisation de la structure avec des zeros
const char *hostname = "www.developpez.com";
char buffer[1024];
hostinfo = gethostbyname(hostname);

if(hostinfo == NULL) {
    fprintf(stderr ,"Unknown host %s.\n", hostname);
    exit(EXIT_FAILURE);
}

if(send(sock, buffer, strlen(buffer), 0) < 0)
{
    perror("send()");
    exit(errno);
}

sin.sin_addr = *(IN_ADDR *) hostinfo -> h_addr; // Adrese dans le champ h_addr de la struct hostinfo
sin.sin_port = htons(PORT); // htons pour le port
sin.sin_family = AF_INET;

if(connect(sock, ( SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR) {
    perror("connect()");!
    exit(errno);
}

closesocket(sock);