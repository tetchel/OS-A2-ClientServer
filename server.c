#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <err.h>

//arguments passed to run function (described in main)
typedef struct arguments {
    int* arrayptr;
    int arraysize;
} arguments;

//locks connection array so only one thread works on it at a time
pthread_mutex_t array_lock;

//used to track number of enqueued connections
sem_t semaphore;

//for testing
//pthread_barrier_t barrier;

//bind server socket so we can begin listening
int bind_socket(struct addrinfo* results) {
    struct addrinfo* addr;
    int sockfd;
    
    //go through all results and try and find one we can create a bound socket to
    for(addr = results; addr != NULL; addr = addr->ai_next) {
	sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	//error, go to next
	if(sockfd == -1)
	    continue;
	
	if(bind(sockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
	    //bind failed
	    close(sockfd);
	    continue;
	}
	else
	    //success
	    break;
    }
    if(!addr) {
	errx(EXIT_FAILURE, "Unable to bind to a socket");
    }
    freeaddrinfo(results);
    return sockfd;
}

//opens accepting socket
int open_server_socket(const char* port) {
    //prepare getaddrinfo parameters
    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int retval;
    if((retval == getaddrinfo(NULL, port, &hints, &results)) == -1) {
	errx(EXIT_FAILURE, "%s", gai_strerror(retval));
    }

    int sockfd = bind_socket(results);
    
    //listen for new conenctions
    if(listen(sockfd, 25) == -1) {
	errx(EXIT_FAILURE, "Unable to listen on socket");
    }
    else {
	printf("Listening on port %s\n", port);
    }

    return sockfd;
}

//wait for a connection and print when one is received
int wait_for_connection(int sockfd) {
    struct sockaddr_in client_addr;
    unsigned addr_len = sizeof(struct sockaddr_in);
    //accept new connection (blocking call)
    int connectionfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);

    if(connectionfd == -1)
        errx(EXIT_FAILURE, "Unable to accept connection");

    //print out human-readable IP of client
    char ip[INET_ADDRSTRLEN];
    inet_ntop(client_addr.sin_family, &client_addr.sin_addr, ip, sizeof(ip));
    printf("Receieved connection from %s\n", ip);
    //return fd to this connection
    return connectionfd;
}

#define MAX_MSG_SIZE 1024

//function that each thread is executing
//receives request from client, then multiplies by 10, then replies to client
void* run(void* arg) {
    arguments* args = (arguments*)arg;    
    
    //do this forever
    while(1) {
	//wait for a connection, then decrement sem when we get one
	//blocks when there is no connection waiting
	sem_wait(&semaphore);
	//lock the array of connections as we work with it
	pthread_mutex_lock(&array_lock);

	//extract a connectionfd from args->arrayptr
	int connectionfd;
	int i;
	for(i=0; i < args->arraysize; i++) {
	    if(args->arrayptr[i] != 0) {
		//then arrayptr[i] is an open connection
		connectionfd = args->arrayptr[i];
		//signal that this connection has been removed from the queue
		args->arrayptr[i] = 0;
	    }
	}
	//finished with connection array
	pthread_mutex_unlock(&array_lock);

	//receive input from client
	char recvbuff[MAX_MSG_SIZE];
	int bytes_read = recv(connectionfd, recvbuff, sizeof(recvbuff)-1, 0);
	if(bytes_read == -1) {
	    perror("recv");
	    exit(EXIT_FAILURE); 
	}
	recvbuff[bytes_read] = '\0';

	//scan the number from data received
	int to_mult;
	sscanf(recvbuff, "%d", &to_mult);
	
	to_mult *= 10;
	
	char sendbuff[MAX_MSG_SIZE];
	//print multiplied number into a string
	snprintf(sendbuff, sizeof(sendbuff), "%d", to_mult);
	//send reply
	send(connectionfd, sendbuff, strlen(sendbuff), 0);
	//finished with this connection
	close(connectionfd);
	printf("Replied %d\n", to_mult);
    }
}

//main function handles all threading operations, networking is not done here
int main(int argc, char** argv) {
    //handle and re-output command line parameters
    if(argc != 4) {
	errx(EXIT_FAILURE, "Usage: ./server Port_Number Number_Of_Threads Connection_Array_Size\n");
    } 
    long int 
	numthreads  = strtol(argv[2], NULL, 10),
	arraysize   = strtol(argv[3], NULL, 10);

    char* portnum = argv[1];

    printf("Parameters: Port: %s Number of Threads: %ld Array Size: %ld\n", portnum, numthreads, arraysize);
    
    //initialize threadpool, connection array from parameters
    pthread_t threadpool[numthreads];
    int* connections_array = calloc(arraysize, sizeof(int));

    //create thread pool
    int i;
    for(i=0; i < numthreads; i++) {
	arguments* arg = malloc(sizeof(struct arguments));
	//arguments are a pointer to beginning of connection array
	arg->arrayptr = connections_array;
	//and the size of the array pointed to
	arg->arraysize = arraysize;
	if(pthread_create(&threadpool[i], NULL, run, (void*) arg)) {
	    errx(EXIT_FAILURE, "Fatal error creating thread\n");
	}
    }
    //initialize mutex and semaphore
    int status = sem_init(&semaphore, 0, 0);
    if(status)
	perror("sem_init");
    
    status = pthread_mutex_init(&array_lock, NULL);
    if(status)
	perror("pthread_mutex_init");

    //open accepting socket
    int sockfd = open_server_socket(portnum);

    //next index in array to place a new connection in
    int next_index = 0;
    //server loop
    while(1) {
	int connectionfd = wait_for_connection(sockfd);	
	if(connections_array[next_index] != 0)
	    //if it is full, wait for the previous thread on this index to finish executing before adding new one
	    pthread_join(threadpool[next_index], NULL);
	
	//assign the new connectionfd into the array
	connections_array[next_index] = connectionfd;

	//update next_index
	if(next_index == arraysize-1) 
	    next_index = 0;
	else 
	    next_index++;
    
	//indicate that there is a new connection in queue by incrementing sem	
	sem_post(&semaphore);
    }

    close(sockfd);
    return 0;
}
