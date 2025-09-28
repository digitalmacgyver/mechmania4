#!/usr/bin/env python3
"""
MechMania IV Physics Simulator
A Python implementation of the core physics and geometry from the game.

COORDINATE SYSTEM:
- World ranges from (-512, -512) to (512, 512)
- +Y points DOWNWARD on screen
- Angles: 0 = right (+X), PI/2 = down (+Y), PI = left (-X), -PI/2 = up (-Y)
"""

import math
from typing import Tuple, Optional

# World boundaries
WORLD_X_MIN = -512.0
WORLD_Y_MIN = -512.0
WORLD_X_MAX = 512.0
WORLD_Y_MAX = 512.0
WORLD_SIZE_X = WORLD_X_MAX - WORLD_X_MIN
WORLD_SIZE_Y = WORLD_Y_MAX - WORLD_Y_MIN

# Constants
PI = math.pi
PI2 = 2 * math.pi
MAX_SPEED = 30.0
MIN_MASS = 3.0
MIN_SIZE = 1.0


class Coord:
    """2D coordinate with toroidal wrapping."""

    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.fX = x
        self.fY = y
        self.wrap()

    def wrap(self):
        """Apply toroidal wrapping to keep coordinates in world bounds."""
        # Wrap X coordinate
        offset_x = self.fX - WORLD_X_MIN
        if offset_x < 0:
            offset_x += WORLD_SIZE_X * (1 + int(-offset_x / WORLD_SIZE_X))
        wrapped_x = math.fmod(offset_x, WORLD_SIZE_X)
        self.fX = wrapped_x + WORLD_X_MIN

        # Wrap Y coordinate
        offset_y = self.fY - WORLD_Y_MIN
        if offset_y < 0:
            offset_y += WORLD_SIZE_Y * (1 + int(-offset_y / WORLD_SIZE_Y))
        wrapped_y = math.fmod(offset_y, WORLD_SIZE_Y)
        self.fY = wrapped_y + WORLD_Y_MIN

    def dist_to(self, other: 'Coord') -> float:
        """Calculate shortest distance to another coordinate (handles wrapping)."""
        dx = abs(self.fX - other.fX)
        dy = abs(self.fY - other.fY)

        # Check if wrapping gives shorter distance
        if dx > WORLD_SIZE_X / 2:
            dx = WORLD_SIZE_X - dx
        if dy > WORLD_SIZE_Y / 2:
            dy = WORLD_SIZE_Y - dy

        return math.sqrt(dx * dx + dy * dy)

    def vect_to(self, other: 'Coord') -> 'Traj':
        """Get trajectory vector to another coordinate (handles wrapping)."""
        # Calculate direct differences
        dx = other.fX - self.fX
        dy = other.fY - self.fY

        # Check if wrapping gives shorter path
        if dx > WORLD_SIZE_X / 2:
            dx -= WORLD_SIZE_X
        elif dx < -WORLD_SIZE_X / 2:
            dx += WORLD_SIZE_X

        if dy > WORLD_SIZE_Y / 2:
            dy -= WORLD_SIZE_Y
        elif dy < -WORLD_SIZE_Y / 2:
            dy += WORLD_SIZE_Y

        rho = math.sqrt(dx * dx + dy * dy)
        theta = math.atan2(dy, dx)

        return Traj(rho, theta)

    def angle_to(self, other: 'Coord') -> float:
        """Get angle to another coordinate."""
        return self.vect_to(other).theta

    def __add__(self, other: 'Coord') -> 'Coord':
        result = Coord(self.fX + other.fX, self.fY + other.fY)
        result.wrap()
        return result

    def __sub__(self, other: 'Coord') -> 'Coord':
        result = Coord(self.fX - other.fX, self.fY - other.fY)
        result.wrap()
        return result

    def __str__(self) -> str:
        return f"Coord({self.fX:.2f}, {self.fY:.2f})"

    def __repr__(self) -> str:
        return self.__str__()


