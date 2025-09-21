// MMIV Observer
// Tim Harahan

#include "Observer.h"
#include "Station.h"
#include "Ship.h"
#include "Asteroid.h"

#ifdef USE_XPMLIB
#include "xpm.h"
#endif

Observer::Observer(char* regFileName, int gfxFlag)
{
#ifdef USE_XPMLIB
  XpmAttributes xa;
  xa.valuemask = XpmCloseness;
  xa.closeness = 50000;
#endif
  
// Open a connection to the X Server
  if(!(display = XOpenDisplay(NULL)))
    cout << "Error: Observer cannot open display" << endl;

  myWorld = NULL;
  drawnames=1;

  // Don't load images without a file
  if(gfxFlag == 1) {
    useXpm = TRUE;
    useVelVectors=FALSE;  // No tactical display
  }
  else {
    useXpm = FALSE;
    useVelVectors=TRUE;  // Tactical display
  }

  screenNum = DefaultScreen(display);
  displayWidth = DisplayWidth(display, screenNum);
  displayHeight = DisplayHeight(display, screenNum);

  // Set border and background colors here
  black = BlackPixel(display, screenNum);
  white = WhitePixel(display, screenNum);
  cmap = DefaultColormap (display, screenNum);

  lasCol = GetPixelValue("red");
  gray = GetPixelValue("#A0A0A0");

  // Create the window
  win = XCreateSimpleWindow(display, RootWindow(display, screenNum),
			    0, 0, displayWidth, displayHeight, 0, 
			    black, gray);

  gc = XCreateGC(display,win, 0, NULL);
  //  XSetLineAttributes(display,gc, 3,LineSolid,CapRound,JoinBevel);

  // Set some properties of the window
  XSetStandardProperties(display,win,
			 "MechMania IV: The Vinyl Frontier",
			 "MM4", None,NULL,0,NULL);
  
  // Insert code to iconify Observer here if you really feel like it

  // Set size hints here if the whole window is ever refused to you
  // Same goes for wm hints and class hints, if necessary

  XSelectInput(display, win, ExposureMask | PointerMotionMask | 
	       KeyPressMask | ButtonPressMask | ButtonReleaseMask);

  // Load the fonts
  char fontname[]="-*-fixed-*-*-*-*-*-120-*-*-*-*-*-*";
  char smfnt[]="-*-fixed-*-*-*-*-*-100-*-*-*-*-*-*";

  smallfont = XLoadQueryFont(display,smfnt);
  if (smallfont==NULL) smallfont = XLoadQueryFont(display,"fixed");
  if (smallfont==NULL) { // If it's *still* NULL
    printf("Failed to load font... FOOL!\n");
    exit(-1);
  }

  font_info=XLoadQueryFont(display,fontname);
  if (font_info==NULL) {
    font_info = XLoadQueryFont(display,"fixed");
  }
  
  printf ("Fonts loaded\n");
  XSetFont(display,gc, font_info->fid);

  canvas = XCreatePixmap(display, win,
			 displayWidth, displayHeight, 
			 DefaultDepth(display, screenNum));
  
  spaceWidth = (int) (displayWidth * 0.7);
  spaceHeight = spaceWidth;
  borderX = (int) (displayWidth * 0.015);
  borderY = (int) ((displayHeight - spaceHeight) * 0.1);
  int totWinHgt = spaceHeight - 3*borderY;

  timeX = (int) (2*borderX) + spaceWidth;
  timeY = (int) borderY;
  timeWidth = displayWidth - timeX - borderX;
  timeHeight = (int)(totWinHgt*0.05);

  t1PosX = (int) (2 * borderX) + spaceWidth;
  t1PosY = timeY + timeHeight + borderY;
  tWidth = displayWidth - t1PosX - borderX;
  tHeight = (int)((font_info->ascent+font_info->descent)
		  * 7.3);

  t2PosX = t1PosX;
  t2PosY = t1PosY + tHeight + borderY;

  msgWidth = tWidth;
  msgHeight = spaceHeight - (tHeight+t2PosY);

  msgPosX = t1PosX;
  msgPosY = t2PosY + tHeight + borderY;
  
  spaceCanvas = XCreatePixmap(display, win,
			      spaceWidth, spaceHeight, 
			      DefaultDepth(display, screenNum));
  
  timeCanvas = XCreatePixmap(display, win, timeWidth, timeHeight,
			     DefaultDepth(display, screenNum));
  msgCanvas = XCreatePixmap(display, win, msgWidth, msgHeight, 
			    DefaultDepth(display, screenNum));
  t1Canvas = XCreatePixmap(display, win, tWidth, tHeight,
			   DefaultDepth(display, screenNum));
  t2Canvas = XCreatePixmap(display, win, tWidth, tHeight,
			   DefaultDepth(display, screenNum));
  
  XMapWindow(display, win);

  XFlush(display);

  // Start the intializations
  AlertStatus(0.0,0.0);  // Dummy call to initialize
  teamcol[0]=GetPixelValue("#ffb573");
  teamcol[1]=GetPixelValue("#00c68c");
  teamcol[2]=GetPixelValue("#ff11ac");
  teamcol[3]=GetPixelValue("#ffff22");
  fuelcol=GetPixelValue("#00ff00");
  vinylcol=GetPixelValue("#ff00ff");

  oneThirdCircle = (2 * PI2) / 3;
  twoThirdCircle = 2 * oneThirdCircle;
  // Provide coord centering info
  centerx = (double) (spaceWidth>>1);
  centery = (double) (spaceHeight>>1);
  
  myViewer = new SpaceViewer(regFileName, this);
  initMsg();

  attractor = 0;
      // load the logo 
#ifdef USE_XPMLIB
  XpmReadFileToPixmap(display, win,
		      "gfx/MM4Logo.xpm",
		      &logoPix,
		      &logoClip,
		      &xa);

  logoW = xa.width;
  logoH = xa.height;
#endif

  //  cout << "Ob constructor clear" << endl;
}

