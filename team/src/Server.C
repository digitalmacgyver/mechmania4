/* Server.C
 * Implementation of CServer class
 * 9/3/1998 Misha Voloshin
 * MechMania IV
 */

#include "Server.h"
#include "World.h"
#include "Team.h"
#include "Ship.h"
#include "ServerNet.h"

///////////////////////////////////////////
// Construction/Destruction

CServer::CServer(int numTms, int port)
{
  UINT i;

  nTms=numTms;
  ObsConn=(UINT)-1;

  pmyNet = new CServerNet(nTms+1, port);
  pmyWorld = new CWorld(nTms);
  
  abOpen = new bool[nTms+1];
  for (i=0; i<nTms+1; i++)
    abOpen[i]=false;  // No connections there yet

  auTCons = new UINT[nTms];
  aTms = new CTeam*[nTms];
  for (i=0; i<nTms; i++) {
    aTms[i] = CTeam::CreateTeam();
    auTCons[i]=(UINT)-1;
    aTms[i]->SetTeamNumber(i);
    aTms[i]->Create(4,i);
    pmyWorld->SetTeam(i,aTms[i]);
  }

  pmyWorld->CreateAsteroids(VINYL, 5, 40.0);
  pmyWorld->CreateAsteroids(URANIUM, 5, 40.0);
  pmyWorld->PhysicsModel(0.0);  // Add new stuff

  wldbuflen=MAX_THINGS*256;
  wldbuf = new char[wldbuflen];
  memset(wldbuf,0,wldbuflen);

  printf ("World created, %d teams initialized\n",nTms);
  printf ("Ready for connections on port %d\n",port);
}

CServer::~CServer()
{
  delete [] wldbuf;

  delete [] abOpen;
  delete [] auTCons;

  delete pmyWorld;
  for (UINT i=0; i<nTms; i++) {
    delete aTms[i];
  }
  delete [] aTms;

  delete pmyNet;
}

////////////////////////////////////////
// Data access

UINT CServer::GetNumTeams() const
{
  return nTms;
}

double CServer::GetTime()
{
  return pmyWorld->GetGameTime();
}

CWorld *CServer::GetWorld()
{
  return pmyWorld;
}

////////////////////////////////////////
// Methods

UINT CServer::ConnectClients()
{
  int conn;
  char outbuf[2048];

  for (UINT i=0; i<GetNumTeams()+1; i++) {  // Teams and observer
    conn = pmyNet->WaitForConn();
    abOpen[conn-1]=true;
    printf ("Establishing connection #%d\n",conn);

    // Tell them they've connected
    snprintf (outbuf, sizeof(outbuf), n_servconack);
    pmyNet->SendPkt(conn,outbuf,strlen(outbuf));
  }

  // They've all linked up, now who the hell are they?
  int totcl=0, tmindex=0;
  int slen = strlen(n_obcon);   // n_obcon and n_teamcon same length

  while ((UINT)totcl < GetNumTeams()+1) {
    conn = pmyNet->CatchPkt();
    if (pmyNet->GetQueueLength(conn) < slen) {
      continue;
    }

    totcl++;   // It responded, whoever the hell it is    
    if (memcmp(pmyNet->GetQueue(conn),n_obcon,slen)==0) {
      ObsConn=conn;   // That's our observer
      snprintf (outbuf, sizeof(outbuf), "X");  // Dummy character, eases clientside parsing
      pmyNet->SendPkt(conn, outbuf,1);
    }

    if (memcmp(pmyNet->GetQueue(conn),n_teamcon,slen)==0) {
      if ((UINT)tmindex>=GetNumTeams()) continue;  // Who are all these people!?
      auTCons[tmindex]=conn;
      snprintf (outbuf, sizeof(outbuf), "%c",tmindex);
      pmyNet->SendPkt(conn, outbuf,1);
      tmindex++;
    }

    pmyNet->FlushQueue(conn);
    IntroduceWorld(conn);             // Tell it about its world
  }

  return GetNumTeams()+1;
}

void CServer::IntroduceWorld(int conn)
{
  char buf[4];   // Larger than probably needed

  buf[0] = GetNumTeams();
  buf[1] = aTms[0]->GetShipCount();
  
  pmyNet->SendPkt(conn,buf,2);
  return;
}

