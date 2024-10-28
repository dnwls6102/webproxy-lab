#include "csapp.h"

int main(int argc, char **argv)
{
    //클라이언트 소켓 식별자를 저장하는 변수 clientfd
    int clientfd;
    //host 문자열과 포트번호 문자열, 그리고 입력하는 문자열을 저장할 버퍼
    char *host, *port, buf[MAXLINE];
    //rio 입출력 함수를 위한 rio_t 구조체
    rio_t rio;

    // 처음 실행할 때 매개변수가 3개가 아니면
    if (argc != 3)
    {
        //사용법을 알려주고
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        //프로그램 종료
        exit(0);
    }
    //입력받은 첫 번째 매개변수를 host로
    host = argv[1];
    //두 번째 매개변수를 port로
    port = argv[2];

    //open_clientfd함수를 통해 반환받은 클라이언트 소켓의 식별자 값을 clientfd에 입력
    clientfd = open_clientfd(host, port);
    //만약 clientfd에 저장된 값이 -1이면
    if (clientfd == -1)
    {
        //오류 메세지 출력
        printf("NO Connection\n");
    }
    //rio 입력 함수를 위해 선언한 rio_t 변수 rio의 초기화
    rio_readinitb(&rio, clientfd);

    //입력받은 값이 NULL값이 아니면(키보드 인터럽트가 아닌 이상 웬만해서 멈출 일은 없음)
    while (fgets(buf, MAXLINE, stdin) != NULL)
    {
        //문자열 버퍼에 입력받은 값을 클라이언트 소켓에 전달
        rio_writen(clientfd, buf, strlen(buf));
        //클라이언트 소켓에서 문자열을 읽어 버퍼에 다시 저장
        rio_readlineb(&rio, buf, MAXLINE);
        //콘솔창에 출력
        fputs(buf, stdout);
    }
    close(clientfd);
    exit(0);
}