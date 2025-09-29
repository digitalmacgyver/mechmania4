/* Server.h
 * Implementation of the server
 * 9/3/1998 by Misha Voloshin
 * For use with MechMania IV
 */

#ifndef _SERVER_H_DSFJKHSDFJSDLKFJLKSDFJHDFJD
#define _SERVER_H_DSFJKHSDFJSDLKFJLKSDFJHDFJD

#include "stdafx.h"

class CWorld;
class CTeam;
class CServerNet;

class CServer {
 public:
  CServer(int numTms = 2, int port = 2323);
  ~CServer();

  unsigned int GetNumTeams() const;
  double GetTime();
  CWorld *GetWorld();

  unsigned int ConnectClients();  // Return # successfully connected

  void IntroduceWorld(int conn);
  unsigned int SendWorld(int conn);
  void BroadcastWorld();  // Sends world to all open connections
  void MeetTeams();       // Gets teams from clients and sends to observer

  void ReceiveTeamOrders();  // Gives orders to local teams' ships
  void WaitForObserver();    // Waits for observer to send ack

  double Simulation();  // return game time

  // Pause control
  void SetPaused(bool p) { bPaused = p; }
  bool IsPaused() const { return bPaused; }
  void ResumeSync();

 protected:
  unsigned int nTms;      // # teams
  unsigned int *auTCons;  // Array of team connection #'s

  unsigned int ObsConn;  // Observer connection
  bool *abOpen;  // Flag to tell if connection's open

  unsigned int wldbuflen;
  char *wldbuf;  // World buffer

  CServerNet *pmyNet;
  CWorld *pmyWorld;
  CTeam **aTms;

  bool bPaused = false;
};

#endif