// This is just an empty constructor to satisfy the compiler

Observer::Observer()
{

}

Observer::~Observer()
{

  XFreeFont(display,smallfont);
  XFreeFont(display,font_info);

  if(myViewer != NULL)
    delete myViewer;

  if(canvas != 0)
    XFreePixmap(display, canvas);

  if(spaceCanvas != 0)
    XFreePixmap(display, spaceCanvas);

  if(t1Canvas != 0)
    XFreePixmap(display, t1Canvas);

  if(t2Canvas != 0)
    XFreePixmap(display, t2Canvas);

  if(msgCanvas != 0)
    XFreePixmap(display, msgCanvas);

  // Free the X elements
  XFreeGC(display, gc);
  XCloseDisplay(display);
}

unsigned long Observer::GetPixelValue(const char *colorname)
{
  XColor col1,col2;
  unsigned long res;

  XAllocNamedColor(display, cmap, colorname, &col1, &col2);
  res = col1.pixel;
  return res;
}

int Observer::getWorld(CWorld* theWorld)
{
  //  cout << "Entered Ob::getWorld" << endl;
  if(theWorld != NULL)
    {
      myWorld = theWorld;
      return 0;
    }
  else
    {
      myWorld = NULL;
      cout << "Error: Observer received NULL world" << endl;
      return 1;
    } 
}