class Traj:
    """Trajectory/vector in polar coordinates (rho=magnitude, theta=angle)."""

    def __init__(self, rho: float = 0.0, theta: float = 0.0):
        self.rho = rho
        self.theta = theta
        self.normalize()

    def normalize(self):
        """Normalize angle to [-PI, PI] range."""
        while self.theta > PI:
            self.theta -= PI2
        while self.theta < -PI:
            self.theta += PI2

    def convert_to_coord(self) -> Coord:
        """Convert trajectory to Cartesian coordinate."""
        x = self.rho * math.cos(self.theta)
        y = self.rho * math.sin(self.theta)
        return Coord(x, y)

    def __add__(self, other: 'Traj') -> 'Traj':
        """Add two trajectories (velocity addition)."""
        x1 = self.rho * math.cos(self.theta)
        y1 = self.rho * math.sin(self.theta)
        x2 = other.rho * math.cos(other.theta)
        y2 = other.rho * math.sin(other.theta)

        x_sum = x1 + x2
        y_sum = y1 + y2

        rho = math.sqrt(x_sum * x_sum + y_sum * y_sum)
        theta = math.atan2(y_sum, x_sum)

        return Traj(rho, theta)

    def __sub__(self, other: 'Traj') -> 'Traj':
        """Subtract trajectory."""
        neg_other = Traj(other.rho, other.theta + PI)
        return self + neg_other

    def __mul__(self, scalar: float) -> 'Traj':
        """Multiply trajectory by scalar."""
        return Traj(self.rho * scalar, self.theta)

    def __truediv__(self, scalar: float) -> 'Traj':
        """Divide trajectory by scalar."""
        if scalar == 0:
            return Traj(0, self.theta)
        return Traj(self.rho / scalar, self.theta)

    def __str__(self) -> str:
        return f"Traj(rho={self.rho:.2f}, theta={self.theta:.2f})"

    def __repr__(self) -> str:
        return self.__str__()


class Thing:
    """Simplified game object with position, velocity, and physical properties."""

    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.pos = Coord(x, y)
        self.vel = Traj(0.0, 0.0)
        self.orient = 0.0
        self.mass = 40.0
        self.size = 12.0
        self.name = ""

    # Getters
    def get_pos(self) -> Coord:
        return self.pos

    def get_size(self) -> float:
        return self.size

    def get_orient(self) -> float:
        return self.orient

    def get_velocity(self) -> Traj:
        return self.vel

    def get_momentum(self) -> Traj:
        return self.vel * self.mass

    def get_mass(self) -> float:
        return self.mass

    def get_name(self) -> str:
        return self.name

    # Setters
    def set_mass(self, mass: float):
        if mass >= MIN_MASS:
            self.mass = mass

    def set_orient(self, orient: float):
        self.orient = orient

    def set_size(self, size: float):
        if size >= MIN_SIZE:
            self.size = size

    def set_pos(self, pos: Coord):
        self.pos = pos
        self.pos.wrap()

    def set_vel(self, vel: Traj):
        self.vel = vel
        # Clamp to max speed
        if self.vel.rho > MAX_SPEED:
            self.vel.rho = MAX_SPEED

    def set_name(self, name: str):
        self.name = name[:13]  # Max 13 characters

    # Physics methods
    def overlaps(self, other: 'Thing') -> bool:
        """Check if this thing overlaps with another."""
        dist = self.pos.dist_to(other.pos)
        combined_size = self.size + other.size
        return dist < combined_size

    def predict_position(self, dt: float = 1.0) -> Coord:
        """Predict future position after time dt."""
        displacement = self.vel * dt
        future_pos = self.pos + displacement.convert_to_coord()
        return future_pos

    def relative_velocity(self, other: 'Thing') -> Traj:
        """Calculate relative velocity to another thing."""
        return other.vel - self.vel

    def relative_momentum(self, other: 'Thing') -> Traj:
        """Calculate relative momentum to another thing."""
        rel_vel = self.relative_velocity(other)
        # Use reduced mass for relative momentum
        reduced_mass = (self.mass * other.mass) / (self.mass + other.mass)
        return rel_vel * reduced_mass

    def drift(self, dt: float = 1.0):
        """Update position based on velocity."""
        displacement = self.vel * dt
        self.pos = self.pos + displacement.convert_to_coord()

    def __str__(self) -> str:
        return f"Thing('{self.name}' at {self.pos}, vel={self.vel})"