UINT CServer::SendWorld(int conn)
{
  if (abOpen[conn-1]!=true) return 0;
  if (pmyNet->IsOpen(conn)==0) {
    abOpen[conn-1]=false;
    printf ("Lost connection %d\n",conn);
    return 0;
  }

  UINT lenpred, lenact, netsize;

  lenpred = pmyWorld->GetSerialSize();
  if (lenpred>wldbuflen || lenpred<=0) return 0;
  lenact = pmyWorld->SerialPack(wldbuf,wldbuflen);

  if (lenact!=lenpred) {  // Didn't predict right, something's wrong
    printf ("Serialization error\n");
    return 0;
  }

  netsize = htonl(lenact);
  pmyNet->SendPkt(conn,(char*)(&netsize),sizeof(UINT));
  pmyNet->SendPkt(conn,wldbuf,lenact);
  return lenact;
}

void CServer::BroadcastWorld()
{
  if (bPaused) {
    // While paused, avoid waking teams; observer gets updates elsewhere
    return;
  }
  for (UINT conn=1; conn<=GetNumTeams()+1; conn++) {
    if (conn==ObsConn) continue;  // Observer gets world elsewhere
    if (abOpen[conn-1]!=true) continue;  // This connection closed, next!

    if (pmyNet->IsOpen(conn)==0) continue;  // Don't send to closed socket

    SendWorld(conn);
  }

  for (UINT tm=0; tm<GetNumTeams(); tm++) {
    pmyWorld->atstamp[tm]=pmyWorld->GetTimeStamp();
    // They got the world, start counting!
  }
}

void CServer::ResumeSync()
{
  // Reset team timing
  double now = pmyWorld->GetTimeStamp();
  for (UINT tm=0; tm<GetNumTeams(); tm++) {
    pmyWorld->atstamp[tm] = now;
  }
  // Clear per-step flags without advancing time
  pmyWorld->PhysicsModel(0.0);
  // Push a fresh world snapshot to all teams even if paused was engaged
  for (UINT conn=1; conn<=GetNumTeams()+1; conn++) {
    if (conn==ObsConn) continue;
    if (abOpen[conn-1]!=true) continue;
    if (pmyNet->IsOpen(conn)==0) continue;
    SendWorld(conn);
  }
}

void CServer::WaitForObserver()
{
  // Don't wait for a disconnected server
  if (abOpen[ObsConn-1]==false) return;

  UINT len;
  char *pq;

  while (true) {
    while ((len=pmyNet->GetQueueLength(ObsConn))
	   < strlen(n_oback)) {
      pmyNet->CatchPkt();

      // Check if connection closed
      if (pmyNet->IsOpen(ObsConn)==0) {
	abOpen[ObsConn-1]=false;
	printf ("Observer disconnected\n");
	return;
      }
    }

    // Refresh pointer to queue for current contents
    pq = pmyNet->GetQueue(ObsConn);

    // Check for control commands from observer
    if (len >= (int)strlen(n_pause) && memcmp(pq, n_pause, strlen(n_pause))==0) {
      SetPaused(true);
      pmyNet->FlushQueue(ObsConn);
      printf("Observer requested PAUSE\n");
      continue; // Keep waiting for ack
    }
    if (len >= (int)strlen(n_resume) && memcmp(pq, n_resume, strlen(n_resume))==0) {
      SetPaused(false);
      pmyNet->FlushQueue(ObsConn);
      printf("Observer requested RESUME\n");
      // Perform resume-safe sync: reset team timers, settle flags, and push snapshot
      ResumeSync();
      continue; // Keep waiting for ack
    }

    if (memcmp(pq,n_oback,strlen(n_oback))==0)
      break;   // Yay!  It's the ack!

    pmyNet->FlushQueue(ObsConn);  // Whatever it was, it was wrong
  }

  pmyNet->FlushQueue(ObsConn);
}

void CServer::MeetTeams()
{
  int conn;
  UINT len, tn, totresp=0;
  char *buf;
  bool *abGotFlag = new bool[GetNumTeams()];

  for (tn=0; tn<GetNumTeams(); tn++)
    abGotFlag[tn]=false;

  while (totresp<GetNumTeams()) {
    for (tn=0; tn<GetNumTeams(); tn++) {
      if (abGotFlag[tn]==true) continue;

      conn = auTCons[tn];
      len = pmyNet->GetQueueLength(conn);
      if (len>=aTms[tn]->GetSerInitSize()) {
	totresp++;
	abGotFlag[tn]=true;
      }
    }

    if (totresp>=GetNumTeams()) break;
    pmyNet->CatchPkt();
  }

  for (tn=0; tn<GetNumTeams(); tn++) {
    conn = auTCons[tn];
    len = pmyNet->GetQueueLength(conn);
    buf = pmyNet->GetQueue(conn);

    aTms[tn]->SerUnpackInitData(buf,len);
    len = aTms[tn]->GetSerInitSize();   // Make *sure* length is right
    WaitForObserver();
    pmyNet->SendPkt(ObsConn,buf,len);  // And send to observer

    pmyNet->FlushQueue(conn);
  }

  delete [] abGotFlag;
}

