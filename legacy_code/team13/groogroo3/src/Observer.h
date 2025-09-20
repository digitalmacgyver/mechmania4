// Mechmania IV Main Observer

#ifndef OBSERVER_H_DEEDLEQUEEP
#define OBSERVER_H_DEEDLEQUEEP

#include <fstream.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <iostream.h>
#include <stdlib.h>
#include <sys/types.h>
#include "stdafx.h"
#include "SpaceViewer.h"
#include "Thing.h"
#include "Ship.h"
#include "Team.h"
#include "World.h"

class Observer
{
  friend class SpaceViewer;

protected:
  int displayWidth, displayHeight, spaceWidth, spaceHeight, screenNum;
  int msgWidth, msgHeight, tWidth, tHeight, spaceClipWidth, spaceClipHeight;
  int borderX, borderY, t1PosX, t1PosY, t2PosX, t2PosY, msgPosX, msgPosY;
  int timeX,timeY,timeHeight,timeWidth;
  int logoW, logoH;
  double oneThirdCircle, twoThirdCircle, centerx, centery;
  Window win;
  Pixmap canvas, spaceCanvas, timeCanvas, msgCanvas, 
      t1Canvas, t2Canvas, spaceClipMask, logoPix, logoClip;
  GC gc;
  unsigned long black, white, gray, lasCol, fuelcol, vinylcol, teamcol[6];
  Display *display;
  XGCValues xgcv;
  XFontStruct *font_info, *smallfont;
  Colormap cmap;
  CWorld* myWorld;
  BOOL useXpm, useVelVectors;
  int drawnames;

  int attractor;
  
  int msg_rows, msg_cols, msg_r, msg_c;
  void initMsg( void );
  void scrollUp( void );
  unsigned long GetPixelValue(const char *colorname);
  void printGameTime( double game_time );
  
  void clearStatusWins();
  UINT AlertStatus(double, double);
  void plotStatusWins(int, Pixmap);
  void drawAll();
  void printMsg(char *str, int color=0 );

public:
 
  SpaceViewer *myViewer;

  Observer(char*, int);
  Observer();
  ~Observer();
  int getWorld(CWorld*);
  int plotWorld();
  BOOL getUseXpm();
  BOOL setUseXpm(BOOL);
  BOOL getUseVelVectors();
  BOOL setUseVelVectors(BOOL newState);
  void getKeystroke();
  void drawCredits();

  void setAttractor( int val );
 };

#endif







