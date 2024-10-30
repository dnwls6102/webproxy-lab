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
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      if(S_ISREG(sbuf.st_mode))
        printf("권한 없음\n");
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

  if (Fork() == 0)
  {
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}