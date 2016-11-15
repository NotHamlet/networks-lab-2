#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	struct join_request jr_packet;
	struct request_response response_packet;

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

	printf("Received Response Packet\n");
	printf("GID: %d\n", response_packet.gid);
	printf("Magic Number: %#06x\n", ntohs(response_packet.magic_num));
	printf("RID: %d\n", response_packet.rid);
	printf("Next Node IP: %#10x\n", ntohl(response_packet.next_IP));

	close(sockfd);

	return 0;
}
