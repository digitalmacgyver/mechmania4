/* Server.C
 * Implementation of CServer class
 * 9/3/1998 Misha Voloshin
 * MechMania IV
 */

#include "Server.h"
#include "ServerNet.h"
#include "Ship.h"
#include "Team.h"
#include "World.h"
#include "GameConstants.h"

///////////////////////////////////////////
// Construction/Destruction

CServer::CServer(int numTms, int port) {
  unsigned int i;

  nTms = numTms;
  ObsConn = (unsigned int)-1;

  pmyNet = new CServerNet(nTms + 1, port);
  pmyWorld = new CWorld(nTms);

  abOpen = new bool[nTms + 1];
  for (i = 0; i < nTms + 1; ++i) {
    abOpen[i] = false;  // No connections there yet
  }

  auTCons = new unsigned int[nTms];
  aTms = new CTeam *[nTms];
  for (i = 0; i < nTms; ++i) {
    aTms[i] = CTeam::CreateTeam();
    auTCons[i] = (unsigned int)-1;
    aTms[i]->SetTeamNumber(i);
    aTms[i]->Create(g_initial_team_ship_count, i);
    pmyWorld->SetTeam(i, aTms[i]);
  }

  pmyWorld->CreateAsteroids(VINYL, g_initial_vinyl_asteroid_count,
                            g_initial_asteroid_mass);
  pmyWorld->CreateAsteroids(URANIUM, g_initial_uranium_asteroid_count,
                            g_initial_asteroid_mass);
  pmyWorld->PhysicsModel(0.0);  // Add new stuff

  wldbuflen = MAX_THINGS * 256;
  wldbuf = new char[wldbuflen];
  memset(wldbuf, 0, wldbuflen);

  printf("World created, %d teams initialized\n", nTms);
  printf("Ready for connections on port %d\n", port);
}

CServer::~CServer() {
  delete[] wldbuf;

  delete[] abOpen;
  delete[] auTCons;

  delete pmyWorld;
  for (unsigned int i = 0; i < nTms; ++i) {
    delete aTms[i];
  }
  delete[] aTms;

  delete pmyNet;
}

////////////////////////////////////////
// Data access

unsigned int CServer::GetNumTeams() const { return nTms; }

double CServer::GetTime() { return pmyWorld->GetGameTime(); }

CWorld *CServer::GetWorld() { return pmyWorld; }

////////////////////////////////////////
// Methods

unsigned int CServer::ConnectClients() {
  int conn;
  char outbuf[2048];

  for (unsigned int i = 0; i < GetNumTeams() + 1; ++i) {  // Teams and observer
    conn = pmyNet->WaitForConn();
    abOpen[conn - 1] = true;
    printf("Establishing connection #%d\n", conn);

    // Tell them they've connected
    snprintf(outbuf, sizeof(outbuf), n_servconack);
    pmyNet->SendPkt(conn, outbuf, strlen(outbuf));
  }

  // They've all linked up, now who the hell are they?
  int totcl = 0, tmindex = 0;
  int slen = strlen(n_obcon);  // n_obcon and n_teamcon same length

  while ((unsigned int)totcl < GetNumTeams() + 1) {
    conn = pmyNet->CatchPkt();
    if (pmyNet->GetQueueLength(conn) < slen) {
      continue;
    }

    totcl++;  // It responded, whoever the hell it is
    if (memcmp(pmyNet->GetQueue(conn), n_obcon, slen) == 0) {
      ObsConn = conn;  // That's our observer
      snprintf(outbuf, sizeof(outbuf),
               "X");  // Dummy character, eases clientside parsing
      pmyNet->SendPkt(conn, outbuf, 1);
    }

    if (memcmp(pmyNet->GetQueue(conn), n_teamcon, slen) == 0) {
      if ((unsigned int)tmindex >= GetNumTeams()) {
        continue;  // Who are all these people!?
      }
      auTCons[tmindex] = conn;
      snprintf(outbuf, sizeof(outbuf), "%c", tmindex);
      pmyNet->SendPkt(conn, outbuf, 1);
      tmindex++;
    }

    pmyNet->FlushQueue(conn);
    IntroduceWorld(conn);  // Tell it about its world
  }

  return GetNumTeams() + 1;
}

