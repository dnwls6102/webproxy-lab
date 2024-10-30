/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"
#define ORIGIN

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;
  #ifdef ORIGIN
  //URL에 쿼리가 살아 있다면
  if((buf = getenv("QUERY_STRING")) != NULL)
  {
    //쿼리에서 '&'가 나오는 부분을 포인터로 설정
    p = strchr(buf, '&');
    //'&'를 '\0'으로 바꿔주기
    *p = '\0';
    //buf에는 쿼리의 시작 부분이 들어가 있을테고, 원래 &가 \0으로 바뀌었으니 첫 번째 인수가 들어간 문자열이 됨
    strcpy(arg1, buf);
    //p+1을 하게 되면 두 번째 인수가 시작되는 문자열을 받아올 수 있게 됨
    strcpy(arg2, p + 1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }
  #endif
  
  #ifdef FORM_ADD
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';
    sscanf(buf, "first=%d", &n1);
    sscanf(p+1, "second=%d", &n2);
  }
  #endif

  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);
  exit(0);
}
/* $end adder */
