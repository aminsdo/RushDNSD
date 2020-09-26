#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "network_wrapper.h"

int check_ip(char *ip)
{
  uint16_t tmp[8] = {0};
  if (!ip)
    return -1;
  for (size_t i = 0; ip[i]; ++i)
    if (ip[i] == '.')
      return inet_pton(AF_INET, ip, tmp);
    else if (ip[i] == ':' && inet_pton(AF_INET6, ip, tmp) == 1)
      return 2;

  return -1;
}

dns_engine* init_serv(dns_engine *engine, char *ip, uint16_t port)
{
  size_t len, nbip;
  int *sockets;

  if (!ip)
    return NULL;

  for (len = 0; ip[len]; ++len)
    if (ip[len] == ',')
      ++nbip;
  engine->ip = malloc(sizeof(char*) * nbip);
  if (!engine->ip)
    return NULL;

  engine->ip[0] = ip;
  size_t j = 1, i = 1;
  for (; j < nbip - 2; ++i)
    if (ip[i] == ','){
      ip[i] = 0;
      engine->ip[j++] = ip + i + 1;
    }
  engine->ip[j] = ip + i;

  engine->nbip = nbip;
  for (i = 0; i < nbip; ++i)
    if (check_ip(engine->ip[i]) < 1){
      free(engine->ip);
      engine->ip = NULL;
      engine->nbip = 0;
      return NULL;
    }

  sockets = malloc(sizeof(int) * nbip);
  if (!sockets){
    free(engine->ip);
    engine->ip = NULL;
    engine->nbip = 0;
    return NULL;
  }

  int *epfds = malloc(sizeof(int) * nbip);
  if (!epfds){
    free(sockets);
    engine->sockets= NULL;
    free(engine->ip);
    engine->ip = NULL;
    engine->nbip = 0;
    return NULL;
  }

  int *nbfd = malloc(sizeof(int) * nbip);
  if (!nbfd){
    free(epfds);
    free(sockets);
    engine->sockets= NULL;
    free(engine->ip);
    engine->ip = NULL;
    engine->nbip = 0;
    return NULL;
  }

  struct epoll_event *ep_events = malloc(sizeof(struct epoll_event) * nbip);
  if (!ep_events){
    free(nbfd);
    free(epfds);
    free(sockets);
    engine->sockets= NULL;
    free(engine->ip);
    engine->ip = NULL;
    engine->nbip = 0;
    return NULL;
  }

  struct epoll_event **events = malloc(sizeof(struct epoll_event*) * nbip);

  int ipcheck, domain, error = 0;
  struct sockaddr_in addr;
  addr.sin_port = htons(port);
  for (i = 0; i < nbip; ++i)
  {
    ipcheck = check_ip(engine->ip[i]);
    domain = ipcheck == 1? AF_INET : AF_INET6;
    sockets[i] = socket(domain, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = domain;
    addr.sin_addr.s_addr = inet_addr(engine->ip[i]);
    if (bind(sockets[i], (struct sockaddr*)&addr, sizeof(addr))){
      error = 1;
      break;
    }
    if (listen(sockets[i], BACKLOG)){
      error = 1;
      break;
    }
    if ((epfds[i] = epoll_create(MAXEPOLLSIZE)) == -1){
      error = 1;
      break;
    }
    ep_events[i].events = EPOLLIN | EPOLLET;
    ep_events[i].data.fd = sockets[i];
    if (epoll_ctl(epfds[i], EPOLL_CTL_ADD, sockets[i], ep_events + i) < 0){
      error = 1;
      break;
    }
     events[i] = calloc(MAXEPOLLSIZE, sizeof(ep_events[i]));
     if (!events[i]){
       error = 1;
       break;
     }
     nbfd[i] = 1;
  }

  if (error){
    for (j=0; j <= i; ++j){
      close(sockets[j]);
      if (j < i)
        free(events[j]);
    }
    free(events);
    free(ep_events);
    free(epfds);
    free(nbfd);
    free(engine->sockets);
    engine->sockets = NULL;
    free(engine->ip);
    engine->ip = NULL;
    engine->nbip = 0;
    return NULL;
  }
  engine->nbfd = nbfd;
  engine->epfds = epfds;
  engine->ep_events = ep_events;
  engine->events = events;
  engine->sockets = sockets;
  engine->port = port;
  return engine;
}



ssize_t dns_get(char **ptr, int socket)
{
  char *buf = malloc(sizeof(char) * 4096);
  ssize_t size = 0;
  size = read(socket, buf, 4096);
  if (size <= 0)
  {
    free(buf);
    buf = NULL;
  }
  *ptr = buf;
  return size;
}

ssize_t dns_send(int socket, char *pck, size_t size)
{
  ssize_t written = write(socket, pck, size);
  return written;
}

void close_serv(dns_engine *engine)
{
  for (int i=0; i < engine->nbip; ++i)
    close(engine->sockets[i]);
  free(engine->sockets);
  engine->sockets = NULL;
  free(engine->ip);
  engine->ip = NULL;
  engine->nbip = 0;
}

