/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#include "Slave.h"

#define PORT "10010"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

//GLOBAL
uint32_t next_IP = 0x83cc0e38;
uint8_t rid = 1;


void sigchld_handler(int s)
{
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


uint8_t compute_checksum(void *buf, size_t length) {
	uint8_t carry;
	uint16_t total = 0;
	uint8_t *cBuf = (uint8_t *)buf;
	int i;
	for (i = 0; i < length; i++) {
		total += (uint8_t)(cBuf[i]);
		carry = (uint8_t)(total / 0x100);
		// printf("%4x ",total);
		total = total & 0xFF;
		total += carry;
		// printf("%4x\n",total);
	}
	return ~((uint8_t)total);
}

void *forwarding_loop(void *arg);

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
  struct join_request jr_packet;
  struct request_response response_packet;
	pthread_t forwarding_thread_id;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if (argc != 2) {
    fprintf(stderr,"usage: master MasterPort#\n");
    exit(1);
	}

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	pthread_create(&forwarding_thread_id, NULL, forwarding_loop, NULL);

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

    if ((rv = recv(new_fd, (void *)(&jr_packet), sizeof jr_packet, 0)) == -1) {
      perror("recv");
      continue;
    }
    printf("Received Join Request\n");
		printf("GID: %d\n",jr_packet.gid);
		printf("Magic Number: %#06x\n",ntohs(jr_packet.magic_num));

		response_packet.gid = GID;
		response_packet.magic_num = htons(MAGIC_NUMBER);
		response_packet.rid = rid;
		response_packet.next_IP = htonl(next_IP);

		next_IP = ntohl(((struct in_addr*)get_in_addr((struct sockaddr *)&their_addr))->s_addr);
		printf("%#010x\n", next_IP);

    if (send(new_fd, (void *)(&response_packet), sizeof response_packet, 0) == -1) {
      perror("send");
      continue;
    }

		rid++;
		// if (!fork()) { // this is the child process
		// 	close(sockfd); // child doesn't need the listener
		// 	if (send(new_fd, "Hello, world!", 13, 0) == -1)
		// 		perror("send");
		// 	close(new_fd);
		// 	exit(0);
		// }
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}


void *forwarding_loop(void *arg) {
	int rv;
	struct addrinfo hints, *res, *p;
	int sockfd;
	char this_port_number[10];
	struct ring_message message_packet;
	size_t numbytes;
	struct sockaddr_in next_addr;

	//we will need a decimal string expressing the port number
	sprintf(this_port_number,"%d", BASE_PORT + 5*GID);
	printf("%s\n", this_port_number);
	//set up a socket
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, this_port_number, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}
	// loop through all the results and bind to the first we can
	for(p = res; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}
		break;
	}
	freeaddrinfo(res);

	printf("starting to listen for ring packets\n");
	while (1) {
		memset(&message_packet, 0, sizeof message_packet);
		//get a packet (blocking)
		if ((numbytes = recvfrom(sockfd, (void *)(&message_packet), sizeof message_packet, 0, NULL, NULL)) == -1) {
		    perror("recvfrom");
		    exit(1);
		}
		printf("\nProcessing packet...\n");
		if (ntohs(message_packet.magic_num) != MAGIC_NUMBER) {
			printf("\nPacket received with incorrect \"magic number\" value.\n");
			continue;
		}
		if ( compute_checksum((void*)(&message_packet), numbytes) != 0) {
			printf("\nPacket received with incorrect checksum.\n");
			continue;
		}

		if (message_packet.rid_dest == 0) {
			//TODO deal with packet sizes or somethign
			//set checksum to 0 so that print statement doesn't get pissed or whatever?
			((uint8_t *)(&message_packet))[numbytes-1] = '\0';
			printf("\nReceived message: %s\n", message_packet.message);
		} else if (message_packet.ttl > 1) {
			//Decrease the TTL value of the packet, and forward it to next_addr

			memset(&next_addr, 0, sizeof next_addr);
			next_addr.sin_family = AF_INET;
			next_addr.sin_port = htons(BASE_PORT + 5*GID + rid-1);
			next_addr.sin_addr.s_addr = htonl(next_IP);


			message_packet.ttl--;
			((uint8_t *)(&message_packet))[numbytes-1] = 0;
			((uint8_t *)(&message_packet))[numbytes-1] = compute_checksum((void*)(&message_packet), (sizeof message_packet) - 1);

			if ((numbytes = sendto(sockfd, (void *)(&message_packet), numbytes, 0,
					 (struct sockaddr *)(&next_addr), sizeof next_addr)) == -1) {
				perror("error forwarding packet: sendto");
				exit(1);
			}
			printf("\nForwarded packet to %x\n", next_IP);
		} else {
			printf("\nDiscarded packet.\n");
		}

	}
	printf("---exiting forwarding loop thread---\n");
	return NULL;
}
