// Mechmania IV SpaceViewer Header
// Tim Harahan 7-13-1998 10PM

#ifndef SPACEVIEWER_H_BOOGELY
#define SPACEVIEWER_H_BOOGELY

#include <fstream>
#include <iostream>
#include "ObserverImage.h"
#include "Observer.h"

using namespace std;

#ifndef numstars
#define numstars  2048
#endif // numstars

class Observer;

class SpaceViewer
{

protected:
  struct XpmInfo
  {
    int depth, width, height;
  };

  struct SStarType {
    UINT uX,uY;
  } aStars[numstars];

  ObserverImage **obImages;
  Observer *sObserver;
  UINT lasCol;
  double imgRotInc;
  UINT xpmColors[256];

  int getImages(char*);

public:
  bool gotImages, bstarplot;

  SpaceViewer(char*, Observer*);
  ~SpaceViewer();

  void Clear();
  void plotLaser(double, double, double, double);
  void plotThing(double, double, double, int, const char *thingname=NULL);
  void plotVelVector(double, double, double, double, double);
  int readXpmFromFile(char*, Pixmap*, Pixmap*, XpmInfo*);
  void testImages(int);

  void initStars(int maxx, int maxy);
  void plotStars();
};

#endif
