#include "csapp.h"

ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0)
    {
        if((nread = read(fd, bufp, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0;
            else
                return -1;
        }
        else if (nread == 0)
            break;
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0)
    {
        if ((nwritten = write(fd, bufp, nleft)) <= 0)
        {
            if (errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

void rio_readinitb(rio_t *rp, int fd)
{
    rp -> rio_fd = fd;
    rp -> rio_cnt = 0;
    rp -> rio_bufptr = rp -> rio_buf;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while(rp -> rio_cnt <= 0)
    {
        rp -> rio_cnt = read(rp -> rio_fd, rp -> rio_buf, sizeof(rp -> rio_buf));
        if (rp -> rio_cnt < 0)
        {
            if (errno != EINTR)
                return -1;
        }
        else if (rp->rio_cnt == 0)
            return 0;
        else   
            rp -> rio_bufptr = rp -> rio_buf;
    }

    cnt = n;
    if (rp -> rio_cnt < n)
        cnt = rp -> rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp -> rio_bufptr += cnt;
    rp -> rio_cnt -= cnt;
    return cnt;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;

    for(n = 1; n < maxlen; n++)
    {
        if ((rc= rio_read(rp, &c, 1)) == 1)
        {
            *bufp++ = c;
            if (c == '\n')
            {
                n++;
                break;
            }
        }
        else if (rc == 0)
        {
            if (n == 1)
                return 0;
            else   
                break;
        }
        else
            return -1;
    }

    *bufp = 0;
    return n-1;
}

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while(nleft > 0)
    {
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1;
        else if (nread == 0)    
            break;
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}

int open_clientfd(char *hostname, char *port)
{
    int clientfd;
    struct addrinfo hints, *listp, *p; //addrinfo는 단일 연결 리스트. listp가 헤더 역할

    //앞으로 생성할 클라이언트 소켓이 연결 가능한 서버 주소 리스트를 받아옴
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; //소켓 타입을 스트림 소켓으로 설정
    hints.ai_flags = AI_NUMERICSERV; //숫자 타입 서버를 사용하겠음
    hints.ai_flags |= AI_ADDRCONFIG; //환경에 따라서 IPv4 / IPv6 주소를 선택할 수 있게끔 함
    getaddrinfo(hostname, port, &hints, &listp); //매개변수로 전달받은 호스트 주소와 포트 번호를 addrinfo 구조체로 변환

    //리스트 중 문제없이 연결 가능한 서버 주소를 고르는 과정
    for (p = listp; p; p = p -> ai_next)
    {
        printf("Trying to create socket: ai_family=%d, ai_socktype=%d, ai_protocol=%d\n", p->ai_family, p->ai_socktype, p->ai_protocol);
        //소켓 디스크립터(=식별자)를 생성 : 만약 socket() 함수 반환값이 0 이하(==-1)이라면
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
        {
            perror("소켓 생성 실패\n");
            continue; //소켓 생성 실패, 다음 주소로 넘어가기
        }
            
        
        //서버로 연결: 만약 연결에 실패하지 않았다면
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
        {
            printf("연결 성공\n");
            break; //연결 성공. 반복문 종료
        }
        close(clientfd); //서버와의 연결 실패. 소켓을 닫고 새로운 서버를 찾기
    }

    freeaddrinfo(listp); //연결 리스트의 헤더 해제
    if (!p) //전달받은 서버 주소 리스트가 없는 경우(이럴경우 p는 NULL이 됨)
        return -1;
    else //연결에 성공했다면
        return clientfd; //클라이언트의 소켓을 반환

}

int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p; //addrinfo는 단일 연결 리스트, listp가 헤더 역할
    int listenfd, optval = 1;

    //서버 주소 리스트를 받아옴 : 서버의 로컬 시스템의 모든 IP 주소를 받아온다고 생각하면 됨
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; //listen 전용 소켓 타입을 스트림 소켓으로 설정
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; //소켓을 listen 소켓으로 설정 및 상황에 맞게 IPv4 / IPv6 주소를 반환하기
    hints.ai_flags |= AI_NUMERICSERV; //service 이름 대신에 포트 번호를 직접 사용하는 것을 요청
    getaddrinfo(NULL, port, &hints, &listp); //NULL로 설정하게 되면, 로컬 시스템의 모든 IP 주소를 반환시켜 서버가 여러 IP 주소를 수신할 수 있게 함

    //bind(결합 / 연결) 가능한 소켓들 탐색
    for (p = listp; p; p = p -> ai_next)
    {
        //소켓 식별자를 생성함
        if ((listenfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) < 0)
            continue; //소켓 생성에 실패했을 경우 다음 bind 가능한 소켓으로 넘어감

        //"Address already in use(이미 사용중인 주소임)" 에러를 제거시키기
        //setsockopt : 소켓 옵션을 설정하는데 사용됨
        //SO_REUSEADDR : 동일한 포트를 여러 소켓이 사용할 수 있도록 허용.
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

        //소켓 디스크립터(=식별자)를 클라이언트 소켓 주소와 결합시키기
        if (bind(listenfd, p -> ai_addr, p -> ai_addrlen) == 0)
            break;
        //bind에 실패하면 소켓을 닫고 다음 bind 가능한 소켓으로 넘어가기
        close(listenfd);
    }

    freeaddrinfo(listp);

    if (!p) //전달받은 서버 주소 리스트가 없는 경우(이럴경우 p는 NULL이 됨)
        return -1;
    
    //연결 요청을 받을 수 있는 listening 소켓 만들기 : 만약 listening 소켓 생성에 실패한 경우
    if (listen(listenfd, LISTENQ) < 0)
    {
        close(listenfd); //listenfd 식별자를 가지고 있는 소켓을 닫기
        return -1;
    }
    return listenfd; //listening socket의 식별자를 반환

}