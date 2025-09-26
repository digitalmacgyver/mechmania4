// Tim Harahan
// MM4 SpaceViewer Components
// Created 6-29-98
// Last modified 7-13-98

#include "SpaceViewer.h"

#define USE_XPMLIB
#define USE_CLIPPING

#ifdef USE_XPMLIB
#include "xpm.h"
#endif

// This one draws

const UINT numImg = 1000;

SpaceViewer::SpaceViewer(char* filename, Observer* myObserver) {
  UINT initCol, i;
  XColor col1, col2;

  // cout << "Entered SpaceViewer constructor" << endl;

  gotImages = false;
  obImages = new ObserverImage*[numImg];
  for (i = 0; i < numImg; i++) {
    obImages[i] = NULL;
  }

  // Save a reference to the Observer
  sObserver = myObserver;

  // Turn off sprites if there were load problems
  if (sObserver->getUseXpm()) {
    if (getImages(filename) != 0) {
      sObserver->setUseXpm(false);
      gotImages = false;
    } else {
      gotImages = true;
    }
  }

  // Get the laser color

  imgRotInc = PI2 / 18.0;

  // Init xpmColors
  XAllocNamedColor(sObserver->display, sObserver->cmap, "blue", &col1, &col2);
  initCol = col1.pixel;
  for (int cntr1 = 0; cntr1 < 256; cntr1++) {
    xpmColors[cntr1] = sObserver->lasCol;
  }

  initStars(1280, 1024);  // Assume pretty large display, lower star density if
                          // assumptn is false
  bstarplot = gotImages;  // Defaults to tactical display
}

SpaceViewer::~SpaceViewer() {
  int i;
  for (i = 0; i < numImg; i++) {
    delete obImages[i];
  }
  delete[] obImages;
  // Do not attempt to free the Observer from here
}

int SpaceViewer::getImages(char* filename) {
  Pixmap currentBitmap, currentPixmap;
  XpmInfo xpmAttr;
  char *currentBMPFile, *currentRegFile, nameBuffer[50];
  unsigned int cntr = 0;
  int readingName, value, namePos = 0;

  currentPixmap = 0;

  currentRegFile = filename;
  ifstream graphicsRegistry(currentRegFile, (ios::in | ios::nocreate));

  if (!graphicsRegistry) {
    cerr << "Graphics registry " << currentRegFile << " could not be opened"
         << endl;
    return 1;
  } else {
    readingName = 0;
    currentBMPFile = NULL;
    //      XSetForeground(sObserver->display, sObserver->gc, vinCol);
    XSetBackground(sObserver->display, sObserver->gc,
                   BlackPixel(sObserver->display, sObserver->screenNum));

    while (graphicsRegistry >> nameBuffer) {
      // If the filename isn't a comment, act on it
      nameBuffer[49] = 0;
      if (nameBuffer[0] != ';') {
        value = readXpmFromFile(nameBuffer, &currentPixmap, &currentBitmap,
                                &xpmAttr);

        if (value == 0) {
          obImages[cntr] =
              new ObserverImage(currentPixmap, currentBitmap, xpmAttr.width,
                                xpmAttr.height, sObserver->display);
          cntr++;
        } else {
          cout << "Warning: bitmap " << nameBuffer << " at index " << cntr
               << " was not valid." << endl;
        }
      } else {
        printf("%s\n",
               nameBuffer);  // A comment, let's show it for progress monitoring
      }

      // Reset for the next batch
      namePos = 0;
      readingName = 0;
      for (int cntr1 = 0; cntr1 < 13; cntr1++) {
        nameBuffer[cntr1] = 0;
      }
    }
    // Set this to an error code system

    //      cout << "Done reading bitmaps" << endl;
    return 0;
  }
}

