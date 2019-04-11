#include <iostream>
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

#define PORT "8080"
#define BACKLOG 10

#define MAX 1024
#define MAX_CLIENTS 256
#define MAX_USERNAME 32

struct Client {
  int* sockfd;
  char username[MAX_USERNAME];
};

Client** clients;
pthread_mutex_t clients_mutex;

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void add_client(Client* client) {
  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] == NULL) {
      clients[i] = client;
      break;
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int* sockfd) {
  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i]->sockfd == sockfd) {
      delete clients[i];
      break;
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

void send_message_to_clients(Client* client, char msg[]) {
  char send_msg[MAX + MAX_USERNAME + 1];
  strcpy(send_msg, client->username);
  strcat(send_msg, ": ");
  strcat(send_msg, msg);

  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] != NULL && clients[i]->sockfd != client->sockfd) {
      int* sockfd = clients[i]->sockfd;
      if (send(*sockfd, send_msg, MAX+MAX_USERNAME-1, 0) == -1) {
        std::cerr << "send msg" << std::endl;
      }
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

void send_joined_message_to_clients(Client* client) {
  char joined_msg[MAX];
  strcpy(joined_msg, client->username);
  strcat(joined_msg, " joined");

  pthread_mutex_lock(&clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] != NULL && clients[i]->sockfd != client->sockfd) {
      if (send(*(clients[i]->sockfd), joined_msg, MAX-1, 0) == -1) {
        std::cerr << "send joined" << std::endl;
      }
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

void send_left_message_to_clients(Client* client) {
  pthread_mutex_lock(&clients_mutex);

  char left_msg[MAX];
  strcpy(left_msg, client->username);
  strcat(left_msg, " left");

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] != NULL && clients[i]->sockfd != client->sockfd) {
      if (send(*(clients[i]->sockfd), left_msg, MAX-1, 0) == -1) {
        std::cerr << "send left" << std::endl;
      }
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

void set_username(Client* client) {
  int numbytes;
  char username[MAX_USERNAME];

  numbytes = recv(*(client->sockfd), username, MAX_USERNAME-1, 0);

  if (numbytes == -1) {
    std::cerr << "recv user" << std::endl;
    return;
  } else if (numbytes == 0) {
    return;
  }

  username[numbytes] = '\0';
  std::cout << "server: user " << username << std::endl;
  strcpy(client->username, username);
}

void *client_handler(void* sock) {
  int* sockfd = (int*) sock;
  int numbytes;
  char msg[MAX];

  Client* client = new Client();
  client->sockfd = sockfd;
  set_username(client);
  add_client(client);
  send_joined_message_to_clients(client);

  while(1) {
    numbytes = recv(*sockfd, msg, MAX-1, 0);

    if (numbytes == -1) {
      std::cerr << "recv msg" << std::endl;
      break;
    } else if (numbytes == 0) {
      break;
    }

    msg[numbytes] = '\0';
    std::cout << "server: received " << msg << std::endl;

    send_message_to_clients(client, msg);
  }

  std::cout << "server: closing socket " << *sockfd << std::endl;
  close(*sockfd);
  send_left_message_to_clients(client);
  remove_client(sockfd);
  pthread_exit(NULL);
}

void create_and_bind_socket(int* sockfd) {
  struct addrinfo hints, *servinfo, *p;
  int yes = 1;
  int rv;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    exit(1);
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      std::cerr << "server: socket" << std::endl;
      continue;
    }

    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
      std::cerr << "setsockopt" << std::endl;
      exit(1);
    }

    if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(*sockfd);
      std::cerr << "server: bind" << std::endl;
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    std::cerr << "server: failed to bind" << std::endl;
    exit(1);
  }
}

void init_clients() {
  pthread_mutex_lock(&clients_mutex);
  clients = new Client*[MAX_CLIENTS];
  pthread_mutex_unlock(&clients_mutex);
}

void deinit_clients() {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] != NULL) {
      delete clients[i]->sockfd;
      delete clients[i];
    }
  }

  delete[] clients;
  pthread_mutex_unlock(&clients_mutex);
}

int main() {
  int sockfd;
  create_and_bind_socket(&sockfd);

  if (listen(sockfd, BACKLOG) == -1) {
    std::cerr << "listen" << std::endl;
    exit(1);
  }

  std::cout << "server: waiting for new connections..." << std::endl;

  init_clients();

  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  char s[INET6_ADDRSTRLEN];
  while(1) {
    int* new_fd = new int;
    sin_size = sizeof(their_addr);
    if ((*new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
      std::cerr << "accept" << std::endl;
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof(s));
    std::cout << "server: got connection from " << s << std::endl;

    pthread_t client;
    pthread_create(&client, NULL, client_handler, new_fd);
  }

  deinit_clients();

  return 0;
}
