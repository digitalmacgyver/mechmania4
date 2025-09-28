#include "Coord.h"
#include "Traj.h"

/*

Build with:

g++ -I../../team/src ../../team/src/Coord.C ../../team/src/Traj.C ../../team/src/Sendable.C main.C

*/

void set_order_results(CTraj v, double rho, double theta, double maxspeed = 30.0) {
  CTraj w( rho, theta );
  CTraj final_vel = v + w;

  printf("v: %g, %g\n", v.rho, v.theta);
  printf("rho: %g\n", w.rho);
  printf("theta: %g\n", w.theta);
  printf("final_vel: %g, %g\n", final_vel.rho, final_vel.theta);

  if ( final_vel.rho > maxspeed ) {
    final_vel.rho = maxspeed;
  }
  printf("final_vel_clamped: %g, %g\n", final_vel.rho, final_vel.theta);
  final_vel -= v;
  printf("final_rho_result: %g\n", final_vel.rho);

  CTraj accel( final_vel.rho, theta );
  printf("accel: %g, %g\n", accel.rho, accel.theta);
  for ( int i = 0; i < 5; i++ ) {
    v += (accel * 0.2);
    printf("engine step %d - v before clamping: %g, %g\n", i, v.rho, v.theta);
    if ( v.rho > maxspeed ) {
      v.rho = maxspeed;
      printf("engine step %d - v clamped: %g, %g\n", i, v.rho, v.theta);
    }
  }
}

void test1() {
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

void do_tests() {
  //test1();

  CTraj v( 30.0, 0.0 );
  double rho = 30.0;
  double theta = PI / 2;
  //set_order_results( v, rho, theta );

  CTraj v2( 30.0, 0.7 );
  double rho2 = 3.36;
  double theta2 = 1.1;
  set_order_results( v2, rho2, theta2 );
}

int main() {
  do_tests();
  return 0;
}
