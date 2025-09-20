// Tim Harahan
// Mechmania IV ObserverImage class
// ObserverIMage is a container class for the bitmaps being used 
// by the Observer SpaceViewer.

#ifndef OBSERVERIMAGE_H_DARIA
#define OBSERVERIMAGE_H_DARIA

#include <X11/Xlib.h>
#include <iostream.h>

class ObserverImage
{
  Pixmap myImage, clipMask;
  int width, height;
  Display *myDisplay;

public:
  ObserverImage(Pixmap, Pixmap, int, int, Display*);
  ObserverImage();
  ~ObserverImage();

  
  Pixmap getImage();
  Pixmap getClipMask();
  int getWidth(); 
  int getHeight();
};

#endif