void CServer::ReceiveTeamOrders()
{
  if (bPaused) {
    // While paused, still service observer traffic and refresh world view
    WaitForObserver();
    SendWorld(ObsConn);
    return;
  }
  int conn, len;
  UINT tn, totresp=0;
  char *buf;
  bool *abGotFlag = new bool[GetNumTeams()];
  double tstart, tnow, tobs;
  double timediff, tthink;

  for (tn=0; tn<GetNumTeams(); tn++) {
    aTms[tn]->Reset();
    abGotFlag[tn]=false;
  }

  tstart = pmyWorld->GetTimeStamp();  // Teams can't take >60sec/turn to respond
  tobs = tstart;     // The observer should receive updates just in case
  while (totresp<GetNumTeams()) {
    tnow = pmyWorld->GetTimeStamp();
    timediff = tnow-tobs;
    if (timediff>=5.0) {  // Server updates every 5 seconds
      WaitForObserver();
      SendWorld(ObsConn);
      tobs=tnow;
    }

    for (tn=0; tn<GetNumTeams(); tn++) {
      if (abGotFlag[tn]==true) continue;  // Already counted

      conn = auTCons[tn];
      if (abOpen[conn-1]!=true) {
	totresp++;    // Skip over this one, count as gotten
	abGotFlag[tn]=true;  // Pretend we got it
	continue;   // But don't actually compute stuff
      }

      if (pmyNet->IsOpen(conn)==0) {
	abOpen[conn-1]=false;
	printf ("%s disconnected\n", aTms[tn] ? aTms[tn]->GetName() : "Unknown Team");
	continue;
      }

      // Team's still thinking, handle time counting here
      timediff = tnow - pmyWorld->atstamp[tn];
      if (aTms[tn]->GetWallClock()==0.0) timediff=0.01;   // Just began game, something small
      pmyWorld->auClock[tn]+=timediff;
      pmyWorld->atstamp[tn] = tnow;
      if (aTms[tn]->GetWallClock() >300.0) {
	printf ("%s timed out, severing connection\n", aTms[tn] ? aTms[tn]->GetName() : "Unknown Team");
	pmyNet->CloseConn(conn);  // Close connection, will skip over
	continue;   // Check other teams
      }

      tthink = tnow-tstart;
      if (tthink>60.0) {
	printf ("%s taking too long, orders ignored\n", aTms[tn] ? aTms[tn]->GetName() : "Unknown Team");
	totresp++;   // Pretend it's responded
	abGotFlag[tn]=true;  // Pretend it's responded 
	continue;    // And keep chugging
      }

      len = pmyNet->GetQueueLength(conn);
      if ((UINT)len>=aTms[tn]->GetSerialSize()) {
	totresp++;
	abGotFlag[tn]=true;

	buf = pmyNet->GetQueue(conn);
	aTms[tn]->SerialUnpack(buf,len);  // Ships get orders
	pmyNet->FlushQueue(conn);
      }
    }

    if (totresp>=GetNumTeams()) break;
    pmyNet->CatchPkt();
  }

  pmyWorld->PhysicsModel(0.0);
  delete [] abGotFlag;
}

double CServer::Simulation()
{
  if (bPaused) {
    // Don't advance physics or lasers, just keep observer connection alive
    WaitForObserver();
    SendWorld(ObsConn);
    return GetTime();
  }
  double t, maxt=1.0, tstep=0.2;
  UINT tm;
  for (t=0.0; t<maxt; t+=tstep) {
    pmyWorld->PhysicsModel (tstep);
    if (t>=maxt-tstep) {
      pmyWorld->LaserModel();
    }

    WaitForObserver();
    SendWorld(ObsConn);

    for (tm=0; tm<nTms; tm++) {
      aTms[tm]->MsgText[0]=0;
    }
  }

  return GetTime();
}

///////////////////////////////////
// Protected methods