int Observer::plotWorld()
{
  UINT nship, nteam;
  CTeam *pTeam;
  CThing *aThing;
  CShip *pShip;
  CCoord lasPos;
  int type, txtX,txtY;
  double pX, pY, endX, endY, pLasX, pLasY, lasRange, rad, ang, sclx, scly;
  // Okay, maybe we don't use XDrawLines anymore, but this still works
  XPoint triPts[3];
  CAsteroid* astPtr;
  CTraj vel;
  char thname[128];

  if(myWorld == NULL)
    {
      cout << "Observer called to plot a null world; aborting plot" << endl;
      return 2;
    }

  // Check for keyboard events before we get all wacky on the queue
  getKeystroke();

  // Clear the canvas
  myViewer->Clear();

  // Plot the lasers and update status
  
  for(nteam=0; nteam < myWorld->GetNumTeams(); nteam++)
    {
      pTeam = myWorld->GetTeam(nteam);
      
      if(pTeam == NULL) { 
	printf ("Ack, plotting null teams, we're all gonna die!\n");
	continue;
      }

      for(nship=0; nship < pTeam->GetShipCount(); nship++)
	{

	  pShip = pTeam->GetShip(nship);
	 
	  if(pShip!=NULL)
	    { 	 
	      lasRange = pShip->GetLaserBeamDistance();
	      if(lasRange!=0.0)
		{

		  // Set the ship position
		  pX = pShip->GetPos().fX;
		  pY = pShip->GetPos().fY;

		  ang = pShip->GetOrient();
		  pLasX = pX + (lasRange * cos(ang)); 
		  pLasY = pY + (lasRange * sin(ang));

		  myViewer->plotLaser(pX, pY, pLasX, pLasY);
		}
	    }
	}	      
    }

  // And plot the things in the world
  for (UINT i=myWorld->UFirstIndex; i !=(UINT)-1; i=myWorld->GetNextIndex(i))
    {
      aThing = myWorld->GetThing(i);
      pX = aThing->GetPos().fX;
      pY = aThing->GetPos().fY;
      ang = aThing->GetOrient();

      switch(aThing->GetKind())
	{
	case ASTEROID:
	  type = 4;
	  break;
	  
	case STATION:
	  type = 10;
	  break;
	  
	case SHIP:
	  type = 11;
	  break;
	  
	default:
	  printf("Warning: Observer received untyped thing\n");
	}
      
      type += aThing->GetImage();
      if (aThing->GetTeam() != NULL) 
	type += aThing->GetTeam()->GetWorldIndex() * 6;

	memset(thname,0, 128);
	if (aThing->GetKind()!=ASTEROID) {
  	  if (drawnames==1) sprintf(thname,"%s",aThing->GetName());
	  else if (drawnames==2) {
	     if (aThing->GetKind()==SHIP) 
	     sprintf (thname,"%d:%.0f:%.0f:%.0f",
                      ((CShip*)aThing)->GetShipNumber(),
                      ((CShip*)aThing)->GetAmount(S_SHIELD),
                      ((CShip*)aThing)->GetAmount(S_FUEL), 
                      ((CShip*)aThing)->GetAmount(S_CARGO));  
	     else if (aThing->GetKind()==STATION)
	             sprintf (thname,"%d: %.3f",
		            aThing->GetTeam()->GetTeamNumber(),
			    aThing->GetTeam()->GetScore());
	     }
        }

        if (getUseXpm()==TRUE && myViewer->gotImages==TRUE) 
        {
	  if (aThing->GetKind()==SHIP) {
	    XSetForeground(display,gc, 
			   teamcol[aThing->GetTeam()->GetWorldIndex()]);
	    myViewer->plotThing(pX, pY, ang, type, thname);
	    
	    // Calculate the damage sprite
	    // bIsColliding and bIsGettingShot now convey angles
	    if(aThing->bIsColliding!=NO_DAMAGE)
	      {
		myViewer->plotThing(pX, pY, aThing->bIsColliding, 0, 0);
	      }
	    
	    if(aThing->bIsGettingShot!=NO_DAMAGE)
	      {
		myViewer->plotThing(pX, pY, aThing->bIsGettingShot, 1, 0);
	      }
	  }
	  else if(aThing->GetKind()==STATION) 
	    {
	      XSetForeground(display,gc, 
			     teamcol[aThing->GetTeam()->GetWorldIndex()]);
	      myViewer->plotThing(pX, pY, ang, type, thname);
	      
	      if(aThing->bIsColliding!=NO_DAMAGE)
		{
		  myViewer->plotThing(pX, pY, aThing->bIsColliding, 2, 0);
		}
	      
	      if(aThing->bIsGettingShot!=NO_DAMAGE)
		{
		  myViewer->plotThing(pX, pY, aThing->bIsGettingShot, 3, 0);
		}
	    }
	  else myViewer->plotThing(pX, pY, ang, type, NULL);
	  // cout << "Added stuff of type " << type << " to SpaceViewer" << endl;
	}
	
	else
	  {
	    // useXpm not true, so we fall back to vector graphics
	    rad = aThing->GetSize();

	    sclx = spaceWidth / (fWXMax-fWXMin);
	    scly = spaceHeight / (fWYMax-fWYMin);

	    // Take care of name first
	    pTeam = aThing->GetTeam();
	    if (pTeam!=NULL) 
	      XSetForeground(display,gc, teamcol[pTeam->GetWorldIndex()]);
	    else XSetForeground(display,gc, white);

	    XSetFont(display,gc,smallfont->fid);
	    txtX = pX*sclx + centerx;
 	    txtY = pY*scly + centery;
	    txtX -= XTextWidth(smallfont, thname, strlen(thname))>>1;
	    txtY += (rad * scly);
	    txtY += smallfont->ascent;
	    XDrawString(display, spaceCanvas, gc,
                txtX,txtY, thname,strlen(thname));
	    XSetFont(display,gc,font_info->fid);

	    XSetLineAttributes(display,gc,2,LineSolid,CapRound,JoinBevel);
      	    if(aThing->GetKind() == ASTEROID)
	      {
		// Compiler won't let us cast this on the fly
		astPtr = (CAsteroid*) aThing;
		if (astPtr->GetMaterial() == URANIUM)
		   XSetForeground(display, gc, fuelcol);
	        else XSetForeground(display, gc, vinylcol);

		endX = pX + (rad * cos(ang));
		endY = pY + (rad * sin(ang));
		pX *= sclx;
		pY *= scly;
		endX *= sclx;
		endY *= scly;
		pX += centerx;
		pY += centery;
		endX += centerx;
		endY += centery;

		// Scale and center
		XDrawArc(display, spaceCanvas, gc, (pX-rad), (pY-rad), 
			 (rad * 2), (rad * 2), 0, 23040 );
		// XDrawLine(display, spaceCanvas, gc, pX, pY, endX, endY);
	      }
	    else
	      {
		XSetForeground(display,gc, 
			       teamcol[aThing->GetTeam()->GetWorldIndex()]);
		if(aThing->GetKind() == STATION)
		  {
		    pX -= rad;
		    pY -= rad;
		    endX = 2 * rad;
		    endY = 2 * rad;

		    // Scale and center
		    pX *= sclx;
		    pY *= scly;
		    endX *= sclx;
		    endY *= scly;
		    pX += centerx;
		    pY += centery;
		    
		    XDrawRectangle(display, spaceCanvas, gc, pX, pY, 
				   endX, endY);
		  }
		else
		  {
		    // Thing is a ship, calculate triangle points
		    triPts[0].x = pX + (0.7071 * (rad * cos(ang)));
		    triPts[0].y = pY + (0.7071 * (rad * sin(ang)));
		    triPts[1].x = pX + (0.7071 * 
					(rad * cos(ang + oneThirdCircle)));
		    triPts[1].y = pY + (0.7071 * 
					(rad * sin(ang + oneThirdCircle)));
		    triPts[2].x = pX + (0.7071 * 
					(rad * cos(ang + twoThirdCircle)));
		    triPts[2].y = pY + (0.7071 *
					(rad * sin(ang + twoThirdCircle)));

		    // Scale and center
		    triPts[0].x *= sclx;
		    triPts[0].y *= scly;
		    triPts[1].x *= sclx;
		    triPts[1].y *= scly;
		    triPts[2].x *= sclx;
		    triPts[2].y *= scly;

		    triPts[0].x += centerx;
		    triPts[0].y += centery;
		    triPts[1].x += centerx;
		    triPts[1].y += centery;
		    triPts[2].x += centerx;
		    triPts[2].y += centery;
		    
		    XDrawLine(display, spaceCanvas, gc, triPts[0].x, 
			      triPts[0].y, triPts[1].x, triPts[1].y);
		    XDrawLine(display, spaceCanvas, gc, triPts[0].x, 
			      triPts[0].y, triPts[2].x, triPts[2].y);
		  }
	      }
	    
	    XSetLineAttributes(display,gc,1,LineSolid,CapRound,JoinBevel);
	  }
    }
  
// Draw the velocity vectors as appropriate

  if(useVelVectors)
    {
      for (UINT i=myWorld->UFirstIndex; i !=(UINT)-1; i=myWorld->GetNextIndex(i))
	{	
	  aThing = myWorld->GetThing(i);
	  pX = aThing->GetPos().fX;
	  pY = aThing->GetPos().fY;
	  rad = aThing->GetSize();
	  vel = aThing->GetVelocity();

	  myViewer->plotVelVector(pX, pY, rad, vel.rho, vel.theta);
	}
    }

  clearStatusWins();
  plotStatusWins(0, t1Canvas);
  plotStatusWins(1, t2Canvas);

  drawAll();
  
  //  cout << "Ob::WorldPlot complete" << endl;
  return 0;
}

