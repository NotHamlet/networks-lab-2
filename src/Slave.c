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
	int rv;
	struct thread_args *args = (struct thread_args *)arg;
	struct addrinfo hints, *res, *p;
	int sockfd;
	char this_port_number[10];
	struct ring_message message_packet;
	int numbytes;
	uint8_t checksum;
	struct sockaddr_in next_addr;

	//we will need a decimal string expressing the port number
	sprintf(this_port_number,"%d", BASE_PORT + 5*args->gid + (args->this_rid));
	printf("%s\n", this_port_number);
	//set up a socket
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

	memset(&next_addr, 0, sizeof next_addr);
	next_addr.sin_family = AF_INET;
	next_addr.sin_port = htons(BASE_PORT + 5*args->gid + (args->this_rid-1));
	next_addr.sin_addr.s_addr = htonl(args->next_IP);

	while (1) {
		memset(&message_packet, 0, sizeof message_packet);
		//get a packet (blocking)
		if ((numbytes = recvfrom(sockfd, (void *)(&message_packet), sizeof message_packet, 0, NULL, NULL)) == -1) {
		    perror("recvfrom");
		    exit(1);
		}
		printf("\nProcessing packet...\n");
		if ((numbytes != (sizeof message_packet))) {
			printf("\nPacket received with incorrect length.\n");
			continue;
		}
		if (message_packet.magic_num != MAGIC_NUMBER) {
			printf("\nPacket received with incorrect \"magic number\" value.\n");
			continue;
		}
		checksum = compute_checksum((void *)(&message_packet), (sizeof message_packet)-1);
		if ( checksum != message_packet.checksum) {
			printf("\nPacket received with incorrect checksum. Needed %#x, found %#x.\n", checksum, message_packet.checksum);
			continue;
		}

		if (message_packet.rid_dest == args->this_rid) {
			//TODO deal with packet sizes or somethign
			printf("Received message: %s\n", message_packet.message);
		} else if (message_packet.ttl > 1) {
			//Decrease the TTL value of the packet, and forward it to next_addr
			message_packet.ttl--;
			if ((numbytes = sendto(sockfd, (void *)(&message_packet), sizeof message_packet, 0,
					 (struct sockaddr *)(&next_addr), sizeof next_addr)) == -1) {
				perror("error forwarding packet: sendto");
				exit(1);
			}
			//TODO remove this?
			printf("\nForwarded packet.\n");
		} else {
			printf("\nDiscarded packet.\n");
		}

	}
	printf("---exiting forwarding loop thread---\n");
	return NULL;
}

void *sending_loop(void *arg) {
	struct thread_args *args = (struct thread_args *)arg;
	struct sockaddr_in next_addr;
	int sockfd;
	size_t numbytes;
	struct ring_message message_packet;
	int dest_rid;
	char *line = NULL;

	//get a socket for the outgoing messages
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("send: socket");
		exit(1);
	}

	//initialize struct for next node's address info
	memset(&next_addr, 0, sizeof next_addr);
	next_addr.sin_family = AF_INET;
	next_addr.sin_port = htons(BASE_PORT + 5*args->gid + (args->this_rid-1));
	next_addr.sin_addr.s_addr = htonl(args->next_IP);

	while (1) {
		printf("Input ring ID of destination:");
		fflush(stdout);
		getline(&line, &numbytes, stdin);
		if (sscanf(line, "%d", &dest_rid) < 1) {
			continue;
		}
		printf("Input message to send:");
		fflush(stdout);
		getline(&line, &numbytes, stdin);
		//TODO is this the right call? Do we need to subtract one to account for the null character?
		if (strlen(line) > MAX_MESSAGE_LENGTH) {
			printf("Message too long.\n");
		} else {
			memset(&message_packet, 0, sizeof message_packet);
		  message_packet.gid = args->gid;;
		  message_packet.magic_num = MAGIC_NUMBER;
		  message_packet.ttl = INITIAL_TTL;
		  message_packet.rid_source = args->this_rid;
		  message_packet.rid_dest = dest_rid;
			strcpy(message_packet.message, line);
			message_packet.checksum = compute_checksum((void*)(&message_packet), (sizeof message_packet) - 1);

			if ((numbytes = sendto(sockfd, (void *)(&message_packet), sizeof message_packet, 0,
					 (struct sockaddr *)(&next_addr), sizeof next_addr)) == -1) {
				perror("error sending packet: sendto");
				exit(1);
			}

			printf("message sent. %s %d\n",message_packet.message, numbytes);
		}
		free(line);
		line = NULL;
	}
	printf("---exiting send loop thread---\n");
	return NULL;
}