int SpaceViewer::readXpmFromFile(char* fileName, Pixmap* newPixmap,
                                 Pixmap* newBitmap, XpmInfo* imgInfo) {
#ifdef USE_XPMLIB
  int err;
  XpmAttributes xa;
  xa.valuemask = XpmCloseness;
  xa.closeness = 50000;

  err = XpmReadFileToPixmap(sObserver->display, sObserver->win, fileName,
                            newPixmap, newBitmap, &xa);
  if (err) {
    fprintf(stderr, "%s: %d\n", fileName, err);
  }

  /*  fprintf(stderr,"%s / %d / %d\n",fileName,*newPixmap, *newBitmap); */

  imgInfo->width = xa.width;
  imgInfo->height = xa.height;

  return err;

#else
  int charsPerPixel, ycntr = 0;
  ;
  UINT colval;
  char inChar1, inChar2, *colString, *dataRow;
  bool infoRead, colorsRead, colorsStarted;
  XColor col1, col2;
  char intString[4];

  infoRead = false;
  colorsRead = false;
  colorsStarted = false;

  printf("Reading file %s\n", fileName);
  ifstream inFile(fileName, ios::in);

  // Read 'til you hit the first "..." data block
  while (inFile >> inChar1) {
    if (infoRead && colorsStarted && !colorsRead && inChar1 == '/')
      colorsRead = true;

    if (inChar1 == '"') {
      if (!colorsRead) {
        if (!infoRead) {
          inFile >> intString;
          while ((intString[0] < 48) || (intString[0] > 57)) {
            inFile >> intString;
          }
          imgInfo->width = atoi(intString);

          while ((intString[0] < 48) || (intString[0] > 57)) {
            inFile >> intString;
          }
          imgInfo->height = atoi(intString);

          while ((intString[0] < 48) || (intString[0] > 57)) {
            inFile >> intString;
          }

          while ((intString[0] < 48) || (intString[0] > 57)) {
            inFile >> intString;
          }
          //  charsPerPixel = atoi(intString);
          charsPerPixel = 6;

          imgInfo->depth =
              DefaultDepth(sObserver->display, sObserver->screenNum);

          // cout << "Creating pixmap of size " << imgInfo->width << " by " <<
          // imgInfo->height << endl;

          *newPixmap =
              XCreatePixmap(sObserver->display, sObserver->win, imgInfo->width,
                            imgInfo->height, imgInfo->depth);

          // Insert error checking for NULL pixmaps here

          // Create the clipmask and initialize it to clip nothing
          *newBitmap = XCreatePixmap(sObserver->display, sObserver->win,
                                     imgInfo->width, imgInfo->height, 1);

          /*
          XSetForeground(sObserver->display, sObserver->gc,
                         sObserver->white);
          XFillRectangle(sObserver->display, *newBitmap, sObserver->gc,
                         0, 0, imgInfo->width, imgInfo->height);
          */

          colString = new char[(charsPerPixel + 1)];
          //		  colString = new char[7];
          for (int colInitCntr = 0; colInitCntr < (charsPerPixel + 1);
               colInitCntr++)
            colString[colInitCntr] = 0;
          // cout << "charsPerPixel is " << charsPerPixel << endl;

          dataRow = new char[(imgInfo->width + 1)];
          infoRead = true;

          // Get rid of the closing '"'
          while (inChar1 != ',') {
            //       cout << "inChar1 now " << inChar1 << endl;
            inFile >> inChar1;
          }

        } else  // parse the color info
        {
          if (!colorsStarted)
            colorsStarted = true;

          // inChar1 is the mapping index
          inFile >> inChar1;
          // inChar 2 checks for 'c'
          while (inChar2 != '#')
            inFile >> inChar2;

          colString[0] = '#';
          for (int cntr1 = 1; cntr1 < (charsPerPixel + 1); cntr1++) {
            inFile >> inChar2;
            colString[cntr1] = inChar2;
          }
          colString[7] = 0;

          // cout << "Allocating for colString " << colString << endl;

          XAllocNamedColor(sObserver->display, sObserver->cmap, colString,
                           &col1, &col2);
          xpmColors[(int)inChar1] = col1.pixel;

          while (inChar1 != ',')
            inFile >> inChar1;

          // cout << "Got color cell" << endl;
        }
      } else  // We parse the line of drawing
      {
        inFile >> dataRow;

        for (int xcntr = 0; xcntr < imgInfo->width; xcntr++) {
          XSetForeground(sObserver->display, sObserver->gc,
                         xpmColors[(int)dataRow[xcntr]]);
          XDrawPoint(sObserver->display, *newPixmap, sObserver->gc, xcntr,
                     ycntr);

          if (xpmColors[(int)dataRow[xcntr]] == sObserver->black) {
            /*
            XSetForeground(sObserver->display, sObserver->gc,
                           sObserver->black);
            XDrawPoint(sObserver->display, *newBitmap, sObserver->gc,
                       xcntr, ycntr);
            */
          }
        }
        ycntr++;
        // cout << "Parsed line" << dataRow << endl;
      }
    } else if (inChar1 == '}') {
      inFile >> inChar1;
      if (inChar1 == ';') {
        inFile.close();
        delete[] colString;
        delete[] dataRow;
        return 0;
      }
    }
  }
  inFile.close();
  delete[] colString;
  delete[] dataRow;
  return 3;
#endif
}