void Observer::clearStatusWins()
{
  XSetForeground(display, gc, black);
  XFillRectangle(display, timeCanvas, gc, 0, 0, tWidth, tHeight);
  XFillRectangle(display, t1Canvas, gc, 0, 0, tWidth, tHeight);
/*  XFillRectangle(display, msgCanvas, gc, 0, 0, msgWidth, msgHeight);*/
  XFillRectangle(display, t2Canvas, gc, 0, 0, tWidth, tHeight);
}

/*
  AlertStatus will return which of the input values should be returned
  based on a ship's status.  This lets us do checking of when to use warning 
  colors for status text.
*/

UINT Observer::AlertStatus(double statamt, double statcap)
{
  static BOOL lookedup=FALSE;
  static int okCol=white,warnCol=white,critCol=white;
  XColor col,dummy;

  if (lookedup==FALSE) {
    lookedup=TRUE;

    XAllocNamedColor(display, cmap, "red", &col, &dummy);
    critCol = col.pixel;
    XAllocNamedColor(display, cmap, "yellow", &col, &dummy);
    warnCol = col.pixel;
    XAllocNamedColor(display, cmap, "green", &col, &dummy);
    okCol = col.pixel;
  }

  if (statamt > 0.5 * statcap) return okCol;
  if (statamt > 0.2 * statcap) return warnCol;
  return critCol;
}

