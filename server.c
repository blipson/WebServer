/* Server-side use of Berkeley socket calls -- receive one message and print 
   Requires one command line arg:  
     1.  port number to use (on this machine). 
   RAB 3/12 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAXBUFF 100
#define MAXTOK 100
#define MAXTOKSIZE 100
#define NWORKERS 15

/* NAME STRUCT FOR PARSING */

struct name {
  char** tok;
  int count;
  int status;
};

/* END OF NAME STRUCT */

/* TDATA STRUCT FOR EACH THREAD */

struct tdata {
  int serverd; /* Server socket descriptor */
  int ret; /* Return value */
  int *next_request_id; /* Address of next_request_id variable */
  char *prog; /* argv[0] */
  int id; /* index of the thread */
};

/* END OF TDATA STRUCT */

/* REQUEST_TYPE FUNCTION FOR DETERMINING THE TYPE OF REQUEST */

int request_type(char *type, char *http) {
  if ('G' == type[0] && 'E' == type[1] && 'T' == type[2]) {
    if ((!strcmp("HTTP/1.1\n", http)) || (!strcmp("http/1.1\n", http))) {
      return 1; /* 1 is for a GET request */
    }
  }
  else if ('H' == type[0] && 'E' == type[1] && 'A' == type[2] && 'D' == type[3]) {
    if ((!strcmp("HTTP/1.1\n", http)) || (!strcmp("http/1.1\n", http))) {
      return 2; /* 2 is for a HEAD request */
    }
  }
  else if ('P' == type[0] && 'O' == type[1] && 'S' == type[2] && 'T' == type[3]) {
    return 3; /* 3 is for a POST request */
  }
  else {
    return 4; /* 4 is for an error */
  }
}

/* END OF REQUEST_TYPE FUNCTION */

/* PROCESS_REQUESTS FUNCTION */
void *process_requests(void *td) {

  struct sockaddr_in ca;
  struct tdata t_data = *((struct tdata*)td);
  char buff[MAXBUFF]; /* message buffer */
  int clientd; /* Socket desriptor for communicating with the client */
  int size = sizeof(struct sockaddr);
  int request_id;

  printf("Waiting for a incoming connection...\n");
  if ((clientd = accept(t_data.serverd, (struct sockaddr*) &ca, &size)) < 0) {
    printf("%s ", t_data.prog);
    perror("accept()");
    pthread_exit(NULL); /* exits with an accept error */
  }
  
  request_id = *t_data.next_request_id;

  if ((t_data.ret = recv(clientd, buff, MAXBUFF-1, 0)) < 0) {
    printf("%s ", t_data.prog);
    perror("recv()");
    pthread_exit(NULL);
  }

  buff[t_data.ret] = '\0';  // add terminating nullbyte to received array of char
  printf("Received message (%d chars):\n%s\n", t_data.ret, buff);

  /* PARSING CODE */

  struct name n;
  int count2=0;
  int count3=0;
  int word_count=0;
  int space_count=0;
  n.tok = (char**) malloc(MAXTOK);

  while(count2<=10) {
    n.tok[count2] = (char*) malloc(MAXTOKSIZE);
    count2++;
  }

  for (count3=0; count3<=t_data.ret; count3++) {
    if(buff[count3] == '\0') {
      n.count = count3;
      break;
    }
    else if(buff[count3] == '\n') {
      n.count = count3;
      break;
    }
    else if(buff[count3] == '\r') {
      if(buff[count3+1] == '\n') {
	n.count = count3;
	break;
      }
    }
    else {
      if(isspace(buff[count3])) {
	/*We need to skip one*/
	if(word_count>0) {
	  n.tok[word_count][space_count-1] = '\0';
	}
	word_count++;
	space_count=0;
	n.tok[word_count][space_count] = buff[count3+1];
	space_count++;
      }
      else {
        if(word_count>0) {
	  n.tok[word_count][space_count] = buff[count3+1];
	  space_count++;
	}
	else {
	  n.tok[word_count][space_count] = buff[count3];
	  space_count++;
	}
      }
    }
  }

  /* END OF PARSING CODE */
  
  printf("The file you're trying to open is: %s\n", n.tok[1]);

  /* FILE CODE */

  FILE *pFile;
  char buff2[256];
  char buff3[256];
  int shift=0;
  if(n.tok[1][0] == '/') {
    shift = 1;
  }
  pFile=fopen(n.tok[1]+shift, "r");

  /* HEADER CODE */
  char small_hbuff[10];
  sprintf(small_hbuff, "%d", n.count);
  char header_timestamp[30];
  time_t header_now;
  header_now = time(NULL);
  strftime(header_timestamp, 30, "%a, %d %b %Y %T %Z", gmtime(&header_now));
  char header_buffer[175];
  if(pFile==NULL) {
    strcpy(header_buffer, "HTTP/1.1 404 FILE NOT FOUND\n");
  }
  else {
    if((request_type(n.tok[0], n.tok[2]) != 1) &&  (request_type(n.tok[0], n.tok[2]) != 2) && (request_type(n.tok[0], n.tok[2]) != 3)) {
	strcpy(header_buffer, "HTTP/1.1 400 BAD REQUEST\n");
    }
    else {
      if(request_type(n.tok[0], n.tok[2]) ==  3) {
	strcpy(header_buffer, "HTTP/1.1 501 NOT IMPLEMENTED\n");
      }
      else {
	strcpy(header_buffer, "HTTP/1.1 200 OK\n");
      }
    }
  }
  strcat(header_buffer, "Date: ");
  strcat(header_buffer, header_timestamp);
  strcat(header_buffer, "\nConnection: close\n");
  strcat(header_buffer, "Content-Type: text/html; charset=utf-8\n");
  strcat(header_buffer, "Content-Length:");
  strcat(header_buffer, small_hbuff);
  strcat(header_buffer, "\n\n");
  fputs(header_buffer, stdout);
  printf("\n");
  if ((t_data.ret = send(clientd, header_buffer, strlen(header_buffer), 0)) < 0) {
    printf("%s ", t_data.prog);
    perror("send()");
  }

  /* END OF HEADER CODE */

  if(pFile==NULL) {
    perror("Error 404: File Not Found.");
  }
  else {
    while(!feof(pFile)) {
      if(request_type(n.tok[0], n.tok[2]) == 1) {
	if(fgets(buff2, 100, pFile) != NULL) {

	  /* SENDER CODE */

	  if ((t_data.ret = send(clientd, buff2, strlen(buff2), 0)) < 0) {
	    printf("%s ", t_data.prog);
	    perror("send()");
	  }

	  /* END OF SENDER CODE */
	}
      }
      else if(request_type(n.tok[0], n.tok[2]) == 2) {
	printf("HEAD REQUEST\n");
	break;
      }
      else if(request_type(n.tok[0], n.tok[2]) == 3) {
	printf("POST REQUEST\n");
	break;
      }
      else {
	printf("UNKNOWN REQUEST");
	break;
      }
    }
    fclose(pFile);
  
    /* END OF FILE CODE */

    /* LOG FILE CODE eventually will be its own function*/
    printf("Creating log file...\n");
    FILE *logFile;
    logFile = fopen("log.txt", "a");

    time_t log_now;
    char log_timestamp[30];
    log_now = time(NULL);
    strftime(log_timestamp, 30, "%a, %d %b %Y %T %Z", gmtime(&log_now));
    fprintf(logFile, "\n");
    fprintf(logFile, "Thread ID: %d", t_data.id);
    fprintf(logFile, "\n");
    fprintf(logFile, buff);
    fprintf(logFile, log_timestamp);
    fprintf(logFile, "\n\n");
  }

  /* END OF LOG FILE CODE */

  /* CLOSING CODE */
  if ((t_data.ret = close(clientd)) < 0) {
    printf("%s ", t_data.prog);
    perror("close(clientd)");
    pthread_exit(NULL);
  }
  //pthread_exit(NULL);
  
}

