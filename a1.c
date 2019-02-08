#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define DEFAULT_PORT 80 //default server port
#define LENGTH 1024
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/* extracts port number from header, if provided*/
/* else port number set to default*/
int checkport(const char *m)
{
	/* get last occurrence of : */
	char *portno = strrchr(m, ':');
	portno = strtok(portno, ":"); 
	int val = atoi(portno);
	if(val != 0)
		return val;
	else 
		return DEFAULT_PORT;
}

/* extracts hostname from header */
void gethost(const char *m, char **hostname)
{
	fprintf(stderr, "our message:%s\n", m);
	/* check if "Host" has space after */
	*hostname = strtok((char *)m, " ");
	*hostname = strtok(NULL, "\r\n");
	//fprintf(stderr, "testing %s\n", hostname);
		
}
/* reads one line from request header */
void readaline(char *buffer, int *port_num, char **hostname) 
{
	const char *message = strtok(buffer, "\r\n");
	if(message) {
		/* get portno */
		if(strncmp(message, "GET", strlen("GET")) == 0) {
			fprintf(stderr, "Got GET field\n");
			*port_num = checkport(message);
		}
		else if(strncmp(message, "Host: ", strlen("Host: ")) == 0) {
			fprintf(stderr, "Got Host field\n");
			gethost(message, hostname);
		}
	}
}

/* forward client HTTP request to server */
char* forward(char* buffer, int port_num)
{ 
	char hostname[] = "www.cs.tufts.edu"; /*hard code for now */
	struct hostent *server;
	/* extract port number from path*/
	int sockfd, n;
	struct sockaddr_in serv_addr;
	/* get host information */
	server = gethostbyname("www.cs.tufts.edu");
	printf("%s\n", hostname);
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
	/* sets fields in HTTP server addr struct */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port_num);
	/* connect to HTTP server */ 
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
	n = write(sockfd,buffer,strlen(buffer));
	if (n < 0) 
         error("ERROR writing to socket");
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n",buffer);
    close(sockfd);
	return buffer;
}


int main (int argc, char* argv[]) {
	/* n is the return value for read/write calls */
	/* receiving */
	int sockfd, newsockfd, n, pid, portno;
	socklen_t clilen;
	char buffer[4096], temp[4096];
	char *response;
	struct sockaddr_in serv_addr, cli_addr;
	
	/* request header */
	int port_num;
	char* hostname = (char*) malloc(sizeof(char)*10);
	
	if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
	}
	
	/* create new socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
        error("ERROR opening socket");
	
	/* initialize to 0 */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	/* read provided port */
	portno = atoi(argv[1]); 
	serv_addr.sin_family = AF_INET;			/* set address family */
    serv_addr.sin_addr.s_addr = INADDR_ANY;	/* set IP address */
    serv_addr.sin_port = htons(portno);		/* set port number */
	
	/* bind socket to address of host run on server */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		error("ERROR on binding");
	
	/* listen for connections */
	listen(sockfd,5);
	fprintf(stderr, "%s: server listening on port %d \n", argv[0], portno);
	
	/* get size of client addr struct */
	clilen = sizeof(cli_addr);
	while(1){
	port_num = DEFAULT_PORT;
	/* establish connection with client */
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0)
		error("ERROR on accept");
	pid = fork();
	if (pid < 0) 
		error("ERROR on fork");
	if (pid == 0) {
		close(sockfd);
		bzero(buffer,1024);
		bzero(temp, 4096);
		/* read client request */
		n = read(newsockfd,buffer,sizeof(buffer));
		if (n < 0) error("ERROR reading from socket");
		printf("Here is the message: %s\n",buffer);
		
		/* save message */
		strcpy(temp, buffer);
		fprintf(stderr, "%temp: s\n", temp);
		readaline(temp, &port_num, &hostname);
		printf("hostname:%s\n", hostname);
		fprintf(stderr, "port number:%d\n",port_num);
		/* parse GET request */
		/* forward request to server */  
		//response = forward(buffer);
		/* send HTTP response to client */
		n = write(newsockfd,buffer,sizeof(buffer));
		if (n < 0) error("ERROR writing to socket");
		exit(0);
	}
	else 
		close(newsockfd);
    }
    close(sockfd);
    return 0; 
}