/*
UINT Observer::AlertStatus(double shipval, int typeNum, UINT badCol, 
			   UINT midCol, UINT safeCol)
{
  / This function takes a value and an array of values.  Comparing the 
     input value to the table value, it will return a color value of 
     suitable color for the predetermined status.  The values are 
     currently dummied; 50% status earns the warning color, and 20% the
     red.
  /

  if(shipval > 12.5)
    return safeCol;
  else if(shipval > 5.0)
    return midCol;
  else
    return badCol;
}
*/

void Observer::plotStatusWins(int Teamnum, Pixmap tCanvas) 
{
  // Title breakdown:01234567890123456789012345678901234567890123456
  char title[256] = "Ship          SHD   Fuel/Cap Vinyl/Cap\0";
  char bldstr[256];  // Build-string, the buffer to hold txt output

  unsigned long tmcol = teamcol[Teamnum];
  int font_width,font_height, xpos,ypos;
  font_height = font_info->ascent + font_info->descent;
  font_width = font_info->max_bounds.width;
  ypos=0;

  if (myWorld==NULL) return;
  CTeam *pTeam = myWorld->GetTeam(Teamnum);
  if (pTeam==NULL) return;

  // Draw team text here, as good a place as any :)
  pTeam->MsgText[maxTextLen-1]=0;
  printMsg (pTeam->MsgText, tmcol);
  pTeam->MsgText[0]=0;  // Printed it, now don't print again

  // Display the team name, score, and wall-clock
  XSetForeground(display, gc, tmcol);
  sprintf(bldstr,"%2.2d: %-45.45s",
	  pTeam->GetTeamNumber(), pTeam->GetName());
  ypos+=font_height;
  XDrawString(display,tCanvas,gc, 
	      5,ypos,
	      bldstr,strlen(bldstr));

  XSetForeground(display,gc, gray);
  sprintf (bldstr,"Time: %.2f",pTeam->GetWallClock());
  ypos+=font_height;
  XDrawString(display,tCanvas,gc, 
	      5,ypos,
	      bldstr,strlen(bldstr));

  XSetForeground(display, gc, tmcol);
  sprintf(bldstr,"%14s: %.3f",
	  pTeam->GetStation()->GetName(), 
	  pTeam->GetScore());
  XDrawString(display,tCanvas,gc, 
	      5+font_width*15,ypos,
	      bldstr,strlen(bldstr));

  // Display the title line 
  // Don't need buffer, color already white
  ypos+=font_height;
  XSetForeground(display,gc, gray);
  XDrawString (display,tCanvas,gc,
	       5,ypos,
	       title,strlen(title));

  CShip *pSh;
  double dAmt,dCap;
  UINT color;

  for (UINT shnum=0; shnum<pTeam->GetShipCount(); shnum++) {
    pSh = pTeam->GetShip(shnum);
    if (pSh==NULL) continue;   // Ship is Dead.

    ypos+=font_height;  // Next line down
    xpos=5;

    // Print ship's name
    XSetForeground (display,gc, tmcol);
    sprintf (bldstr,pSh->GetName());
    XDrawString (display,tCanvas,gc,
		 xpos,ypos,
		 bldstr, strlen(bldstr));

    // Shields
    xpos = 5+13*font_width;
    dAmt = pSh->GetAmount(S_SHIELD);
    color = AlertStatus(dAmt,25.0);  // What waste...
    XSetForeground (display,gc,color);
    sprintf (bldstr," %.1f",dAmt);
    XDrawString (display,tCanvas,gc,
		 xpos,ypos,
		 bldstr, strlen(bldstr));
    
    // Fuel
    xpos = 5+19*font_width;
    dAmt = pSh->GetAmount(S_FUEL);
    dCap = pSh->GetCapacity(S_FUEL);
    color = AlertStatus(dAmt,dCap);
    XSetForeground (display,gc,color);
    sprintf (bldstr," %.1f/%.1f",dAmt,dCap);
    if (pSh->IsDocked()) sprintf(bldstr," Docked");
    XDrawString (display,tCanvas,gc,
		 xpos,ypos,
		 bldstr, strlen(bldstr));
 
    // Vinyl
    xpos = 5+29*font_width;
    dAmt = pSh->GetAmount(S_CARGO);
    dCap = pSh->GetCapacity(S_CARGO);
    XSetForeground (display,gc,white);
    sprintf (bldstr," %.1f/%.1f",dAmt,dCap);
    XDrawString (display,tCanvas,gc,
		 xpos,ypos,
		 bldstr, strlen(bldstr));
  }
}

