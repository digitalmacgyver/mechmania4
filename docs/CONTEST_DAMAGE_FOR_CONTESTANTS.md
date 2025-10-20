# Contest Damage Reference (Contestants)

> **Scope.** This guide covers the **default damage model** used in the current tournament server. Legacy compatibility switches that recreate the 1998 contest have different balance and are not described here.

## 1. Shields & Destruction
- Each ship starts with shields (default capacity 8000). Shields prevent hull destruction.
- When cumulative damage drives shields to 0 or below, the ship is destroyed and removed from play.
- Shields do **not** regenerate passively. Teams must pay fuel (1 fuel → 1 shield) via `O_SHIELD`.

## 2. Collision Damage
- Collisions transfer momentum. Both participants take equal shield damage based on how much their momentum changed.
- **Ship ↔ Ship:** Always damages both ships. Perfectly elastic collision means they bounce apart; energy is conserved but their shield totals drop according to the momentum exchange.
- **Ship ↔ Asteroid:**
  - **Fits in hold:** The collision becomes perfectly inelastic and no shield damage is applied (the ship “eats” the asteroid).
  - **Too large:** Treated as an elastic impact; the ship takes damage and the asteroid fragments.
- **Ship ↔ Station:** Docking causes no damage. Enemy stations are intangible for collisions; only lasers can hurt them.
- **Asteroid ↔ Station/Ship (after fragmentation):** Each fragment is processed independently using the same rules.

### Docked immunity
Ships in docked state cannot be hit by anything. They can only take damage again after undocking.

## 3. Lasers
- **Fuel cost:** Beam length ÷ 50 (default). Ships cannot fire while docked.
- **Damage:** Proportional to the length of unused beam energy at impact. Close shots are the most efficient; distant targets waste energy travelling. Point-blank beams typically trade 1 fuel for ~1.5 fuel worth of shield damage.
- **Momentum push:** Hits nudge targets along the beam direction. This can alter their path even if shields absorb the energy.

### Targets
- **Ships:** Lose shields. If shields drop below zero, the ship explodes.
- **Stations:** Lose stored vinyl one point per damage until empty.
- **Asteroids:** If the beam deals at least 1 point, the asteroid shatters into three fragments. Otherwise the beam has no effect.

## 4. Jettisoned Cargo
- Jettisoning converts cargo or fuel into free-floating asteroids. These new asteroids behave identically to natural ones: they can be eaten, collided with, or shot.
- Cargo dumped during a turn immediately adjusts the ship’s mass, enabling larger asteroids to be collected moments later.

## 5. Worked Examples
- **Laser duel:** Spending 5 tons of fuel on a point-blank beam deals roughly 150 shield damage to the target (damage ≈ 30 × (beam_length − distance) / 1000).
- **Head-on rams:** Two identical 70-ton ships meeting at 20 u/s lose about the same shields (equal momentum change). Lighter ships are knocked back more but take the same absolute damage as the heavier partner.
- **Asteroid harvest:** A ship with 10 tons of free cargo colliding with an 8-ton vinyl rock absorbs it cleanly (no damage). If it collides with a 14-ton rock instead, both objects bounce and the asteroid splits.

Consult the developer damage guide for formulas and configuration constants.