void CServer::IntroduceWorld(int conn) {
  char buf[4];  // Larger than probably needed

  // Treats these numbers as 8 bit numbers - this works as long as they are 255
  // or less.
  buf[0] = (char)GetNumTeams();
  buf[1] = (char)aTms[0]->GetShipCount();

  pmyNet->SendPkt(conn, buf, 2);
  return;
}

unsigned int CServer::SendWorld(int conn) {
  if (abOpen[conn - 1] != true) {
    return 0;
  }
  if (pmyNet->IsOpen(conn) == 0) {
    abOpen[conn - 1] = false;
    printf("Lost connection %d\n", conn);
    return 0;
  }

  unsigned int lenpred, lenact, netsize;

  lenpred = pmyWorld->GetSerialSize();
  if (lenpred > wldbuflen || lenpred <= 0) {
    return 0;
  }
  lenact = pmyWorld->SerialPack(wldbuf, wldbuflen);

  if (lenact != lenpred) {  // Didn't predict right, something's wrong
    printf("Serialization error\n");
    return 0;
  }

  netsize = htonl(lenact);
  pmyNet->SendPkt(conn, (char *)(&netsize), sizeof(unsigned int));
  pmyNet->SendPkt(conn, wldbuf, lenact);
  return lenact;
}

void CServer::BroadcastWorld() {
  if (bPaused) {
    // While paused, avoid waking teams; observer gets updates elsewhere
    return;
  }
  for (unsigned int conn = 1; conn <= GetNumTeams() + 1; ++conn) {
    if (conn == ObsConn) {
      continue;  // Observer gets world elsewhere
    }
    if (abOpen[conn - 1] != true) {
      continue;  // This connection closed, next!
    }

    if (pmyNet->IsOpen(conn) == 0) {
      continue;  // Don't send to closed socket
    }

    SendWorld(conn);
  }

  for (unsigned int tm = 0; tm < GetNumTeams(); ++tm) {
    pmyWorld->atstamp[tm] = pmyWorld->GetTimeStamp();
    // They got the world, start counting!
  }
}

void CServer::ResumeSync() {
  // Reset team timing
  double now = pmyWorld->GetTimeStamp();
  for (unsigned int tm = 0; tm < GetNumTeams(); ++tm) {
    pmyWorld->atstamp[tm] = now;
  }
  // Clear per-step flags without advancing time
  pmyWorld->PhysicsModel(0.0);
  // Push a fresh world snapshot to all teams even if paused was engaged
  for (unsigned int conn = 1; conn <= GetNumTeams() + 1; ++conn) {
    if (conn == ObsConn) {
      continue;
    }
    if (abOpen[conn - 1] != true) {
      continue;
    }
    if (pmyNet->IsOpen(conn) == 0) {
      continue;
    }
    SendWorld(conn);
  }
}

void CServer::WaitForObserver() {
  // Don't wait for a disconnected server
  if (abOpen[ObsConn - 1] == false) {
    return;
  }

  unsigned int len;
  char *pq;

  while (true) {
    while ((len = pmyNet->GetQueueLength(ObsConn)) < strlen(n_oback)) {
      pmyNet->CatchPkt();

      // Check if connection closed
      if (pmyNet->IsOpen(ObsConn) == 0) {
        abOpen[ObsConn - 1] = false;
        printf("Observer disconnected\n");
        return;
      }
    }

    // Refresh pointer to queue for current contents
    pq = pmyNet->GetQueue(ObsConn);

    // Check for control commands from observer
    if (len >= (int)strlen(n_pause) &&
        memcmp(pq, n_pause, strlen(n_pause)) == 0) {
      SetPaused(true);
      pmyNet->FlushQueue(ObsConn);
      printf("Observer requested PAUSE\n");
      continue;  // Keep waiting for ack
    }
    if (len >= (int)strlen(n_resume) &&
        memcmp(pq, n_resume, strlen(n_resume)) == 0) {
      SetPaused(false);
      pmyNet->FlushQueue(ObsConn);
      printf("Observer requested RESUME\n");
      // Perform resume-safe sync: reset team timers, settle flags, and push
      // snapshot
      ResumeSync();
      continue;  // Keep waiting for ack
    }

    if (memcmp(pq, n_oback, strlen(n_oback)) == 0) {
      break;  // Yay!  It's the ack!
    }

    pmyNet->FlushQueue(ObsConn);  // Whatever it was, it was wrong
  }

  pmyNet->FlushQueue(ObsConn);
}

