#include <sys/types.h>

#include <ctime>

#include "Observer.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "World.h"

main() {
  // Test code
  srand(time(NULL));

  CWorld myWorld(2);  // World created
  CWorld TestWorld(2);

  // Observer myObserver("graphics.reg");
  Observer TestObserver("graphics.reg");

  // First let's create the teams and add them to the world
  CTeam *aTms[2], *aTestTms[2];
  UINT unTm;
  for (unTm = 0; unTm < 2; unTm++) {
    aTms[unTm] = new CTeam(unTm, &myWorld);
    aTms[unTm]->Create(2, unTm);
    myWorld.SetTeam(unTm, aTms[unTm]);

    aTestTms[unTm] = new CTeam(unTm, &myWorld);
    aTestTms[unTm]->Create(2, unTm);
    TestWorld.SetTeam(unTm, aTestTms[unTm]);
  }
  myWorld.PhysicsModel(0.0);  // Add new stuff
  TestWorld.PhysicsModel(0.0);

  // Now let's create asteroids
  myWorld.CreateAsteroids(VINYL, 5, 40.0);
  myWorld.CreateAsteroids(URANIUM, 5, 40.0);

  // TestWorld.CreateAsteroids(VINYL, 5, 40.0);
  // TestWorld.CreateAsteroids(URANIUM, 5, 40.0);

  // And here's the main loop

  double cyc;
  char wldbuf[16176];
  UINT wldsz;

  while (myWorld.GetGameTime() < 400.0) {
    //    printf ("%f\n",myWorld.GetGameTime());
    wldsz = myWorld.SerialPack(wldbuf, myWorld.GetSerialSize());
    TestWorld.SerialUnpack(wldbuf, wldsz);

    for (unTm = 0; unTm < 2; unTm++) {
      aTms[unTm]->Turn();
    }

    for (cyc = 0.0; cyc < 0.8; cyc += 0.2) {
      myWorld.PhysicsModel(0.2);

      wldsz = myWorld.SerialPack(wldbuf, myWorld.GetSerialSize());
      TestWorld.SerialUnpack(wldbuf, wldsz);

      TestObserver.getWorld(&TestWorld);
      TestObserver.plotWorld();
    }

    myWorld.PhysicsModel(0.2);
    myWorld.LaserModel();

    wldsz = myWorld.SerialPack(wldbuf, myWorld.GetSerialSize());
    TestWorld.SerialUnpack(wldbuf, wldsz);

    TestObserver.getWorld(&TestWorld);
    TestObserver.plotWorld();
  }

  // Clean-up time

  double maxvin = -1.0;
  char wintm[512];
  for (unTm = 0; unTm < 2; unTm++) {
    if (aTms[unTm]->GetScore() > maxvin) {
      maxvin = aTms[unTm]->GetScore();
      snprintf(wintm, sizeof(wintm), "%s", aTms[unTm]->GetName());
    }
    delete aTms[unTm];
    delete aTestTms[unTm];
  }

  printf("Winner is %s\n", wintm);
}
