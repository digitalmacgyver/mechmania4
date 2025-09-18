#include "Coord.h"
#include "Traj.h"

void main() {
  CCoord a(0,-511);
  CCoord b(0,511);
  CCoord t1(0,1024);
  CCoord t2(1024,0);
  CCoord t3(1024,1024);
  CCoord b1 = b-t1;
  CCoord b2 = b-t2;
  CCoord b3 = b-t3;

  CTraj r=a.VectTo(b);  
  CTraj r1=a.VectTo(b1);
  CTraj r2=a.VectTo(b2);
  CTraj r3=a.VectTo(b3);

  printf("Location of b: %g, %g\n", b.fX, b.fY);
  printf("Angle to b : %g\n", r.theta);
  printf("Distance to b : %g\n", r.rho);

  printf("Location of b1: %g, %g\n", b1.fX, b1.fY);
  printf("Angle to b1: %g\n", r1.theta);
  printf("Distance to b1 : %g\n", r1.rho);

  printf("Location of b2: %g, %g\n", b2.fX, b2.fY);
  printf("Angle to b2: %g\n", r2.theta);
  printf("Distance to b2 : %g\n", r2.rho);

  printf("Location of b3: %g, %g\n", b3.fX, b3.fY);
  printf("Angle to b3: %g\n", r3.theta);
  printf("Distance to b3 : %g\n", r3.rho);

}
