#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT "8080"
#define MAX 1024
#define MAX_USERNAME 32

pthread_mutex_t sockfd_mutex;
char* username;

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void create_and_connect_socket(int* sockfd, char* hostname) {
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(hostname, SERVER_PORT, &hints, &servinfo)) != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    exit(1);
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      std::cerr << "client: seerver" << std::endl;
      continue;
    }

    if (connect(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(*sockfd);
      std::cerr << "client: connect" << std::endl;
      continue;
    }

    break;
  }

  if (p == NULL) {
    std::cerr << "client: failed to connect" << std::endl;
    exit(1);
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
  std::cout << "client: connecting to " << s << std::endl;

  freeaddrinfo(servinfo);
}

void *input_handler(void* sock) {
  int* sockfd = (int*) sock;

  pthread_mutex_lock(&sockfd_mutex);

  if (send(*sockfd, username, MAX_USERNAME-1, 0) == -1) {
    std::cerr << "send" << std::endl;
  }
  pthread_mutex_unlock(&sockfd_mutex);

  while(1) {
    char send_msg[MAX];

    std::cin.getline(send_msg, MAX);

    if (strcmp(send_msg, ":q") == 0) {
      break;
    }

    pthread_mutex_lock(&sockfd_mutex);
    if (send(*sockfd, send_msg, MAX-1, 0) == -1) {
      std::cerr << "send" << std::endl;
    }
    pthread_mutex_unlock(&sockfd_mutex);
  }

  return 0;
}

void *output_handler(void* sock) {
  int* sockfd = (int*) sock;

  while(1) {
    char recv_msg[MAX];
    int numbytes;

    pthread_mutex_lock(&sockfd_mutex);
    numbytes = recv(*sockfd, recv_msg, MAX+MAX_USERNAME-1, 0);
    pthread_mutex_unlock(&sockfd_mutex);

    if (numbytes == -1) {
      std::cerr << "recv" << std::endl;
      break;
    } else if (numbytes == 0) {
      break;
    }

    recv_msg[numbytes] = '\0';
    std::cout << recv_msg << std::endl;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: client username hostname" << std::endl;
    exit(1);
  }

  username = new char[MAX_USERNAME];
  username = argv[1];

  char* hostname = NULL;
  if (argc == 3) {
    hostname = argv[2];
  }

  int* sockfd = new int;
  create_and_connect_socket(sockfd, hostname);

  pthread_t input, output;
  pthread_create(&input, NULL, input_handler, sockfd);
  pthread_create(&output, NULL, output_handler, sockfd);

  pthread_join(input, NULL);
  pthread_cancel(output);

  std::cout << "close" << std::endl;
  close(*sockfd);

  delete sockfd;

  return 0;
}
