// Tim Harahan
// Mechmania IV ObserverImage class
// ObserverImage is a container class for the bitmaps being used
// by the Observer SpaceViewer.

#include "ObserverImage.h"

ObserverImage::ObserverImage(Pixmap aPixmap, Pixmap CMask, int aWidth,
                             int aHeight, Display* aDisplay) {
  myDisplay = aDisplay;
  myImage = aPixmap;
  clipMask = CMask;
  width = aWidth;
  height = aHeight;
}

ObserverImage::ObserverImage() {
  myImage = 0;
  clipMask = 0;
  width = -1;
  height = -1;
}

ObserverImage::~ObserverImage() {
  // Move this to the SpaceViewer
  if (myImage != 0) {
    XFreePixmap(myDisplay, myImage);
  }

  if (clipMask != 0) {
    XFreePixmap(myDisplay, clipMask);
  }
}

Pixmap ObserverImage::getImage() { return myImage; }

Pixmap ObserverImage::getClipMask() { return clipMask; }

int ObserverImage::getWidth() { return width; }

int ObserverImage::getHeight() { return height; }
