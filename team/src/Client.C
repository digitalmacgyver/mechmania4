/* Client.C
 * 9/3/1998 by Misha Voloshin
 * For use with MechMania IV
 */

#include "Client.h"
#include "ClientNet.h"
#include "ParserModern.h"
#include "Team.h"
#include "World.h"

extern CParser* g_pParser;

//////////////////////////////////////
// Construction/Destruction

CClient::CClient(int port, char *hostname, bool bObserv) {
  bObflag = bObserv;
  umyIndex = (unsigned int)-1;
  aTms = NULL;

  pmyWorld = NULL;
  /*
  // LEGACY CODE: We used to initiate pmyWorld here, but it is almost
  // immediately overwritten with the world from the server, so we skip that
  now. pmyWorld = new CWorld(0); pmyWorld->CreateAsteroids(VINYL,5,40.0);
  pmyWorld->CreateAsteroids(URANIUM,5,40.0);
  pmyWorld->ResolvePendingOperations();  // Add new asteroids to world
  */

  pmyNet = new CClientNet(hostname, port);
  if (IsOpen() == 0) {
    return;  // Connection failed
  }

  const int ack_length = static_cast<int>(strlen(n_servconack));
  while (pmyNet->GetQueueLength() < ack_length) {
    pmyNet->CatchPkt();
  }

  if (memcmp(pmyNet->GetQueue(), n_servconack, ack_length) != 0) {
    printf("Connection failed\n");
    exit(-1);
  }

  printf("Connection to MechMania IV server established\n");
  pmyNet->FlushQueue();

  const char *conack = (bObflag ? n_obcon : n_teamcon);
  pmyNet->SendPkt(1, conack, strlen(conack));
  printf("Identifying myself as %s\n", bObflag ? "Observer" : "Team client");

  while (pmyNet->GetQueueLength() <= 0) {
    pmyNet->CatchPkt();
  }
  umyIndex = pmyNet->GetQueue()[0];
  if (bObflag == false) {
    printf("Recognized as team index %d\n", umyIndex);
  } else if (umyIndex != 'X') {
    printf("Observation request not acknowledged\n");
  } else {
    printf("Recognized as observer\n");
  }
  pmyNet->FlushQueue();

  MeetWorld();
}

CClient::~CClient() {
  delete pmyNet;
  if (pmyWorld != NULL) {
    delete pmyWorld;
  }
  if (aTms != NULL) {
    for (unsigned int i = 0; i < numTeams; ++i) {
      if (aTms[i] != NULL) {
        delete aTms[i];
      }
    }
  }
  delete[] aTms;
}

///////////////////////////////////////////////////
// Data access

CWorld *CClient::GetWorld() { return pmyWorld; }

int CClient::IsOpen() { return pmyNet->IsOpen(1); }

///////////////////////////////////////////////////
// Methods

void CClient::MeetWorld() {
  char *buf;
  unsigned int numSh, len;

  delete pmyWorld;
  while (pmyNet->GetQueueLength() < 2) {
    pmyNet->CatchPkt();
  }

  buf = pmyNet->GetQueue();
  numTeams = buf[0];
  numSh = buf[1];
  pmyNet->FlushQueue();

  unsigned int i, teamNum;

  aTms = new CTeam *[numTeams];
  for (i = 0; i < numTeams; ++i) {
    aTms[i] = CTeam::CreateTeam();
  }
  pmyWorld = new CWorld(numTeams);

  printf("%d teams with %d ships each\n", numTeams, numSh);

  for (i = 0; i < numTeams; ++i) {
    teamNum = 0;

    aTms[i]->SetTeamNumber(teamNum);
    aTms[i]->SetWorld(pmyWorld);
    aTms[i]->Create(numSh, i);
    pmyWorld->SetTeam(i, aTms[i]);

    if (!bObflag && g_pParser && i == umyIndex) {
      auto shipArtRequest = g_pParser->GetShipArtRequest();
      if (shipArtRequest && !shipArtRequest->empty()) {
        aTms[i]->SetShipArtRequest(*shipArtRequest);
      }
    }
  }

  pmyWorld->ResolvePendingOperations();

  if (bObflag == true) {
    // We're the observer, send an ack
    SendAck();
    MeetTeams();  // Get team names and ship stats
    return;       // Nothing else for the observer to do
  }

  // We're a team
  // Now let's initialize our team and send it to the server
  len = aTms[umyIndex]->GetSerInitSize();
  buf = new char[len];
  aTms[umyIndex]->Init();  // Team initialized
  aTms[umyIndex]->SerPackInitData(buf, len);
  pmyNet->SendPkt(1, buf, len);

  delete buf;
}

