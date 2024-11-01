/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#define No119

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char * method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char* method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  //rio 입출력함수 사용을 위해 rio 변수 초기화
  Rio_readinitb(&rio, fd);
  //rio 변수의 버퍼에 저장된 값을 한 줄씩 읽어 buf에 저장
  //HTTP 요청 처리의 종료 조건 : Rio_readlineb가 EOF를 만나면 종료
  //만약 Rio_readlineb가 EOF를 만나지 못한다면, 요청을 계속해서 읽어오게 되고, 응답을 반복적으로 생성할 수 있음
  if (!Rio_readlineb(&rio, buf, MAXLINE))
      return; //만약 클라이언트 소켓으로부터 요청을 읽어들이지 못했다면 doit함수 종료
  printf("Request headers: ");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0)
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  //HTTP 요청 헤더를 읽어와 콘솔 창에 출력 : tiny 서버를 구현하는 데에 사용하진 않음
  read_requesthdrs(&rio);

  //parse_uri를 통해서 클라이언트가 정적 컨텐츠를 요구하는지, 아니면 브라우저 경로에 /cgi-bin을 추가시켜서 동적 컨텐츠를 요구하는지 판단
  //정적이면 is_static에 1이, 동적이면 is_static에 0이 들어감
  is_static = parse_uri(uri, filename, cgiargs);
  //stat 함수 : 파일의 정보를 가져오는 시스템 콜. 파일을 찾지 못하면 -1 반환
  //이 함수를 통해 sbuf에 유저에게 요청받은 경로의 파일의 메타 데이터를 저장시킴
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) {
    //S_ISREG : 파일 모드를 검사하여 해당 파일이 일반 파일인지를 검사
    //S_IRUSR 매크로 상수 : 파일의 소유자에게 읽기 권한이 있는지 나타내는 매크로 함수. 파일의 소유자는 당연히 Tiny 서버
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }

  else //동적 컨텐츠를 요청한 경우 : 하위 조건문은 정적 컨텐츠의 경우와 동일
  {
    //S_IXUSR : 파일의 소유자에게 실행 권한이 있는지를 확인하는 데에 사용됨
    //모든 파일들은 바이너리 구조로 구성됨. S_IXUSR과의 비트 연산을 사용하면 해당 파일의 실행 권한 여부를 확인 가능
    //HTML은 단순 텍스트 파일. 즉 실행 가능한 파일이 아니기에 사용자에게 실행 권한이 없는 것이다
    //그래서 S_IXUSR & sbuf.st_mode는 FALSE가 나오는 것이고, 그래서 cgi_bin에 html파일을 두면 안되는 것이다
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
     serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  //body에 옮기기만 하고, 반환을 하지 않았는데 어떻게 브라우저에 띄우는지?
  //Rio_writen(fd, body, strlen(body))를 통해 소켓에 저장시켜서 함. 즉, HTML 형식으로 응답하는 것
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  //클라이언트 소켓에 보내줄 HTTP 응답
  //우리가 buf에 응답 콘텐츠 유형과 응답 길이를 명시해 주었고
  //Rio_writen으로 buf와 body를 모두 전달해주기에
  //클라이언트의 웹 브라우저가 잘 알아듣고 HTML을 렌더링하는 것
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  //uri 경로에 cgi-bin이 없다면 => 정적 컨텐츠를 요구했다면
  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if(uri[strlen(uri) -1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else //동적 컨텐츠를 요구했다면
  {
    //유저가 동적으로 입력한 값이 나오는 부분인 ? 부분의 인덱스를 저장
    ptr = index(uri, '?');
    //있다면
    if(ptr)
    {
      //? 다음의 문자열을 cgiargs에 저장
      strcpy(cgiargs, ptr + 1);
      //?가 있던 부분(ptr에는 ?가 저장된 메모리 주소가 저장되어 있음)을 \0으로 바꾸어 uri에서 동적 입력값을 없애기
      //원래 uri에 "/cgi-bin/script.cgi?name=value"가 저장되어 있었다면
      //ptr에 \0을 대입시키면 "/cgi-bin/script.cgi\0name=value"가 되어버림
      //C에서 문자열을 인식할 때, \0을 만나면 문자열이 종료되었다고 인식하기 때문에
      //uri는 이제 사실상 "/cgi-bin/script.cgi"가 되는 것이다
      *ptr = '\0';
    }
    else //없다면
      //cgiargs에 빈 값을 전달
      strcpy(cgiargs, "");
    //현재 경로(tiny 실행 파일이 저장된 경로)에서 uri 경로를 타고 들어가야 하니까 온점(.)추가
    strcpy(filename, ".");
    //filename에 uri 추가
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char* method)
{
  //서버 소켓의 파일 디스크립터
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //get_filetype으로 요청받은 파일의 유형을 파악함. html 파일, png, jpg, gif, txt 파일만 구현되어 있음
  get_filetype(filename, filetype);
  //buf 문자열에 HTTP 요청 헤더를 작성함
  sprintf(buf, "HTTP/1.0 200 OK \r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  //작성한 내용이 저장된 buf 문자열을 클라이언트 소켓으로 전송
  Rio_writen(fd, buf, strlen(buf));
  //그리고 헤더 내용을 그대로 콘솔창에도 출력
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcmp(method, "GET") == 0)
  {
      // #ifdef No119
      // 파일을 읽고 클라이언트에게 전송
      srcfd = Open(filename, O_RDONLY, 0); // 파일을 읽기 전용으로 엶
      srcp = (char*) malloc(filesize); // 파일 크기만큼 메모리 할당
      if (Rio_readn(srcfd, srcp, filesize) != filesize)
      {
          fprintf(stderr, "File read failed\n");
          Close(srcfd);
          free(srcp);
          return;
      }
      Close(srcfd); // 파일 닫기

      // 파일 내용을 클라이언트에게 전송
      //Rio_writen(fd, srcp, filesize);
      if (rio_writen(fd, srcp, filesize) != filesize)
      {
          fprintf(stderr, "File write to client failed\n");
          free(srcp);
          return;
      }
      // if (Rio_writen(fd, srcp, filesize) != filesize)
      // {
      //     fprintf(stderr, "File write to client failed\n");
      //     free(srcp);
      //     return;
      // };

      // 할당된 메모리 해제
      free(srcp);
      // #endif

      // #ifdef ORIGIN
      // //Open의 매개변수에는 filename이라는 이름의 변수가 전달되긴 했지만, 실제로는 파일 경로를 전달하면 됨
      // //즉, 아무것도 입력하지 않은 경우에는 ./home.html이 전달되고
      // //tiny 실행 파일 이외의 경로릐 파일을 열게 된다면 ./directory1/directory2/file 같은 느낌의 문자열이 전달되는 것임
      // srcfd = Open(filename, O_RDONLY, 0);
      // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
      // Close(srcfd);
      // Rio_writen(fd, srcp, filesize);
      // Munmap(srcp, filesize);
      // #endif
      }

}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char* method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  //HEAD 메소드로 요청을 보냈다면
  if (strcmp(method, "HEAD") == 0)
  {
    sprintf(buf, "%sContent-type: text/html\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, 0);
    Rio_writen(fd, buf, strlen(buf));
    return;
  }

  //Fork(): 현재 프로세스를 복사하여 자식 프로세스를 생성하는 시스템 콜
  //Fork()의 반환값이 0이다 : 자식 프로세스에서 호출되었음을 의미
  //부모 프로세스는 계속해서 요청을 받고 있어야 하기 때문에(listening 소켓)
  //자식 프로세스를 별도로 생성하여 클라이언트 소켓의 요청을 처리한다
  //정적 콘텐츠를 보여줄 때 별도의 프로세스를 생성하지 않는 이유: 단순한 파일 전송 작업이라 굳이 필요하지 않음
  if (Fork() == 0)
  {
    //setenv : 환경 변수 설정 함수. QUERY_STRING 환경 변수를 cgiargs(현재 ? 이후 100 & 300 부분이 저장되어 있음)로 설정.
    //setenv의 세 번째 매개변수 1 : 0이면 기존의 환경 변수 내용을 덮어쓰지 않음. 1이면 덮어 씀.
    setenv("QUERY_STRING", cgiargs, 1);
    //Dup2 : 파일 디스크립터를 복제하는 함수. 첫 번째 매개변수가 복제할 원본 파일 디스크립터
    //두 번째 매개변수는 복제한 파일 디스크립터가 가리킬 파일 디스크립터
    //표준 출력이 fd와 동일한 파일을 가리키게 된다 : 즉 이제 표준 출력으로 받은 값들은 클라이언트 소켓으로 그대로 넘어간다.
    //다시 말하자면, 기존에 표준 출력에 부여된 1이라는 파일 디스크립터 값이 이제 fd와 동일한 값으로 바뀌면서
    //프로세스에서 표준 출력을 호출하고, 값을 입력하면 콘솔창에 출력되는 게 아니고 클라이언트 소켓에 저장되는 것이다
    //언제까지? 프로세스가 종료되거나(Close), 또다른 Dup2 함수로 표준 출력이 가리키는 디스크립터가 다시 원본으로 바뀔때까지
    Dup2(fd, STDOUT_FILENO);
    //filename : CGI 프로그램이 저장된 경로
    //environ : 환경 변수 목록
    //Execve의 두 번째 매개변수 : 프로그램의 인자 목록, 즉 프로그램 시작할 때 같이 전달하는 변수들
    //emptylist를 사용한다는 것 : 별 다른 인자를 전달하지 않겠다는 의미. CGI 프로그램이 별 다른 인자 없이 실행됨을 의미한다
    Execve(filename, emptylist, environ);
    //만약 filename에 html 파일의 경로가 들어간다면, execve 함수는 -1을 반환하고 ENOTEXEC(파일이 실행 가능한 형식이 아님) errno를 설정할 것임
  }
  Wait(NULL);
}