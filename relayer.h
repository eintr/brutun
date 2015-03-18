#ifndef RELAYER_H
#define RELAYER_H

void relay(int sd, int tunfd, struct sockaddr_in *peer_addr);

#endif