unsigned int CClient::ReceiveWorld() {
  if (IsOpen() == 0) {
    pmyWorld->bGameOver = true;
    pmyWorld->PhysicsModel(0.1);  // Slow-mo
    return 0;
  }

  unsigned int netlen, len, aclen;
  char *buf = pmyNet->GetQueue();

  while (pmyNet->GetQueueLength() < static_cast<int>(sizeof(unsigned int))) {
    pmyNet->CatchPkt();
    if (IsOpen() == 0) {
      return 0;  // Eek!  World disappeared!
    }
  }

  memcpy(&netlen, buf, sizeof(unsigned int));
  len = ntohl(netlen);

  if (len > MAX_THINGS * 256) {
    return 0;  // Bad!
  }
  while (pmyNet->GetQueueLength() < (int)(len + sizeof(unsigned int))) {
    pmyNet->CatchPkt();
    if (IsOpen() == 0) {
      return 0;
    }
  }

  aclen = pmyWorld->SerialUnpack(buf + sizeof(unsigned int), len);
  pmyNet->FlushQueue();

  if (aclen != len) {
    printf("World length incongruency; %d!=%d\n", aclen, len);
  }
  return aclen;
}

unsigned int CClient::ReceiveWorldNonBlocking() {
  if (IsOpen() == 0) {
    pmyWorld->bGameOver = true;
    pmyWorld->PhysicsModel(0.1);
    return 0;
  }

  unsigned int netlen, len, aclen;
  char *buf = pmyNet->GetQueue();

  if (pmyNet->GetQueueLength() < static_cast<int>(sizeof(unsigned int))) {
    int conn = pmyNet->CatchPktNonBlocking();
    (void)conn;
    if (IsOpen() == 0 || pmyNet->GetQueueLength() <
                              static_cast<int>(sizeof(unsigned int))) {
      return 0;
    }
  }

  memcpy(&netlen, buf, sizeof(unsigned int));
  len = ntohl(netlen);

  if (len > MAX_THINGS * 256) {
    return 0;
  }

  const int required =
      static_cast<int>(len + sizeof(unsigned int));
  if (pmyNet->GetQueueLength() < required) {
    int conn = pmyNet->CatchPktNonBlocking();
    (void)conn;
    if (IsOpen() == 0 || pmyNet->GetQueueLength() < required) {
      return 0;
    }
  }

  aclen = pmyWorld->SerialUnpack(buf + sizeof(unsigned int), len);
  pmyNet->FlushQueue();

  if (aclen != len) {
    printf("World length incongruency; %d!=%d\n", aclen, len);
  }
  return aclen;
}

void CClient::MeetTeams() {
  if (bObflag != true) {
    return;  // Only observer allowed
  }

  unsigned int nTm, len;
  char *buf;

  for (nTm = 0; nTm < numTeams; ++nTm) {
    while ((len = pmyNet->GetQueueLength()) < aTms[nTm]->GetSerInitSize()) {
      pmyNet->CatchPkt();
    }

    buf = pmyNet->GetQueue();
    aTms[nTm]->SerUnpackInitData(buf, len);
    pmyNet->FlushQueue();

    SendAck();  // We took your load
  }
}

int CClient::SendAck() {
  if (IsOpen() == 0) {
    return 0;  // Don't write to closed conn
  }
  return pmyNet->SendPkt(1, n_oback, strlen(n_oback));
}

int CClient::SendPause() {
  if (IsOpen() == 0) {
    return 0;
  }
  return pmyNet->SendPkt(1, n_pause, strlen(n_pause));
}

int CClient::SendResume() {
  if (IsOpen() == 0) {
    return 0;
  }
  return pmyNet->SendPkt(1, n_resume, strlen(n_resume));
}

void CClient::DoTurn() {
  static char buf[4096];  // static to save on realloc
  if (IsOpen() == 0) {
    return;  // Don't write to closed conn
  }

  CTeam *pTm = aTms[umyIndex];
  unsigned int len = pTm->GetSerialSize();
  if (len > 4096) {
    return;  // BAD!
  }

  pTm->Reset();                  // Resets stuff
  pTm->Turn();                   // Team's AI does its thing
  pTm->SerialPack(buf, len);     // Pack up our hard-won orders
  pmyNet->SendPkt(1, buf, len);  // And ship them to the server
}