void Observer::initMsg( void )
{
    int font_width,font_height;
    font_height = font_info->ascent + font_info->descent;
    font_width = font_info->max_bounds.width; 
    
    msg_rows = (msgHeight - (msgHeight%font_height))/font_height;
    msg_cols = ((msgWidth-5) - ((msgWidth-5)%font_width))/font_width;

    msg_r = 0;
    msg_c = 0;
    XSetForeground(display, gc, black);
    XFillRectangle(display, msgCanvas, gc, 0, 0, msgWidth, msgHeight);

}

void Observer::scrollUp( void )
{
    int font_width,font_height;
    font_height = font_info->ascent + font_info->descent;
    font_width = font_info->max_bounds.width;
    
    XCopyArea( display, msgCanvas, msgCanvas, gc, 
	       0, font_height,
	       msgWidth,font_height * (msg_rows - 1),
	       0, 0 );
    XSetForeground(display, gc, black );
    XFillRectangle(display, msgCanvas, gc, 0, 
		   font_height * (msg_rows - 1), msgWidth, msgHeight);
}

void Observer::printMsg( char *str, int color )
{
    char tmpstr[]="a";
    int font_width,font_height;
    font_height = font_info->ascent + font_info->descent;
    font_width = font_info->max_bounds.width;
    int i=0;

    while( *str )
    {
	if( i > 6 )
	    break;

	if( *str == '\n' )
	{
	    i++;
	    msg_c = 0; msg_r++;
	}
	else
	{
	    tmpstr[0] = *str;
	    
	    XSetForeground(display, gc, color );
	    XDrawString(display,msgCanvas,gc,
			(msg_c * font_width) + 5, 
			(msg_r * font_height) + font_info->ascent,
			tmpstr, 1);
	    msg_c++;
	}
	
	if( msg_c >= msg_cols )
	{
	    i++;
	    msg_c = 0;
	    msg_r++;
	}
	
      
	if( msg_r >= msg_rows )
	{
	    scrollUp();
	    msg_r = msg_rows-1;
	}
	str++;
    }
}