/* plotThing
   Input:
    pX - physics model X coord
    pY - physics model Y coord
    ang - orientation in radian
    type - type number of the thing to plot

    addStuff calculates the appropriate bitmap to be used for a
    CThing of the above data and plots it to an appropriately
    transformed version of (pX, pY)
*/

void SpaceViewer::plotThing(double pX, double pY, double ang, int type,
                            const char* thingname) {
  int index, rotAdj;
  double rot, posX, posY, sclX, sclY;

  //  cout << "Entered SpaceViewer stuff adder" << endl;

  sclX = (double)sObserver->spaceWidth / 1024.0;
  sclY = (double)sObserver->spaceHeight / 1024.0;

  // Calculate the bitmap to be used
  index = type * 18;

  // Select the appropriately rotated image
  rot = ang;
  while (rot > PI2) {
    rot -= PI2;
  }
  while (rot < 0) {
    rot += PI2;
  }

  rotAdj = (int)(rot / imgRotInc);
  index += rotAdj;
  if (rotAdj < 0) {
    index += 18;
  }

  if (obImages[index] == NULL) {
    return;
  }

  // cout << "Selected image index " << index << " for type " << type;
  // cout << " thing with rot " << ang << endl;

  // Compute the position
  posX = pX;
  posY = pY;
  posX *= sclX;
  posY *= sclY;
  posX += (double)(sObserver->centerx);
  posY += (double)(sObserver->centery);

  // Adjust the position to the bitmap's UL corner
  posX -= (obImages[index]->getWidth() >> 1);
  posY -= (obImages[index]->getHeight() >> 1);

  //  cout << "Plotted thing " << obImages[index]->getImage() << " at " << (int)
  //  posX << "," << (int) posY << ") " << endl;

  //  cout << "Dimensions: " << obImages[index]->getWidth() << "," <<
  //  obImages[index]->getHeight() << endl;
#ifdef USE_CLIPPING
  XSetClipMask(sObserver->display, sObserver->gc,
               obImages[index]->getClipMask());
  XSetClipOrigin(sObserver->display, sObserver->gc, (int)posX, (int)posY);
#endif
  XCopyArea(sObserver->display, obImages[index]->getImage(),
            sObserver->spaceCanvas, sObserver->gc, 0, 0,
            obImages[index]->getWidth(), obImages[index]->getHeight(),
            (int)posX, (int)posY);
#ifdef USE_CLIPPING
  XSetClipMask(sObserver->display, sObserver->gc, None);
#endif

  // Name-plotting code
  if (sObserver->drawnames == 1 && thingname != NULL) {
    XSetFont(sObserver->display, sObserver->gc, sObserver->smallfont->fid);

    int txtX, txtY;
    txtX = (int)posX;
    txtX += (obImages[index]->getWidth()) >> 1;
    txtX -= XTextWidth(sObserver->smallfont, thingname, strlen(thingname)) >> 1;
    txtY = (int)posY + obImages[index]->getHeight();
    txtY += sObserver->smallfont->ascent;  // + sObserver->smallfont->descent;
    XDrawString(sObserver->display, sObserver->spaceCanvas, sObserver->gc, txtX,
                txtY, thingname, strlen(thingname));

    XSetFont(sObserver->display, sObserver->gc, sObserver->font_info->fid);
  }
}

void SpaceViewer::Clear() {
  XSetForeground(sObserver->display, sObserver->gc,
                 BlackPixel(sObserver->display, sObserver->screenNum));
  XFillRectangle(sObserver->display, sObserver->spaceCanvas, sObserver->gc, 0,
                 0, sObserver->spaceWidth, sObserver->spaceHeight);

  plotStars();
}

