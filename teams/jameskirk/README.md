# James Kirk Team - Engine Exploit Demonstration

## Purpose

The James Kirk team is a special demonstration team that showcases various exploits that existed in the original MechMania IV engine (circa 1998). These exploits demonstrate interesting vulnerabilities in the game architecture that competitive teams could have discovered and potentially used during the original competition.

## Current Exploit: Laser Power Manipulation

The team currently demonstrates one primary exploit:

### TOCTOU Laser Exploit
- **What it does**: Fires extremely high-powered lasers (9999 miles) while only paying the fuel cost for the maximum validated laser distance (512 miles)
- **How it works**: Exploits a Time-Of-Check-Time-Of-Use (TOCTOU) vulnerability in `World::LaserModel()`
- **Technical details**:
  - The engine reads `GetOrder(O_LASER)` to compute damage
  - Then calls `SetOrder(O_LASER)` to validate and deduct fuel
  - Between these two operations, we directly manipulate the orders array
  - Uses the `KobayashiMaru` class to expose protected CShip members via C-style casting

## Running the Team

**CRITICAL REQUIREMENT**: This team's exploits ONLY work with the legacy laser exploit flag enabled:

```bash
# Option 1: Enable only the laser exploit
./mm4serv --legacy-laser-exploit

# Option 2: Enable all legacy features (includes laser exploit)
./mm4serv --legacy-mode
```

**Without one of these flags, the exploits will NOT work.** The modern engine (default behavior) has patched the TOCTOU vulnerability. The laser exploit validates laser power BEFORE firing in the new mode, preventing the KobayashiMaru attack from working.

## Team Behavior

Beyond demonstrating exploits, the James Kirk team implements:
- Combat-focused AI with friend-or-foe detection
- Fuel management (seeks fuel asteroids when < 15 fuel)
- Shield management
- Star Trek-themed combat messages ("Fire phasers!", "Engage!", etc.)
- Pure combat role with no cargo capacity

## Files

- `JamesKirk.h/C` - Main team implementation with combat AI
- `KobayashiMaru.h/C` - Exploit helper class that exposes protected CShip members
- `README.md` - This file

## Historical Context

This team serves as an educational tool to understand:
1. How game engine vulnerabilities could be discovered and exploited
2. The importance of proper validation and access control in game design
3. Common programming vulnerabilities from the late 1990s C++ era

## Ethical Note

These exploits are preserved for historical and educational purposes. In a real competition, using such exploits would likely have been against the rules and spirit of fair play. The team demonstrates these vulnerabilities to help understand security considerations in game engine design.