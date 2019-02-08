#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define SERV_TCP_PORT 9000 // default server port
#define MAX_MESG 1024
#define MAX_ADDR 80
#define HISTORY_LEN 10
//define some strings that we need for processing the messages
#define GET "GET"
#define USERAGENT "User-Agent"
#define NOINFO "No info provided"

#define TRUE 1
#define FALSE 0

//replaces characetr "charIndex" with multi-character string
//"replace" in buffer.
void replaceCharWithStr(char *buffer, int charIndex, char* replace) {
  char temp[MAX_MESG];
  //abort if buffer doesn't have space
  assert(strlen(buffer) + strlen(replace) + 1 < MAX_MESG);
  //make a copy of original string
  strncpy(temp, buffer, strlen(buffer));
  buffer[0] = '\0';
  //grab everything before the index
  strncat(&buffer[0], &temp[0], charIndex);
  //add our replacement string
  strncat(&buffer[charIndex], replace, strlen(replace));
  //finish off with the rest of the original string
  strncat(&buffer[charIndex + strlen(replace)], &temp[charIndex+1], strlen(temp)-charIndex);
}

//Makes sure the buffer string does not have any '\n' or '\r'
//as well as replaces dangerous characters like '<', '>', and '"'
//with their character code equivalents
int cleanString(char *buffer) {
  //fprintf(stderr, "Starting to clean string '%s'\n", buffer);
  int length = strlen(buffer);
  int i;
  //first replace any newlines
  for (i=length-1; (buffer[i]=='\r' || buffer[i]=='\n') && i>=0; i--) { 
    buffer[i]='\0'; 
  }
  //replace dangerous characters
  for(i = length - 1; i>=0; --i) {
    if(buffer[i] == '<') {
      replaceCharWithStr(buffer, i, "&lt;");
    }
    if(buffer[i] == '>') {
      replaceCharWithStr(buffer, i, "&gt;");
    }
    if(buffer[i] == '"') {
      replaceCharWithStr(buffer, i, "&quot;");
    }
  }


  return strlen(buffer);
}

//reads one line from the passed file
//Also cleans it by calling cleanString
//returns the length of the read string
int readOneLine(FILE *r, char *buffer, int buflen) { 
  char *ret = fgets(buffer, MAX_MESG, r); 
  if (ret) { 
    return cleanString(buffer);
  } else { 
    return 0; 
  }
}