/* END OF PROCESS_REQUESTS FUNCTION */

int main(int argc, char **argv) {
  char *prog = argv[0];
  int port;
  int serverd;  /* socket descriptor for receiving new connections */
  int ret; /* return value from the call */
  int next_request_id;

  if (argc < 2) {
    printf("Usage:  %s port\n", prog);
    return 1;
  }
  port = atoi(argv[1]);

  if ((serverd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("%s ", prog);
    perror("socket()");
    return 1;
  }
  
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
    printf("%s ", prog);
    perror("bind()");
    return 1;
  }
  if (listen(serverd, 5) < 0) {
    printf("%s ", prog);
    perror("listen()");
    return 1;
  }

  /* THREADING CODE */

  pthread_t threads[NWORKERS];
  int p_thread;
  long t=0;
  int worker_count;
  int thread_count;
  int thread_error;
  void *status;

  struct tdata tdarray[NWORKERS];
  for(worker_count=0;worker_count<NWORKERS;++worker_count) {
    struct tdata td;
    td.id = worker_count;
    td.serverd = serverd;
    td.ret = ret;
    td.next_request_id = &next_request_id;
    td.prog = prog;
    tdarray[worker_count] = td;
  }

  for(thread_count=0;thread_count<NWORKERS;++thread_count) {
    p_thread = pthread_create(&threads[t], NULL, process_requests, (void*) &tdarray[thread_count]);
    if(thread_error) {
      printf("ERROR: return code from pthread_create() is %d\n", thread_error);
      exit(-1);
    }
  }

  pthread_join(threads[0], NULL);
  if(thread_error) {
    printf("ERROR: return code from pthread_join() is %d\n", thread_error);
  }
  else {
    printf("I am the MAIN, and I successfully completed join.\n");
  }

  /* END OF THREADING CODE */

  if ((close(serverd)) < 0) {
    printf("%s ", prog);
    perror("close(serverd)");
    return 1;
  }
  return 0;
}
