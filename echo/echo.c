#include "csapp.h"

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    //rio 입출력 함수 사용을 위한 rio_t 구조체 변수 rio 초기화
    rio_readinitb(&rio, connfd);
    //소켓에 저장된 문자열 읽어오기
    while((n = rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        //오류 발생하면 출력
        if (n < 0) {
        perror("rio_readlineb error\n");
        break;
        }
        //메세지 전달 받았다고 출력
        printf("server received %d bytes \n", (int)n);
        fflush(stdout);
        //전달받은 메세지를 그대로 다시 클라이언트 소켓으로 전송
        rio_writen(connfd, buf, n);
    }
}