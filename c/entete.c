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