void Observer::printGameTime( double game_time )
{
    char str[256];
    
    sprintf(str, "               Game Time: %.1f", game_time );
    XSetForeground(display, gc, white );
    XDrawString(display,timeCanvas,gc,
		0, (int)(font_info->ascent * 1.5),
		str, strlen(str));
} 

void Observer::drawAll()
{
  printGameTime(myWorld->GetGameTime());

  // Add exposing of status panels here
  XCopyArea(display, timeCanvas, win, gc, 0, 0, timeWidth, timeHeight,
	    timeX, timeY);
  XCopyArea(display, t1Canvas, win, gc, 0, 0, tWidth, tHeight,
	    t1PosX, t1PosY);
  XCopyArea(display, msgCanvas, win, gc, 0, 0, msgWidth, msgHeight,
	    msgPosX, msgPosY);
  XCopyArea(display, t2Canvas, win, gc, 0, 0, tWidth, tHeight,
	    t2PosX, t2PosY);

  if( attractor )
  {
      XSetClipMask(display, gc,logoClip);
      XSetClipOrigin(display, gc, 
		     (spaceWidth - logoW)/2,
		     (spaceHeight - logoH)/2);
      XCopyArea(display, logoPix, spaceCanvas, gc, 0, 0, logoW, logoH,
		(spaceWidth - logoW)/2, (spaceHeight - logoH)/2); 
      XSetClipMask(display, gc, None);
  }
  //  cout << "Copying spaceCanvas to canvas" << endl;
  XCopyArea(display, spaceCanvas, win, gc, 0, 0, spaceWidth, spaceHeight,
	    borderX, borderY);

  drawCredits();

  // Make sure that we can see it
  XFlush(display);
}

void Observer::drawCredits()
{
  XSetFont(display,gc,font_info->fid);
  XSetForeground(display,gc, black);

  char cr1[]="MechMania IV:                    'S'             'N'            'V'              'G'";
  char cr2[]="The Vinyl Frontier               Stars           Names          Velocities       Graphics";

  int sx= 5*borderX;
  int sy1= spaceHeight + 2*borderY + font_info->ascent+font_info->descent;
  int sy2= sy1 + 2*font_info->ascent;

  XDrawString(display,win,gc, sx,sy1, cr1,strlen(cr1));
  XDrawString(display,win,gc, sx,sy2, cr2,strlen(cr2));
}


void Observer::getKeystroke()
{
  XEvent event;
  KeySym key;
  int result;

  char text[32];

  result = XCheckWindowEvent(display, win, KeyPressMask, &event);
  if (result==0) return;  // No events to pick up.  

  if (event.type != KeyPress) return;
  if (XLookupString(&event.xkey,text,31,&key,0) != 1) return;

  switch (text[0]) {

    case 'g':
    case 'G':
      setUseXpm(!getUseXpm());
      break;

    case 'v':
    case 'V':
      setUseVelVectors(!getUseVelVectors());
      break;

    case 's':
    case 'S':
      myViewer->bstarplot ^= 1;  // XOR with 1
      break;

    case 'n':
    case 'N':
      drawnames++;
      drawnames%=3;
      break;

    default:
      break;
  }
}

void Observer::setAttractor( int val )
{
    attractor = val;
}

BOOL Observer::getUseXpm()
{
  return useXpm;
}

BOOL Observer::setUseXpm(BOOL newState)
{
  useXpm = newState;
  return newState;
}

BOOL Observer::getUseVelVectors()
{
  return useVelVectors;
}

BOOL Observer::setUseVelVectors(BOOL newState)
{
  useVelVectors = newState;
  return useVelVectors;
}
