/* Client.h
 * 9/3/1998 by Misha Voloshin
 * For use with MechMania IV
 */

#ifndef _CLIENT_H_DFJKSJHFSDKJFHLKJDSH
#define _CLIENT_H_DFJKSJHFSDKJFHLKJDSH

#include "stdafx.h"

class CWorld;
class CTeam;
class CClientNet;

class CClient
{
 public:
  CClient(int port, char *hostname, BOOL bObserv=FALSE);
  ~CClient();

  CWorld *GetWorld();

  void MeetWorld();
  void MeetTeams();        // Only called by observer, once
  UINT ReceiveWorld();

  int SendAck();  // Send Observer acknowledge
  void DoTurn();   // Run Team::Turn() and send orders to server
  int IsOpen();   // Returns pmyNet->IsOpen(1);

 protected:
  BOOL bObflag;

  UINT numTeams;
  UINT umyIndex;  // For client teams; doesn't do much for observer

  CClientNet *pmyNet;
  CWorld *pmyWorld;
  CTeam **aTms;
};

#endif // _CLIENT_H_DFJKSJHFSDKJFHLKJDSH