void CServer::MeetTeams() {
  int conn;
  unsigned int len, tn, totresp = 0;
  char *buf;
  bool *abGotFlag = new bool[GetNumTeams()];

  for (tn = 0; tn < GetNumTeams(); ++tn) {
    abGotFlag[tn] = false;
  }

  while (totresp < GetNumTeams()) {
    for (tn = 0; tn < GetNumTeams(); ++tn) {
      if (abGotFlag[tn] == true) {
        continue;
      }

      conn = auTCons[tn];
      len = pmyNet->GetQueueLength(conn);
      if (len >= aTms[tn]->GetSerInitSize()) {
        totresp++;
        abGotFlag[tn] = true;
      }
    }

    if (totresp >= GetNumTeams()) {
      break;
    }
    pmyNet->CatchPkt();
  }

  for (tn = 0; tn < GetNumTeams(); ++tn) {
    conn = auTCons[tn];
    len = pmyNet->GetQueueLength(conn);
    buf = pmyNet->GetQueue(conn);

    aTms[tn]->SerUnpackInitData(buf, len);
    len = aTms[tn]->GetSerInitSize();  // Make *sure* length is right
    WaitForObserver();
    pmyNet->SendPkt(ObsConn, buf, len);  // And send to observer

    pmyNet->FlushQueue(conn);
  }

  delete[] abGotFlag;
}

void CServer::ReceiveTeamOrders() {
  if (bPaused) {
    // While paused, still service observer traffic and refresh world view
    WaitForObserver();
    SendWorld(ObsConn);
    return;
  }
  int conn, len;
  unsigned int tn, totresp = 0;
  char *buf;
  bool *abGotFlag = new bool[GetNumTeams()];
  double tstart, tnow, tobs;
  double timediff, tthink;

  for (tn = 0; tn < GetNumTeams(); ++tn) {
    aTms[tn]->Reset();
    abGotFlag[tn] = false;
  }

  tstart = pmyWorld->GetTimeStamp();  // Teams can't take >60sec/turn to respond
  tobs = tstart;  // The observer should receive updates just in case
  while (totresp < GetNumTeams()) {
    tnow = pmyWorld->GetTimeStamp();
    timediff = tnow - tobs;
    if (timediff >= 5.0) {  // Server updates every 5 seconds
      WaitForObserver();
      SendWorld(ObsConn);
      tobs = tnow;
    }

    for (tn = 0; tn < GetNumTeams(); ++tn) {
      if (abGotFlag[tn] == true) {
        continue;  // Already counted
      }

      conn = auTCons[tn];
      if (abOpen[conn - 1] != true) {
        totresp++;             // Skip over this one, count as gotten
        abGotFlag[tn] = true;  // Pretend we got it
        continue;              // But don't actually compute stuff
      }

      if (pmyNet->IsOpen(conn) == 0) {
        abOpen[conn - 1] = false;
        printf("%s disconnected\n",
               aTms[tn] ? aTms[tn]->GetName() : "Unknown Team");
        continue;
      }

      // Team's still thinking, handle time counting here
      timediff = tnow - pmyWorld->atstamp[tn];
      if (aTms[tn]->GetWallClock() == 0.0) {
        timediff = 0.01;  // Just began game, something small
      }
      pmyWorld->auClock[tn] += timediff;
      pmyWorld->atstamp[tn] = tnow;
      if (aTms[tn]->GetWallClock() > 300.0) {
        printf("%s timed out, severing connection\n",
               aTms[tn] ? aTms[tn]->GetName() : "Unknown Team");
        pmyNet->CloseConn(conn);  // Close connection, will skip over
        continue;                 // Check other teams
      }

      tthink = tnow - tstart;
      if (tthink > 60.0) {
        printf("%s taking too long, orders ignored\n",
               aTms[tn] ? aTms[tn]->GetName() : "Unknown Team");
        totresp++;             // Pretend it's responded
        abGotFlag[tn] = true;  // Pretend it's responded
        continue;              // And keep chugging
      }

      len = pmyNet->GetQueueLength(conn);
      if ((unsigned int)len >= aTms[tn]->GetSerialSize()) {
        totresp++;
        abGotFlag[tn] = true;

        buf = pmyNet->GetQueue(conn);
        aTms[tn]->SerialUnpack(buf, len);  // Ships get orders
        pmyNet->FlushQueue(conn);
      }
    }

    if (totresp >= GetNumTeams()) {
      break;
    }
    pmyNet->CatchPkt();
  }

  pmyWorld->PhysicsModel(0.0);
  delete[] abGotFlag;
}

