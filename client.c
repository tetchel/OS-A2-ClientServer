#include <sys/socket.h>
#include <netdb.h>
#include <err.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//opens a socket to hostname:port
int open_client_socket(char* hostname, char* port) {
    //prepare getaddrinfo parameters
    struct addrinfo hints, *results;
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int retval;
    if(retval = getaddrinfo(NULL, port, &hints, &results)) {
	errx(EXIT_FAILURE, "%s", gai_strerror(retval));
    }	
    
    struct addrinfo* addr;
    int sockfd;
    
    //go through results and try and find a connection that works
    for(addr = results; addr != NULL; addr = addr->ai_next) {
	//attempt open socket
	sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

	//error
	if(sockfd == -1)
	    continue;
	
	//success
	if(connect(sockfd, addr->ai_addr, addr->ai_addrlen) != -1)
	    break;
    }
    
    if(!addr)
	errx(EXIT_FAILURE, "Unable to connect!\n");
    else
	return sockfd;
}

#define MAX_MSG_SIZE 1024

//sends to_send over socket sockfd, then returns the reply received from server as an integer
int send_and_receive(const int sockfd, char* to_send) {
    //attempt send
    if(send(sockfd, to_send, strlen(to_send), 0) == -1) {
	perror("recv");
	exit(EXIT_FAILURE);
    }
    
    //receive server response
    char recvbuff[MAX_MSG_SIZE];
    int bytes_read = recv(sockfd, &recvbuff, sizeof(recvbuff)-1, 0);
    if(bytes_read == -1) 
	errx(EXIT_FAILURE, "Couldn't recv from socket!\n");
    //null-terminate response
    recvbuff[bytes_read] = '\0';   
    
    //scan the result into an integer and return it
    int result;
    sscanf(recvbuff, "%d", &result);

    return result;
}

int main(int argc, char** argv) {
    if(argc != 4) {
	errx(EXIT_FAILURE, "Usage: ./client Server_Hostname Port_Number DataToSend\n");
    }
    //process command line args
    char*   hostname = argv[1],
	*   portnum  = argv[2],
	*   to_send  = argv[3];

    //open socket to server
    int sockfd = open_client_socket(hostname, portnum);
    
    //send request to server and read response
    int response = send_and_receive(sockfd, to_send);
    
    printf("Response was: 10*%s = %d\n", to_send, response);    

    close(sockfd);
    return 0;
}


