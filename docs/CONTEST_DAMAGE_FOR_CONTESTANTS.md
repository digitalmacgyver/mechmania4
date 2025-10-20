# Contest Damage Reference (Contestants)

> **Scope.** This guide covers the **default damage model** used in the current tournament server. Legacy compatibility switches that recreate the 1998 contest have different balance and are not described here.

## 1. Shields & Destruction
- Each ship starts with shields (default capacity 8000). Shields prevent hull destruction.
- When cumulative damage drives shields to 0 or below, the ship is destroyed and removed from play.
- Shields do **not** regenerate passively. Teams must pay fuel (1 fuel → 1 shield) via `O_SHIELD`.

## 2. Collision Damage
- Collisions transfer momentum. Both participants take equal damage based on how much their momentum changed (1000 damage = 1 shield unit depleted).
- **Ship ↔ Ship:** Always damages both ships. Perfectly elastic collision means they bounce apart; energy is conserved but their shield totals drop according to the momentum exchange.
- **Ship ↔ Asteroid:**
  - **Fits in hold:** The collision becomes perfectly inelastic and no damage is applied (the ship "eats" the asteroid).
  - **Too large:** Treated as an elastic impact; the ship takes damage and the asteroid fragments.
- **Ship ↔ Station:** Docking causes no damage. Enemy stations are intangible for collisions; only lasers can hurt them.
- **Asteroid ↔ Station/Ship (after fragmentation):** Each fragment is processed independently using the same rules.

### Docked immunity
Ships in docked state cannot be hit by anything. They can only take damage again after undocking.

## 3. Lasers
- **Fuel cost:** Beam length ÷ 50 (default). Ships cannot fire while docked.
- **Damage formula:** `damage = 30 × (beam_length − distance_to_target)` (with default scale)
  - **Example:** A 200-unit beam hitting a target 100 units away deals 3000 damage (depleting 3.0 shield units)
- **Efficiency:** Close shots are the most efficient; distant targets waste energy travelling. Point-blank beams typically trade 1 fuel for ~1.5 fuel worth of shields (meaning the defender needs ~1.5 fuel to restore the shields lost).
- **Momentum push:** Hits nudge targets along the beam direction. This can alter their path even if shields absorb the energy.

### Targets
- **Ships:** Lose shields based on damage dealt.
  - **Conversion:** 1000 damage = 1 shield unit depleted
  - If shields drop below zero, the ship explodes.
- **Stations:** Lose stored vinyl based on damage dealt.
  - **Conversion:** 1000 damage = 1 vinyl ton removed
  - Vinyl is clamped at zero (cannot go negative).
- **Asteroids:** Must receive at least 1000 damage to shatter.
  - **Threshold:** 1000 damage shatters asteroid into three fragments
  - **Required beam:** At least 33.33 units of remaining beam length (since damage = 30 × remaining_length)
  - Damage below 1000 has no effect (does not accumulate).

## 4. Jettisoned Cargo
- Jettisoning converts cargo or fuel into free-floating asteroids. These new asteroids behave identically to natural ones: they can be eaten, collided with, or shot.
- Cargo dumped during a turn immediately adjusts the ship’s mass, enabling larger asteroids to be collected moments later.

## 5. Worked Examples

### Example 1: Laser Attack on Enemy Ship
**Scenario:** Ship A fires a laser at Ship B, which is 50 units away. Ship A spends 5 tons of fuel on the laser.

**Step-by-step calculation:**
1. Fuel spent = 5 tons
2. Beam length purchased = 5 tons × 50 units/ton = 250 units
3. Distance to target = 50 units
4. Remaining beam length past target = 250 - 50 = 200 units
5. Damage formula: damage = 30 × (remaining beam length) = 30 × 200 = **6000 damage**
6. Shield loss = 6000 damage ÷ 1000 = **6.0 shield units depleted**

### Example 2: Head-on Ship Collision
**Scenario:** Ship A (70 tons) traveling at 10 units/second eastward collides with Ship B (70 tons) traveling at 10 units/second westward.

**Step-by-step calculation:**
1. Ship A mass = 70 tons, velocity = +10 units/second (east)
2. Ship B mass = 70 tons, velocity = -10 units/second (west)
3. Relative velocity = 10 - (-10) = 20 units/second
4. For elastic collision between equal masses: |Δp| = m × v_rel = 70 × 20 = 1400
   - (General formula: |Δp| = (2 × m₁ × m₂ / (m₁ + m₂)) × v_rel, which simplifies to m × v_rel when m₁ = m₂)
