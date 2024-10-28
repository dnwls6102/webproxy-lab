#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)
{
    //듣기 소켓 식별자와 연결된 소켓의 식별자를 저장하는 변수
    int listenfd, connfd;
    //클라이언트 소켓의 주소 길이를 저장하는 변수 clientlen : IPv4 주소의 길이와 IPv6 주소의 길이가 다르다
    socklen_t clientlen;
    //클라이언트 소켓의 주소를 저장하는 변수 clientaddr
    struct sockaddr_storage clientaddr;
    //호스트 주소와 포트번호를 저장할 문자열 변수들
    char client_hostname[MAXLINE], client_port[MAXLINE];

    //실행할 때 입력받은 매개변수가 2개가 아니면(프로그램 이름 + 포트번호)
    if (argc != 2)
    {
        //사용법 알려주기
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    //listen 소켓 식별자 저장
    listenfd = open_listenfd(argv[1]);
    //listen 소켓 식별자 생성 실패할 경우 오류 메세지 출력
    if (listenfd == -1)
    {
        printf("Generating Listening socket failed\n");
    }
    while(1)
    {
        //클라이언트 소켓 주소의 길이를 저장시키기
        clientlen = sizeof(struct sockaddr_storage);
        //연결된 클라이언트 소켓의 파일 식별자 값을 저장하기
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        //받아온 소켓 주소를 호스트 이름과 포트 번호로 변환
        getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        //클라이언트 소켓과 연결되었음을 출력함
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        //소켓 클라이언트로 문자열 보내기
        echo(connfd);
        close(connfd);
    }
    exit(0);
}