class Ship(Thing):
    """Ship with additional navigation capabilities."""

    def angle_to_intercept(self, target: Thing, dt: float = 1.0) -> float:
        """
        Calculate angle to turn to intercept a target.
        Returns angle in radians to turn (positive = turn right, negative = turn left).
        """
        # Predict positions
        my_future_pos = self.predict_position(dt)
        target_future_pos = target.predict_position(dt)

        # Get angle to predicted position
        angle_to_target = my_future_pos.angle_to(target_future_pos)

        # Calculate turn angle
        turn = angle_to_target - self.orient

        # Normalize to [-PI, PI]
        while turn > PI:
            turn -= PI2
        while turn < -PI:
            turn += PI2

        return turn

    def set_thrust(self, magnitude: float, dt: float = 1.0):
        """Apply thrust in the direction of ship's orientation."""
        accel = Traj(magnitude, self.orient)
        self.vel = self.vel + (accel * dt)

        # Clamp to max speed
        if self.vel.rho > MAX_SPEED:
            self.vel.rho = MAX_SPEED

    def set_turn(self, angle: float, dt: float = 1.0):
        """Turn the ship by the specified angle."""
        self.orient += angle * dt

        # Normalize orientation
        while self.orient > PI:
            self.orient -= PI2
        while self.orient < -PI:
            self.orient += PI2


# Example usage and tests
if __name__ == "__main__":
    print("MechMania IV Physics Simulator")
    print("=" * 40)

    # Test coordinate wrapping
    print("\n1. Coordinate Wrapping Test:")
    c1 = Coord(400, 400)
    c2 = Coord(-400, -400)
    print(f"c1: {c1}")
    print(f"c2: {c2}")

    # Test wrapping beyond boundaries
    c3 = Coord(600, -600)
    print(f"c3 (600, -600) wraps to: {c3}")

    # Test distance with wrapping
    print(f"Distance c1 to c2 (should use wrapping): {c1.dist_to(c2):.2f}")

    # Test trajectory operations
    print("\n2. Trajectory Operations:")
    v1 = Traj(30, 0)  # Moving right at max speed
    v2 = Traj(30, PI/2)  # Moving down at max speed
    v_sum = v1 + v2
    print(f"v1 (right): {v1}")
    print(f"v2 (down): {v2}")
    print(f"v1 + v2: {v_sum}")

    # Test ship movement
    print("\n3. Ship Movement:")
    ship = Ship(0, 0)
    ship.set_name("TestShip")
    ship.set_vel(Traj(20, PI/4))  # Moving diagonal

    print(f"Initial: {ship}")

    # Predict position after 5 seconds
    future = ship.predict_position(5.0)
    print(f"Position after 5s: {future}")

    # Test interception
    print("\n4. Interception Test:")
    hunter = Ship(-100, 0)
    hunter.set_name("Hunter")
    hunter.orient = 0  # Facing right

    target = Ship(100, 50)
    target.set_name("Target")
    target.set_vel(Traj(10, PI/2))  # Moving down

    turn_angle = hunter.angle_to_intercept(target, 10.0)
    print(f"Hunter needs to turn {turn_angle:.2f} radians ({math.degrees(turn_angle):.1f} degrees)")

    # Test collision detection
    print("\n5. Collision Detection:")
    ship1 = Ship(0, 0)
    ship1.set_size(10)
    ship2 = Ship(15, 0)
    ship2.set_size(10)

    print(f"Ship1 at {ship1.pos}, size={ship1.size}")
    print(f"Ship2 at {ship2.pos}, size={ship2.size}")
    print(f"Ships overlap: {ship1.overlaps(ship2)}")

    ship2.set_pos(Coord(25, 0))
    print(f"Ship2 moved to {ship2.pos}")
    print(f"Ships overlap: {ship1.overlaps(ship2)}")