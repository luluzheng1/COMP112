#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>

#define DEFAULT_PORT 80 //default server port
#define LENGTH 2000000
#define numCache 10
#define SIZE 1024 * 2014 * 10
typedef struct Data {
	char *hostname, *url,*content;
	int Age,max_Age,last_accessed,portno; 
	time_t time_cached;
} Data;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void initialize_Struct(Data **d) {
	(*d)->hostname = (char *)malloc(sizeof(char *));
	(*d)->url = (char *)malloc(sizeof(char*));
	(*d)->portno = 80;
	(*d)->content = (char *)malloc(sizeof(char*));
	(*d)->Age = -1;
	(*d)->max_Age = -1;
	(*d)->last_accessed = -1;
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

void getURL(const char *m, Data **object)
{
	(*object)->url = strtok((char *)m, " ");
	(*object)->url = strtok(NULL, " ");
	//fprintf(stderr, "URL:%s\n", (*object)->url);
}

/* extracts hostname from header */
void gethost(const char *m, Data **object)
{
	/* check if "Host" has space after */
	(*object)->hostname = strtok((char *)m, " ");
	(*object)->hostname = strtok(NULL, "\r\n");	
	//fprintf(stderr, "hostname:%s\n", (*object)->hostname);
}
/* reads one line from request header */
void readaline(char *message, Data **object) 
{
	if(message) {
		/* get portno */
		if(strncmp(message, "GET", strlen("GET")) == 0) {
			fprintf(stderr, "Got GET field\n");
			char *tempstr = malloc(strlen(message)+1);
			memcpy(tempstr, message, strlen(message)+1);
			getURL(tempstr, object);
			(*object)->portno = checkport(message);
			//fprintf(stderr, "message:%s URL:%s\n port_num: %d\n", message, (*object)->url, (*object)->portno);
		}
		else if(strncmp(message, "Host: ", strlen("Host: ")) == 0) {
			//fprintf(stderr, "Got Host field\n");
			gethost(message, object);
		}
	}
}

/* extracts content length from response header*/
int getlength(char *message) 
{
	if(strncmp(message, "Content-Length: ", 
	strlen("Content-Length: ")) == 0) {
		char *content = strtok(message, " ");
		content = strtok(NULL, "\r\n");
		int content_length = atoi(content);
		return content_length;
	}	
	else
		return 0;
}

int get_MaxAge(char* buffer)
{
	if(strncmp(buffer, "Cache-Control: max-age=", 
	strlen("Cache-Control: max-age=")) == 0) {
		fprintf(stderr, "buffer: %s\n", buffer);
		char *temp = strtok(buffer, "=");
		temp = strtok(NULL, "\r\n");
		fprintf(stderr,"temp:%s\n", temp);
		int max_age = atoi(temp);
		fprintf(stderr, "temp:%s max_age:%d\n", temp, max_age);
		return max_age;
	}
	else /*some other protocol */
		return -1;
}	

double compute_Age(Data **Cache, int index) {
	time_t curr_time;
	time_t orig_time = (Cache)[index]->time_cached;
	
	time(&curr_time);
	double Age = difftime(curr_time, orig_time);
	printf("Age:%f\n", Age);
	return Age;
}
/* forward client HTTP request to server */
int forward(char* buffer, Data **object)
{ 
	struct hostent *server;
	int sockfd, n, content_length = 0;
	int header_length, max_Age;
	struct sockaddr_in serv_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	char temp[LENGTH];
	if (sockfd < 0) 
        error("ERROR opening socket");
	/* get host information */
	server = gethostbyname((*object)->hostname);
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
    serv_addr.sin_port = htons((*object)->portno);
	/* connect to HTTP server */ 
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
	n = write(sockfd,buffer,strlen(buffer));
	if (n < 0) 
         error("ERROR writing to socket");
    bzero(buffer,LENGTH);
	
	n = read(sockfd, buffer, LENGTH);
	memcpy(temp, buffer, strlen(buffer)+1); /* null char */
	char *state; /* ptr for state*/
	char *message = strtok_r(temp, "\r\n", &state); 
	/*determine content length */
	for(int i = 0; i < 20; i++) {
		char *temp = message;
		/*checks maxage*/
		(*object)->max_Age = get_MaxAge(temp);
		/*checks content length */
		int cl = getlength(message);
		if (cl != 0) 
			content_length = cl;
		message = strtok_r(NULL, "\r\n", &state);
	}
	fprintf(stderr, "content_length%d\n", content_length);
	/* check for partial read */
	int num = 1;
	while(n < content_length || num) {
		num = read(sockfd, &buffer[n], LENGTH);
		n += num;
		fprintf(stderr, "partial reading: %d \n", n);
	}

	fprintf(stderr, "size of content: %d\n", n);
    if (n < 0) 
        error("ERROR reading from socket");
   	//printf("%s",buffer);
   	close(sockfd);
	return n;
}

/* searches for object in cache */
/* returns the index of the item stored in cache */
int find_item(Data **Cache, char *URL)
{
	for(int i = 0; i < numCache; i++)
	{
		if(Cache[i]->url == URL)
			return i;
	}
	return -1;
}

/* extracts content in cache */
int get_item(Data **Cache, char **content, char*URL)
{
	int index = find_item(Cache, URL);
	if(index != -1)
	{
		*content = Cache[index]->content;
		return index;
	}
	else
		return -1;
}

void cache_item(Data **Cache, Data *object, int index)
{
	Cache[index]->hostname = object->hostname;
	Cache[index]->url = object->url;
	Cache[index]->content = object->content;
}
	
int main (int argc, char* argv[]) {
	/* receiving */
	/* n is the return value for read/write calls */
	socklen_t clilen;
	int sockfd, newsockfd, n, pid; 
	char buffer[LENGTH], temp[LENGTH];
	struct sockaddr_in serv_addr, cli_addr;
	
	/* intialize temp struct*/
	Data *object = malloc(sizeof(*object));
	initialize_Struct(&object);
	
	/* Declare array of pointers to struct object */
	/*Data *d[10];
	/* Allocate pointers and initialize cache*/ 
	/*for(int i = 0; i < numCache; i++){
		d[i] = malloc(sizeof(Data));
		if(d[i] == NULL)
		{
			printf("Memory allocation error");
		}
		initialize_Struct(&(d[i]));
	}
	/* Declare pointer to Cache array */
	/*Data*(*Cache)[] = &d;*/
	
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
	int portnum = atoi(argv[1]); 
	serv_addr.sin_family = AF_INET;			/* set address family */
    serv_addr.sin_addr.s_addr = INADDR_ANY;	/* set IP address */
    serv_addr.sin_port = htons(portnum);		/* set port number */
	
	/* bind socket to address of host run on server */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		error("ERROR on binding");
	
	/* listen for connections */
	listen(sockfd,5);
	fprintf(stderr, "%s: server listening on port %d \n", argv[0], portnum);
	
	/* get size of client addr struct */
	clilen = sizeof(cli_addr);
	while(1){
	/* establish connection with client */
	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0)
		error("ERROR on accept");
	pid = fork();
	if (pid < 0) 
		error("ERROR on fork");
	if (pid == 0) {
		close(sockfd);
		bzero(buffer,LENGTH);
		bzero(temp, LENGTH);
		/* read client request */
		n = read(newsockfd,buffer,sizeof(buffer));
		if (n < 0) error("ERROR reading from socket");
		
		/* local copy of header */
		memcpy(temp, buffer, strlen(buffer)+1); /* null char */
		char *state; /* ptr for state*/
		char *message = strtok_r(temp, "\r\n", &state);
		/* parse GET request */
		do{
			readaline(message, &object);
		}while((message = strtok_r(NULL, "\r\n", &state)) != NULL);
		
		/* forward request to server, returns n */  
		int length = forward(buffer, &object);
		/* send HTTP response to client */
		
		n = write(newsockfd,buffer,length);
		
		fprintf(stderr, "client receives n of:%d\n", n);
		if (n < 0) error("ERROR writing to socket");
		exit(0);
	}
	else 
		close(newsockfd);
    }
    close(sockfd);
    return 0; 
}

/*(*Cache)[0]->hostname = "why hello";
	(*Cache)[0]->hostname = buffer;
		printf("hostname: %s\n", (*Cache)[0]->hostname);
		(*Cache)[0]->hostname = buffer;
		printf("hostname: %s\n", (*Cache)[0]->hostname);
	printf("hostname: %s\n", (*Cache)[0]->hostname);
	printf("portno: %d\n", (*Cache)[0]->Age);
	printf("portno: %d\n", (*Cache)[0]->portno);
	*/
	