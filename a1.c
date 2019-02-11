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
#define LENGTH 4096
#define SIZE 10485760
#define numCache 10

typedef struct Data {
	char *hostname, *url,*content;
	double Age,max_Age;
	int portno; 
	time_t time_cached, time_accessed;
} Data;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void initialize_Struct(Data **d) 
{
	(*d)->hostname = (char *)malloc(100);
	(*d)->url = (char *)malloc(100);
	(*d)->portno = 80;
	(*d)->content = (char *)malloc(sizeof(sizeof(char) * SIZE));
	(*d)->Age = -1;
	(*d)->max_Age = -1;
	(*d)->time_cached = (time_t)malloc(sizeof(time_t));
	(*d)->time_accessed = (time_t)malloc(sizeof(time_t));
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
}

/* extracts hostname from header */
void gethost(const char *m, Data **object)
{
	/* check if "Host" has space after */
	(*object)->hostname = strtok((char *)m, " ");
	(*object)->hostname = strtok(NULL, "\r\n");	
}

/* reads one line from request header */
void readaline(char *message, Data **object) 
{
	if(message) {
		/* get portno */
		if(strncmp(message, "GET", strlen("GET")) == 0) {
			char *tempstr = malloc(strlen(message)+1);
			memcpy(tempstr, message, strlen(message)+1);
			getURL(tempstr, object);
			(*object)->portno = checkport(message);
		}
		else if(strncmp(message, "Host: ", strlen("Host: ")) == 0) {
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
		char *temp = strtok(buffer, "=");
		temp = strtok(NULL, "\r\n");
		int max_age = atoi(temp);
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
	return Age;
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
	Cache[index]->portno = object->portno;
	Cache[index]->Age = compute_Age(Cache, index);
	Cache[index]->max_Age = object->max_Age;
	time(&(Cache[index]->time_cached));
	time(&(Cache[index]->time_accessed));
}

/* Given an index, returns 1 if stale, 0 if fresh */
int check_stale(Data **Cache, int index)
{
	if(Cache[index]->Age >= Cache[index]->max_Age)
		return 1;
	else
		return 0;
}

/* Loops through cache to find potential stale item*/
/* returns index of stale item if found, else returns -1 */
int find_stale(Data **Cache)
{
	for(int i = 0; i < numCache; i++)
	{
		if(check_stale(Cache, i) == 1)
			return i;
	}
	return -1;
}

/* Searches Cache for oldest cached item */
int find_least_accessed(Data **Cache)
{
	int index = 0;
	time_t oldest = Cache[index]->time_accessed;
	for(int i = 0; i < numCache; i++)
	{
		if(difftime(Cache[i]->time_accessed, oldest) < 0)
		{
			oldest = Cache[i]->time_accessed;
			index = i;
		}
	}
	return index;
}

/* forward client HTTP request to server */
int forward(char* buffer, Data **object)
{ 
	struct hostent *server;
	int sockfd, n, content_length = 0;
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

	n = read(sockfd,(*object)->content, SIZE);
	memcpy(temp,(*object)->content, LENGTH); /* null char */
	char *state; /* ptr for state*/
	char *message = strtok_r(temp, "\r\n", &state); 
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
	/* check for partial read */
	int num = 1;
	while(n < content_length || num) {
		num = read(sockfd, &((*object)->content) + n, LENGTH);
		n += num;
	}
    if (n < 0) 
        error("ERROR reading from socket");
   	close(sockfd);
	return n;
}

int main (int argc, char* argv[]) {
	/* receiving */
	/* n is the return value for read/write calls */
	socklen_t clilen;
	int tracker = 0; /*number of objects in Cache */
	int sockfd, newsockfd, n, pid; 
	char buffer[LENGTH], temp[LENGTH];
	struct sockaddr_in serv_addr, cli_addr;
	
	/* intialize temp struct*/
	Data *object = malloc(sizeof(*object));
	initialize_Struct(&object);
	
	/* Declare array of pointers to struct object */
	Data *d[10];
	/* Allocate pointers and initialize cache*/ 
	for(int i = 0; i < numCache; i++){
		d[i] = malloc(sizeof(Data));
		if(d[i] == NULL)
			fprintf(stderr,"Memory allocation error");
		initialize_Struct(&(d[i]));
	}
	/* Declare pointer to Cache array */
	Data*(*Cache)[] = &d;
	
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
		
			int index = find_item(*Cache, object->url);
			if(index != -1 && !check_stale(*Cache, index))
			{
				/* if in Cache and fresh */
				/* send cache content to client */
				n = write(newsockfd, (*Cache)[index]->content, SIZE);
				time(&((*Cache)[index]->time_accessed));
			} 
			if((index != -1 && check_stale(*Cache, index)) || index == -1)
			{
				/* if in Cache and stale, or if not in Cache */
				/* forward to server */
				int length = forward(buffer, &object);
				/* send HTTP response to client */
				n = write(newsockfd,object->content,length);
				if (n < 0) error("ERROR writing to socket");
					exit(0);
				/* Caching */
				/* if Cache not full */
				if(tracker <= 10) 
				{
					cache_item(*Cache, object, tracker);
					tracker++;
				}
				else if(object->max_Age != 0)
				{
					/* replace with stale item */
					/* else replace with least recently accessed */
					if(find_stale(*Cache) != -1)
						cache_item(*Cache, object, find_stale(*Cache));
					else
						cache_item(*Cache, object, find_least_accessed(*Cache));
				}
			}
		}
		else 
			close(newsockfd);			
	}
    close(sockfd);
    return 0; 
}
	