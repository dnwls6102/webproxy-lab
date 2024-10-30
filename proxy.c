#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

//캐시 구조체
typedef struct {
    char *url; //캐싱한 페이지의 url
    char *response_data; //응답한 내용
    size_t size;
    time_t last_accessed; // 최근 접속한 시각
} cache_entry;

//캐시 저장공간 할당
cache_entry cache[MAX_CACHE_SIZE];

void proxy_connect(int fd);
int parse_uri(char *uri, char *host, char *port, char* uri_ptos);
int request_to_server(int connfd, char *host, char *port, char *uri_ptos);
char *find_in_cache(char *uri_ptos); //캐시에서 데이터 찾아내는 함수
void store_in_cache(char *uri_ptos, char *response_data, size_t size); //캐시에 데이터 저장하는 함수

int main(int argc, char**argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
      printf(stderr, "usage: %s <port>\n", argv[0]);
      exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while(1)
  {
      clientlen = sizeof(clientaddr);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      proxy_connect(connfd);
      Close(connfd);

  }

  return 0;
}

void proxy_connect(int connfd)
{
    int clientfd;
    char buf[MAXLINE], host[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio;
    char uri_ptos[MAXLINE];

    Rio_readinitb(&rio, connfd);

    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("Request headers to proxy: ");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    parse_uri(uri, host, port, uri_ptos);
    request_to_server(connfd, host, port, uri_ptos);
    
}

int parse_uri(char* uri, char* host, char* port, char* uri_ptos)
{
    char *ptr;

    if(!strstr(uri, "http://")) //클라이언트에서 정상적인 주소를 요청하지 않았다면
        return -1; //FAIL 반환
    
    //ptr에 uri 시작 주소 대입
    ptr = uri;
    //http:// 건너뛰기
    ptr += 7;
    //일단 host에 ptr(=http://가 빠진 uri)을 복사시켜 놓기(: 부분을 \0으로 처리해서 자연스럽게 분리시킬거임)
    strcpy(host, ptr);

    //서버로 보낼 uri 주소 추출하기
    if (ptr = strchr(host, '/'))
    {
        if (host[strlen(host) - 1] == '/')//만약 별도의 경로가 없다면(예: http://www.google.com:80/)
        {
            strcpy(uri_ptos, "/");//uri_ptos에 '/' 값이 저장됨
            *ptr = '\0';
        }
        else
        {
            //포트번호 다음의 경로 부분의 시작(/) 부분을 \0으로 처리
            *ptr = '\0';
            ptr += 1;
            //주의 : strcpy의 두 번째 인자에는 반드시 string이 들어가야 함 (그러지 않을 시 segfault 오류)
            strcpy(uri_ptos, "/");  //만약 별도의 경로가 있다면(예: http://github.com:80/dnwls6102)
            strcat(uri_ptos, ptr);//uri_ptos에는 'dnwls6102\0'이 저장될 것임
            printf("Saved string: %s\n", uri_ptos);
        }
    }

    //포트 번호 추출하기 : 원리는 서버로 보내는 uri 주소를 추출하는 것과 동일
    if (ptr = strchr(host, ':'))
    {
        *ptr = '\0';
        ptr += 1;
        strcpy(port, ptr);
    }
    else //만약 별도의 포트 번호가 없다면 : 기본 포트 번호 80 부여
        strcpy(port, "80");
}

int request_to_server(int connfd, char *host, char *port, char *uri_ptos)
{
    char buffer[MAXLINE];
    char *cached_response = find_in_cache(uri_ptos);

    if (cached_response != NULL)
    {
        Rio_writen(connfd, cached_response, MAXLINE);
        return 0;
    }
    //프록시 서버에서로부터 목적 사이트로 요청을 보내는 소켓 생성
    int p_clientfd = Open_clientfd(host, port);
    //서버 응답 여부 flag
    int n;

    if (p_clientfd < 0)
    {
        fprintf(stderr, "Connection to destination failed\n");
        return -1;
    }

    //주의 : 개행 문자를 만나지 않으면 출력이 버퍼에만 남아있고 터미널에 출력되지 않을 수 있음
    //주의 2 : sprintf는 기존의 버퍼에다가 입력 문자열을 덮어씌우니, 반드시 문자열이 시작되기 전에 %s 서식문자를 주고, buffer를 대응시켜 내용을 이어가야 함
    sprintf(buffer, "GET %s HTTP/1.0\r\n", uri_ptos);
    sprintf(buffer, "%sHost: %s\r\n", buffer, host);
    sprintf(buffer, "%s%s", buffer, user_agent_hdr);
    sprintf(buffer, "%sConnection: close\r\n\r\n", buffer);

    printf("Request header to server: ");
    printf("%s", buffer);

    //요청 전송
    Rio_writen(p_clientfd, buffer, MAXLINE);

    //목적지 사이트로부터의 응답을 다시 클라이언트 소켓으로 전송
    while ((n = Rio_readn(p_clientfd, buffer, MAXLINE)) > 0)
    {
        Rio_writen(connfd, buffer, n);
        store_in_cache(uri_ptos, buffer, n);
    }

    //프록시 서버의 클라이언트 소켓 닫기
    Close(p_clientfd);
    

    return 0;
}

char *find_in_cache(char *uri_ptos)
{
    //캐시 메모리 공간 전체를 돌면서
    for (int i = 0; i < MAX_CACHE_SIZE; i++)
    {   
        //만약 캐시 url이 유효하고, url과 uri_ptos가 같다면
        if (cache[i].url != NULL && strcmp(cache[i].url, uri_ptos) == 0)
        {
            //저장해뒀던 데이터 반환
            return cache[i].response_data;
        }
    }
    //없으면 NULL 반환
    return NULL;
}

void store_in_cache(char *uri_ptos, char *response_data, size_t size)
{
    for (int i = 0; i < MAX_CACHE_SIZE; i++)
    {
        if (cache[i].url == NULL)
        {
            cache[i].url = strdup(uri_ptos);
            cache[i].response_data = malloc(size);
            memcpy(cache[i].response_data, response_data, size);
            cache[i].size = size;
            cache[i].last_accessed = time(NULL);
            return;
        }
    }
}