5. Damage formula: damage = |Δp| / g_laser_damage_mass_divisor = 1400 / 1000 = **1.4**
6. Shield loss for each ship = **1.4 shield units depleted**

**Note:** Both ships take equal damage regardless of which was "moving faster" - only the relative velocity matters. Lighter ships are knocked back farther but still take the same absolute damage.

### Example 3: Asteroid Collection
**Scenario A (success):** Ship C (50 tons total: 40-ton hull + 10 tons fuel) with 10 tons of free cargo space collides with an 8-ton vinyl asteroid at low speed.

**Step-by-step:**
1. Asteroid mass = 8 tons
2. Ship free cargo capacity = 10 tons
3. Since 8 tons < 10 tons, the asteroid fits
4. Collision type: **Perfectly inelastic** (ship absorbs asteroid)
5. Damage = **0** (no shield loss when collecting)
6. Result: Ship gains 8 tons of vinyl cargo

**Scenario B (collision):** Same ship collides with a 14-ton vinyl asteroid.

**Step-by-step:**
1. Asteroid mass = 14 tons
2. Ship free cargo capacity = 10 tons
3. Since 14 tons > 10 tons, the asteroid does NOT fit
4. Collision type: **Elastic** (both bounce)
5. Damage formula: |Δp| = (2 × m₁ × m₂ / (m₁ + m₂)) × v_rel
6. Assuming relative velocity = 15 units/second:
   - |Δp| = (2 × 50 × 14 / (50 + 14)) × 15
   - |Δp| = (1400 / 64) × 15
   - |Δp| = 21.875 × 15 = 328.125
7. Damage = 328.125 / 1000 = **328 damage**
8. Shield loss for ship = 328 ÷ 1000 = **0.328 shield units depleted**
9. Asteroid also takes 328 damage, causing it to shatter into three fragments (each ~4.67 tons)

## 6. Damage Ranges and Strategic Considerations

### Maximum Theoretical Damage Values
Understanding worst-case damage helps with risk assessment:

**Ship-Ship Collisions:**
- Ship mass range: 40-100 tons (40-ton hull + 0-60 tons fuel/cargo)
- Maximum relative velocity: 60 units/second (both ships at max speed, head-on)
- Maximum damage: ~6.0 shield units per ship
  - Example: Two 100-ton ships colliding head-on at 30 units/second each
  - |Δp| = 100 × 60 = 6000, damage = 6000/1000 = 6.0

**Ship-Asteroid Collisions:**
- Natural asteroid mass range: 3-40 tons
- Maximum damage: ~2.4 shield units
  - Example: Ship vs 40-ton asteroid at 60 units/second relative velocity
  - |Δp| ≈ 2400, damage ≈ 2.4

**Laser Damage:**
- Maximum beam length: 512 units (for 1024×1024 map)
- Point-blank minimum distance: 24 units (sum of two ship radii = 12+12)
- Maximum single laser: 30 × (512 - 24) / 1000 = **14.64 shield units**

### Laser Efficiency Break-Even Analysis

Lasers are most fuel-efficient at close range but become increasingly wasteful at distance:

**Point-Blank Range (24 units):**
- Attacker spends: 1 fuel
- Defender needs: ~1.5 fuel to restore shields
- **Result: Lasers are 50% more fuel-efficient than shields**

**Break-Even Distance (Db = L/3):**
- For a beam of length L, efficiency equals shields at distance L/3
- Example: 300-unit beam breaks even at 100 units
- **Result: Equal fuel trade**

**Beyond Break-Even:**
- At 2L/3: Lasers cost **2x more** fuel than shield restoration
- At 7L/9: Lasers cost **3x more** fuel
- At 14L/15: Lasers cost **10x more** fuel
- **Strategy: Don't shoot distant targets unless tactical reasons justify the waste**

### Practical Strategy Implications

**When to Use Lasers:**
1. Target is within L/3 of your beam length (fuel-efficient)
2. Target has no fuel to raise shields (they can't defend)
3. Tactical momentum push is needed (alter their trajectory)
4. Destroying the target is worth fuel inefficiency

**When to Avoid Lasers:**
1. Target is beyond 2L/3 (severely fuel-inefficient)
2. You're low on fuel (prioritize shields/thrust)
3. Target is about to dock (waste of fuel if they reach safety)

For detailed formulas and constants, consult `CONTEST_DAMAGE_FOR_DEVS.md`.