void SpaceViewer::plotLaser(double pX, double pY, double lX, double lY) {
  double sclx, scly, srcX, srcY, tarX, tarY;

  sclx = sObserver->spaceWidth / 1024.0;
  scly = sObserver->spaceHeight / 1024.0;

  // Scale the coords
  srcX = pX * sclx;
  srcY = pY * scly;
  tarX = lX * sclx;
  tarY = lY * scly;

  // Center the coords in screen
  srcX += (double)(sObserver->centerx);
  srcY += (double)(sObserver->centery);
  tarX += (double)(sObserver->centerx);
  tarY += (double)(sObserver->centery);

  // Draw the beams
  XSetForeground(sObserver->display, sObserver->gc, sObserver->lasCol);
  XSetLineAttributes(sObserver->display, sObserver->gc, 3, LineSolid, CapRound,
                     JoinBevel);
  XDrawLine(sObserver->display, sObserver->spaceCanvas, sObserver->gc,
            (int)srcX, (int)srcY, (int)tarX, (int)tarY);
  if ((srcX < 0) || (tarX < 0)) {
    XDrawLine(sObserver->display, sObserver->spaceCanvas, sObserver->gc,
              (int)(srcX + sObserver->spaceWidth), (int)srcY,
              (int)(tarX + sObserver->spaceWidth), (int)tarY);
  }
  if ((srcY < 0) || (tarY < 0)) {
    XDrawLine(sObserver->display, sObserver->spaceCanvas, sObserver->gc,
              (int)srcX, (int)(srcY + sObserver->spaceHeight), (int)tarX,
              (int)(tarY + sObserver->spaceHeight));
  }
  if ((srcX > 0) || (tarX > 0)) {
    XDrawLine(sObserver->display, sObserver->spaceCanvas, sObserver->gc,
              (int)(srcX - sObserver->spaceWidth), (int)srcY,
              (int)(tarX - sObserver->spaceWidth), (int)tarY);
  }
  if ((srcY > 0) || (tarY > 0)) {
    XDrawLine(sObserver->display, sObserver->spaceCanvas, sObserver->gc,
              (int)srcX, (int)(srcY - sObserver->spaceHeight), (int)tarX,
              (int)(tarY - sObserver->spaceHeight));
  }
  XSetLineAttributes(sObserver->display, sObserver->gc, 1, LineSolid, CapRound,
                     JoinBevel);
}

void SpaceViewer::plotVelVector(double pX, double pY, double rad, double rho,
                                double theta) {
  double srcX, srcY, tarX, tarY, sclx, scly;

  sclx = (double)sObserver->spaceWidth / 1024.0;
  scly = (double)sObserver->spaceHeight / 1024.0;

  // Pick the points
  srcX = pX + (rad * cos(theta));
  srcY = pY + (rad * sin(theta));
  tarX = pX + ((rad + rho) * cos(theta));
  tarY = pY + ((rad + rho) * sin(theta));

  // Scale and center
  srcX *= sclx;
  srcY *= scly;
  tarX *= sclx;
  tarY *= scly;
  srcX += sObserver->centerx;
  srcY += sObserver->centery;
  tarX += sObserver->centerx;
  tarY += sObserver->centery;

  // Color
  XSetForeground(sObserver->display, sObserver->gc, sObserver->white);

  // Draw
  XDrawLine(sObserver->display, sObserver->spaceCanvas, sObserver->gc, srcX,
            srcY, tarX, tarY);

  // Mix well. Serves 6.
}

// Checks for data in the first testSize elements of obImages

void SpaceViewer::testImages(int testSize) {
  for (int cntr1 = 0; cntr1 < testSize; cntr1++) {
    cout << "ObImage test at index " << cntr1 << " ";
    cout << "Width " << obImages[cntr1]->getWidth() << " Height "
         << obImages[cntr1]->getHeight() << " Bitmap "
         << obImages[cntr1]->getImage() << endl;
  }
}

///////////////////////////////////////////////////
// Misha's star-related methods

void SpaceViewer::initStars(int maxx, int maxy) {
  for (UINT i = 0; i < numstars; i++) {
    aStars[i].uX = rand() % maxx;
    aStars[i].uY = rand() % maxy;
  }

  bstarplot = true;
}

void SpaceViewer::plotStars() {
  if (bstarplot == false) {
    return;  // Stars turned off
  }

  XSetForeground(sObserver->display, sObserver->gc, sObserver->white);
  for (UINT i = 0; i < numstars; i++) {
    XDrawPoint(sObserver->display, sObserver->spaceCanvas, sObserver->gc,
               aStars[i].uX, aStars[i].uY);
  }
}