double CServer::Simulation() {
  if (bPaused) {
    // Don't advance physics or lasers, just keep observer connection alive
    WaitForObserver();
    SendWorld(ObsConn);
    return GetTime();
  }

  // Use an integer step counter so the number of physics ticks is immune to
  // floating-point accumulation error from "t += tstep" comparisons. In older
  // builds the final iteration could be skipped if rounding nudged t past maxt.
  // This block retains the desired behavior of always running the loop at least
  // once, even if tstep >= maxt.
  int stepCount = 0;
  if (g_game_turn_duration > 0.0 && g_physics_simulation_dt > 0.0) {
    stepCount = static_cast<int>(g_game_turn_duration / g_physics_simulation_dt);
    if (static_cast<double>(stepCount) * g_physics_simulation_dt <
        g_game_turn_duration) {
      stepCount++;
    }
    if (stepCount <= 0) {
      stepCount = 1;
    }
  }

  for (int step = 0; step < stepCount; ++step) {
    // Calculate turn_phase: progress at START of this sub-tick [0.0, 1.0)
    // For 5 steps (dt=0.2): phases are 0.0, 0.2, 0.4, 0.6, 0.8 (not including 1.0)
    // Special case: if stepCount==1 (dt >= turn length), phase = 0.0
    double turn_phase = (stepCount > 0) ? ((double)step / (double)stepCount) : 0.0;
    pmyWorld->PhysicsModel(g_physics_simulation_dt, turn_phase);
    if (step == stepCount - 1) {
      pmyWorld->LaserModel();
    }

    WaitForObserver();
    SendWorld(ObsConn);

    for (unsigned int tm = 0; tm < nTms; ++tm) {
      aTms[tm]->MsgText[0] = 0;
    }
    // Clear announcer messages after each frame
    pmyWorld->AnnouncerText[0] = 0;
  }

  // Increment turn counter after physics completes
  pmyWorld->IncrementTurn();

  return GetTime();

  /*
  // Legacy floating-point loop retained for historical reference:
  double t, maxt=g_game_turn_duration, tstep=g_physics_simulation_dt;
  unsigned int tm;
  for (t=0.0; t<maxt; t+=tstep) {
    pmyWorld->PhysicsModel (tstep);
    if (t>=maxt-tstep) {
      pmyWorld->LaserModel();
    }

    WaitForObserver();
    SendWorld(ObsConn);

    for (tm=0; tm<nTms; ++tm) {
      aTms[tm]->MsgText[0]=0;
    }
  }

  return GetTime();
  */
}

///////////////////////////////////
// Protected methods
