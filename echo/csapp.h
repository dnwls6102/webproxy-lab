#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
/* Misc constants */
#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
#define LISTENQ  1024  /* Second argument to listen() */
#define RIO_BUFSIZE 8192

typedef struct sockaddr SA;

typedef struct {
    int rio_fd;                /* Descriptor for this internal buf */
    int rio_cnt;               /* Unread bytes in internal buf */
    char *rio_bufptr;          /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;

ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd); 
ssize_t	rio_readnb(rio_t *rp, void *usrbuf, size_t n);
ssize_t	rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

int open_clientfd(char *hostname, char *port);
int open_listenfd(char *port);

#ifdef CSAPP
struct sockaddr_in {
    uint16_t sin_family; /*Protocol family (always AF_INET)*/
    uint16_t sin_port; /*Port number in network byte order*/
    struct in_addr sin_addr; /*IP address in network byte order*/
    unsigned char sin_zero[8]; /*Pad to sizeof(struct sockaddr)*/
};

/*Generic socket address structure (for connect, bind, and accept)*/
struct sockaddr {
    uint16_t sa_family; /*Protocol family*/
    char sa_data[14]; /*address data*/
};

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    char *ai_canonname;
    size_t ai_addrlen; //소켓 주소 구조체의 크기
    struct sockaddr *ai_addr; //소켓 주소 구조체
    struct addrinfo *ai_next; //연결 리스트의 다음 addrinfo 구조체를 가리킴 : 단일 연결 리스트라서 별도로 헤더 및 사이즈 정보를 관리하지 않음
};

int socket(int domain, int type, int protocol);
int connect(int clientfd, const struct sockaddr *addr, socklen_t addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int listenfd, struct sockaddr *addr, int *addrlen);

//반환 성공시 0, 오류 시 양의 정수(오류 코드마다 다름)
//호스트 이름, 호스트 주소, 서비스 이름, 포트번호의 스트링을 소켓 주소 구조체로 변환함
//getaddrinfo의 반환값이 소켓 주소 구조체라는 말 : 마지막 매개변수를 통해 기록한다는 의미
//host : 문자열로 도메인 주소("www.naver.com") 혹은 IP 주소(192.168.1.1)를 받음
//service : 서비스 이름(http 등)이거나 십진수 포트 번호
//hints : 선택적으로 사용할 수 있는 인자. 
//이 hints인자에 들어가는 addrinfo 구조체는 ai_family, ai_socktype, ai_protocol, ai_flags만 설정 가능함. 나머지 멤버는 NULL 또는 0.
//ai_family를 AF_INET으로 설정하면 IPv4주소를 반환, AF_INET6로 설정하면 IPv6 주소로 제한
//ai_socktype을 SOCK_STREAM으로 설정하면 각 고유 주소들에 대해 자신의 소켓 주소가 연결 끝점으로 사용될 수 있는 하나의 addrinfo로 제한
//ai_flags를 AI_ADDRCONFIG로 설정하면 getaddrinfo가 로컬 호스트가 IPv4로 설정되었을 때만 IPv4 주소를 반환하도록 함
//ai_flags를 AI_NUMERICSERV로 설정하면 service 매개변수를 포트번호로 제한함
//ai_flags를 AI_PASSIVE로 설정하면 기존에 connect를 호출할 때 이용 가능한 소켓을 반환하던 getaddrinfo 함수를
//듣기 소켓으로 이용할 수 있는 소켓 주소를 리턴하게 함
int getaddrinfo(const char *host, const char *service, const struct addrinfo *hints, struct addrinfo **result);
void freeaddrinfo(struct addrinfo *result);
//getaddrinfo(gai가 GetAddrInfo의 줄임말)에서 반환한 에러 코드를 메세지(문자열)로 변환해주는 함수
const char *gai_strerror(int errcode);

//getnameinfo: getaddrinfo와 반대되는 기능을 하는 함수
//소켓 주소 구조체를 대응되는 호스트와 서비스 이름 스트링으로 변환함
//sa : 소켓 주소 구조체 포인터
//host : hostlen 바이트 길이의 버퍼
//service : servlen 바이트의 버퍼
//sa를 대응되는 호스트와 서비스 이름 스트링으로 변환한 후, host와 service에 복사함
//flags에 NI_NUMERICHOST를 넣으면 host에서 도메인 이름을 리턴하던 것을 숫자 주소를 리턴하게 함
//flags에 NI_NUMERICSERV를 넣으면 원래 service에서 서비스 이름을 리턴하던 것을 포트 번호를 리턴하게 함
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *service, size_t servlen, int flags);
#endif