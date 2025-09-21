This directory contained a revised version of the server code and the
Team Groogroo code for the 1998 UIUC MechMania IV: The Vinyl Frontier
competition.

TL;DR: To see the simulation on Linux:
$cd build
$cmake ..
$make -j
$./run_groogroo.sh

Details:

For information about the contest, consult CONTEST_RULES.md

For information about how to build and run the code on Linux, consult
build/README.md and README_RUN_CONTEST.md

There are three major components:

* Modernized Code - An updated version of the MMIV code that can be
  built and run on modern linux:
  * build/ target directory for build outputs
  * team/src/ The MechMania IV server code and example team (Chrome
    Funkadelic)
  * teams/ Implementations of alternative teams:
    * groogroo/ Team Groogroo
    * chromefunk/ The example Chrome Funkadelic team broken out into
      its own directory
    * vortex/ A team Claude Code built

* legacy_code/ - A snapshot of the original 1998 server code and some
  Team Groogroo files - this is not functional and included for
  historic interest only. See legacy_code/README.txt for more.

* Docker related files - Configuration and setup for running a
  dockerized version of the contest, see:
  * README_DOCKER_USER.md for instructions on how to setup / run the
    contest code
  * README_DOCKER_DEV.md for details on the docker containerization