int main(int argc, char* argv[]) {

  /*server*/
  int sockfd;
  //  int newsockfd;
  struct sockaddr_in serv_addr;
  int serv_port;
  
  int i;

  /* client */
  struct sockaddr_in cli_addr;    //raw client address
  char cli_dotted[MAX_ADDR];      //message ip address

  /* Record of who's connected */
  char * connectionRecords[HISTORY_LEN];
  for(i = 0; i < HISTORY_LEN; ++i) {
    connectionRecords[i] = (char*) malloc(sizeof(char) * MAX_MESG);
    connectionRecords[i][0]='\0';
  }
  
  //store the head of the list
  //we only the need the head - that way we'll overrite outdated entries
  //when we increment and take a mod
  //when a new entry comes in, it will delete the oldest one
  int head = 0;

  //this part taken from exercize answer written by Prof Couch
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  //read the provided port
  sscanf(argv[1], "%d", &serv_port);

  //Open TCP socket
  if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("can't open stream socket");
    exit(2);
  }

  //bind the local address, so the client can send to the server
  memset((char*) &serv_addr, 0, sizeof(serv_addr)); //initialize to 0's
  serv_addr.sin_family = PF_INET;                   //set domain to INET
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);    //source address is local
  serv_addr.sin_port = htons(serv_port);            //port is the passed one
  
  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("Can't bind to local address");
    exit(3);
  }

  //Listen to the socket
  listen(sockfd, 5);
  fprintf(stderr, "%s: sever listening on port %d\n", argv[0],serv_port);

  for(;;) {
    //wait for connection from client. this is an interactive server
    int mesglen = sizeof(cli_addr);
    int newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, (socklen_t*)&mesglen);
    
    if (newsockfd < 0) {
      perror("can't accept connection from client");
      exit(4);
    }
    
    //get numeric internet address of client
    inet_ntop(PF_INET, (void*)&(cli_addr.sin_addr), cli_dotted, MAX_MESG);
    fprintf(stderr, "%s: sever connection from %s\n", argv[0], cli_dotted);

    //read message from the client
    //do it in FILE*, so reading and writing will be nicer
    FILE *w = fdopen(newsockfd, "w");
    FILE *r = fdopen(newsockfd, "r");
    char fromClient[MAX_MESG];

    //grab the current time as a string
    //used both to tell the time back to the client
    //and to store the time that each client connected
    const time_t curTime = time(NULL);
    char* curTimeString = ctime(&curTime);
    cleanString(curTimeString);

    //read a line
    if(!readOneLine(r, fromClient, MAX_MESG)) {
      //didn't get a line
      fprintf(stderr, "%s: premature EOF\n", argv[0]);
      fprintf(w, "HTTP/1.1 400 Bad Request\nConnection: close\nContent-Length: 0\n"); fflush(w);
    } else if(strncmp(fromClient, GET, strlen(GET)) == 0) {
      //we got a GET request
      //respond with the list of previously connected clients
      fprintf(stderr, "%s: got GET request\n", argv[0]);

      //construct the response string
      //need to do that completely to tell the server the length of the content
      char content[MAX_MESG];
      content[0] = '\0';   
      //start with the fixed text
      strcat(content, "<html><body><h1>hello world\n</h1><h2>List of previously connected clients (before this one):</h2><h3><br>");

      //cycle though each history entry
      //and add it to the message
      int cur;
      //start at 0, so the for loop will work
      for(i = 0; i < HISTORY_LEN; i++) {
	//since we used fgets, each record is guaranteed to end in a '/0'
	cur = (head + i) % HISTORY_LEN; //get the offset from the head so we grab the entries in order
	if(strlen(connectionRecords[cur])) { //don't do anything if there isn't a record yet
	  strcat(content, connectionRecords[cur]);
	  strcat(content, "<br>");
	}
      }
      //close the HTML tags
      strcat(content, "</h3></body></html>");

      fprintf(w, "HTTP/1.1 200 OK\nDate: %s\nContent-Type: text/html\nConnection: close\nContent-Length: %d\n\n%s\n", curTimeString, (int)strlen(content), content); fflush(w);
    } else {
      //we didn't recognise the request
      fprintf(stderr, "%s: Unrecognized request\n", argv[0]);
      fprintf(w, "HTTP/1.1 400 Bad Request\nConnection: close\nContent-Length: 0\n"); fflush(w);
    }

    //read into our records the information about the client
    int foundInfo = FALSE;
    //make a string to store
    char toStore[MAX_MESG];
    //start by coping the current time into that string
    strncpy(toStore, curTimeString, strlen(curTimeString));

    //strncpy claims to add a '\0' - it's not for me for some reason
    //uncomment the next line to see for yourself
    //for me, the two lengths were different
    //also verified by printing the time string above (line has since been removed)

    //fprintf(stderr, "first: '%s' length %d curTimeString length = %d\n", toStore, strlen(toStore), strlen(curTimeString));
    toStore[strlen(curTimeString)] = '\0';
    
    //add a nice colon
    strncat(toStore, ": ", 2);
    
    //add the information about the browser, if it's provided
    while(readOneLine(r, fromClient, MAX_MESG)) {
      //we're looking for the "User-Agent" line start
      if(strncmp(fromClient, USERAGENT, strlen(USERAGENT)) == 0) {
	char* infoStart = index(fromClient, ':') + 2; //split on the :, plus 2 to get past the ":" and the " "
	fprintf(stderr, "got info '%s'\n", infoStart);
	//add it to our string to store
	strncat(toStore, infoStart, strlen(infoStart));
	foundInfo = TRUE;
	break;
      }
    }
    //if browser info wasn't provided, add our nice error message
    if(!foundInfo) {
      strncat(toStore, NOINFO, strlen(NOINFO));
    }
    
    //copy our toStore string into the array
    strncpy(connectionRecords[head], toStore, strlen(toStore));    
    head = (head + 1) % HISTORY_LEN;

    fprintf(stderr, "%s: Closing connection from %s\n", argv[0], cli_dotted);
    fclose(r); 
    fclose(w);
  } //close infinite for loop

}