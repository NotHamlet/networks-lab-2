#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#include <arpa/inet.h>

#include "Slave.h"

#define MESSAGE "ONE SMALL STEP"

#define MAXDATASIZE 100 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *forwarding_loop(void *);

void *sending_loop(void *);

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	struct join_request jr_packet;
	struct request_response response_packet;
	uint8_t gid;
	uint16_t magic_num;
	uint8_t this_rid;
	uint32_t next_IP;

	struct thread_args thr_args;
	pthread_t forwarding_thread_id;
	pthread_t sending_thread_id;


	if (argc != 3) {
    fprintf(stderr,"usage: slave MasterHostname MasterPort#\n");
    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	//First, we send a join request
	jr_packet.gid = GID;
	jr_packet.magic_num = htons(MAGIC_NUMBER);
	if (send(sockfd, (void *)(&jr_packet), sizeof jr_packet, 0) == -1) {
		perror("send");
		exit(1);
	}

	//Then, we wait for a response and set up our junk
	if ((numbytes = recv(sockfd, (void *)(&response_packet), sizeof response_packet, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	if (numbytes != (sizeof response_packet)) {
		printf("Error: Server response was incorrect size.");
		exit(1);
	}

	gid = response_packet.gid;
	magic_num = ntohs(response_packet.magic_num);
	this_rid = response_packet.rid;
	next_IP = ntohl(response_packet.next_IP);

	if (magic_num != MAGIC_NUMBER) {
		printf("Error: Server response contained wrong \"magic number\" value.");
		exit(1);
	}

	//TODO set up forwarding service
	//TODO set up sending service

	printf("Received Response Packet\n");
	printf("GID: %d\n", gid);
	printf("Magic Number: %#06x\n", magic_num);
	printf("RID: %d\n", this_rid);
	printf("Next Node IP: %#10x\n", next_IP);

	close(sockfd);

	//TODO construct thread argument structs
	thr_args.next_IP = next_IP;
	thr_args.this_rid = this_rid;
	thr_args.gid = gid;

	pthread_create(&forwarding_thread_id, NULL, forwarding_loop, &thr_args);
	pthread_create(&sending_thread_id, NULL, sending_loop, &thr_args);

	pthread_join(forwarding_thread_id, NULL);
	pthread_join(sending_thread_id, NULL);
	printf("exiting program\n");
	return 0;
}

void *forwarding_loop(void *arg) {
	struct thread_args *args = (struct thread_args *)arg;
	struct addrinfo hints, *res;
	int sockfd;
	char port_number[10];
	struct ring_message message_packet;
	int byteCount;

	//TODO remove this
	char buf[100];

	//we will need a decimal string expressing the port number
	sprintf(port_number,"%d", BASE_PORT + 5*args->gid + (args->this_rid));
	printf("%s\n", port_number);

	//set up a socket
	hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, port_number, &hints, &res);
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	bind(sockfd, res->ai_addr, res->ai_addrlen);

	while (1) {
		memset(&message_packet, 0, sizeof message_packet);
		memset(buf, 0, 100);
		//get a packet
		recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
		buf[100] = '\0';
		printf("%s\n", buf);

	}
	printf("---exiting forwarding loop thread---\n");
	return NULL;
}

void *sending_loop(void *arg) {
	struct thread_args *args = (struct thread_args *)arg;

	while (0) {

	}
	printf("---exiting send loop thread---\n");
	return NULL;
}
