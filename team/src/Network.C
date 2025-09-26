//
// CNetwork
//
// base networking class.
//
// connection numbers start at 1

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Network.h"

using namespace std;

int CNetwork::fd2conn(int fd) {
  int i;

  for (i = 0; i < maxconn; i++) {
    if (fds[i] == fd) {
      return i + 1;
    }
  }

  return -1;
}

int CNetwork::NewConn(int fd) {
  if (next_conn >= maxconn)
    return -1;

  max_fd = (fd > max_fd) ? fd : max_fd;

  fds[next_conn] = fd;
  next_conn++;

  FD_SET(fd, &sockets_fds);

  return next_conn;
}

void CNetwork::CloseConn(int conn) {
  close(fds[conn - 1]);
  FD_CLR(fds[conn - 1], &sockets_fds);
  fds[conn - 1] = 0;
  timeout[conn - 1] = 0;
}

CNetwork::CNetwork(int themaxconn, int thequeuelen) {
  maxconn = themaxconn;
  maxqlen = thequeuelen;

  int i;

  fds = new int[maxconn];
  timeout = new int[maxconn];

  queuelen = new int[maxconn];
  queuebuf = new char[maxqlen];
  queue = new char *[maxconn];

  for (i = 0; i < maxconn; i++) {
    fds[i] = 0;
    timeout[i] = -1;
    queuelen[i] = 0;
    queue[i] = new char[maxqlen];  // Created the queues
  }

  FD_ZERO(&sockets_fds);
  max_fd = 0;
  next_conn = 0;
}

CNetwork::~CNetwork() {
  delete[] fds;
  delete[] timeout;
  delete[] queuelen;
  delete[] queuebuf;

  for (int i = 0; i < maxconn; i++) {
    delete[] queue[i];
  }
  delete[] queue;
}

int CNetwork::SendPkt(int conn, const char *data, int len) {
  // FIXME: should check for write errors
  write(fds[conn - 1], data, len);
  return 0;
}

int CNetwork::RecvPkt(char *data, int &len) {
  fd_set r_fds, e_fds;
  int v, fd, rd_len;
  int conn;

  struct timeval timeout = {5, 0};  // default to 5 second timeout

  r_fds = e_fds = sockets_fds;

  v = select(max_fd + 1, &r_fds, NULL, &e_fds, &timeout);

  if (!v)
    return -1;

  for (fd = 0; fd <= max_fd; fd++) {
    if (FD_ISSET(fd, &e_fds)) {
      len = 0;
      data[0] = 0;
      conn = fd2conn(fd);
      CloseConn(conn);

      return conn;
    }

    if (FD_ISSET(fd, &r_fds)) {
      conn = fd2conn(fd);

      if ((rd_len = read(fd, data, len - 1)) <= 0) {
        len = 0;
        data[0] = 0;
        CloseConn(conn);

        return conn;
      }

      data[rd_len] = '\0';
      len = rd_len;

      return conn;
    }
  }
  return -1;
}

void CNetwork::SetTimeout(int conn, int thetimeout) {
  timeout[conn - 1] = thetimeout;
}

int CNetwork::IsOpen(int conn) { return fds[conn - 1]; }

/////////////////////////////////////////////////////////////
// Queue-related stuff

int CNetwork::CatchPkt() {
  int conn, left, len = maxqlen;

  conn = RecvPkt(queuebuf, len);
  // printf ("%d bytes on %d\n",len,conn);
  // Remember, first conn is 1, not 0

  if (len < 1)
    return (-conn);
  if (conn < 1 || conn > maxconn)
    return conn;
  // Sloppy error reporting, but it'll do for now

  left = maxqlen - queuelen[conn - 1];
  if (left < len)
    len = left;
  if (len <= 0)
    return conn;

  memcpy(queue[conn - 1] + queuelen[conn - 1], queuebuf, len);
  queuelen[conn - 1] += len;

  return conn;
}

int CNetwork::GetQueueLength(int conn) {
  if (conn < 1 || conn > maxconn)
    return -1;
  return queuelen[conn - 1];
}

char *CNetwork::GetQueue(int conn) {
  if (conn < 1 || conn > maxconn)
    return NULL;
  return queue[conn - 1];
}

void CNetwork::FlushQueue(int conn) {
  if (conn < 1 || conn > maxconn)
    return;
  queuelen[conn - 1] = 0;
}
