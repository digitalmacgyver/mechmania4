#!/usr/bin/env python3
"""
Physics Simulator Examples and Prototyping
Use this file to test navigation strategies and physics intuition.
"""

from physics_sim import *
import math


def test_thrust_mechanics():
    """Test how thrust affects velocity."""
    print("THRUST MECHANICS TEST")
    print("-" * 40)

    ship = Ship(0, 0)
    ship.set_name("TestShip")
    ship.orient = 0  # Facing right

    print(f"Initial: pos={ship.pos}, vel={ship.vel}, orient={ship.orient:.2f}")

    # Apply thrust
    for i in range(5):
        ship.set_thrust(10, dt=1.0)  # 10 units/s acceleration for 1 second
        ship.drift(1.0)  # Move for 1 second
        print(f"After {i+1}s: pos={ship.pos}, vel={ship.vel}")

    print("\nNote: Velocity is clamped at MAX_SPEED=30.0")


def test_wrapping_navigation():
    """Test navigation across world boundaries."""
    print("\nWRAPPING NAVIGATION TEST")
    print("-" * 40)

    # Ship near edge
    ship = Ship(500, 0)
    target = Ship(-500, 0)

    # Direct distance would be 1000, but with wrapping...
    dist = ship.pos.dist_to(target.pos)
    print(f"Ship at {ship.pos}")
    print(f"Target at {target.pos}")
    print(f"Distance with wrapping: {dist:.2f} (not 1000!)")

    # Vector to target uses wrapping
    vec = ship.pos.vect_to(target.pos)
    print(f"Vector to target: {vec}")
    print(f"Should go left (through boundary): angle={vec.theta:.2f} rad")


def test_interception_strategy():
    """Test intercepting a moving target."""
    print("\nINTERCEPTION STRATEGY TEST")
    print("-" * 40)

    hunter = Ship(0, 0)
    hunter.set_name("Hunter")
    hunter.orient = 0  # Facing right

    # Target moving in a circle
    target = Ship(100, 0)
    target.set_name("Target")
    target.set_vel(Traj(20, PI/2))  # Moving down

    print("Simulating 10 turns of pursuit:")
    for turn in range(10):
        # Calculate intercept angle
        angle = hunter.angle_to_intercept(target, dt=2.0)

        # Turn towards intercept
        hunter.orient += angle * 0.5  # Turn halfway

        # Thrust towards target
        hunter.set_thrust(15, dt=1.0)

        # Update positions
        hunter.drift(1.0)
        target.drift(1.0)

        dist = hunter.pos.dist_to(target.pos)
        print(f"Turn {turn+1}: Distance={dist:.1f}, Hunter vel={hunter.vel.rho:.1f}")

        if dist < 20:
            print("INTERCEPT!")
            break


def test_orbit_calculation():
    """Calculate orbit around a point."""
    print("\nORBIT CALCULATION TEST")
    print("-" * 40)

    center = Coord(0, 0)
    radius = 100
    speed = 20

    print(f"Orbiting at radius={radius}, speed={speed}")

    # Create ship at starting position
    ship = Ship(radius, 0)
    ship.set_name("Orbiter")

    # Calculate required velocity for circular orbit
    # Velocity should be tangent to circle
    ship.set_vel(Traj(speed, PI/2))  # Moving down (tangent at right side)

    print("\nOrbit positions:")
    for i in range(8):
        dist = ship.pos.dist_to(center)
        print(f"t={i}: pos={ship.pos}, dist from center={dist:.1f}")

        # Adjust heading to maintain orbit
        angle_to_center = ship.pos.angle_to(center)
        tangent_angle = angle_to_center + PI/2  # 90 degrees from radius

        # Set new velocity tangent to circle
        ship.set_vel(Traj(speed, tangent_angle))
        ship.drift(1.0)


def test_collision_avoidance():
    """Test detecting and avoiding collisions."""
    print("\nCOLLISION AVOIDANCE TEST")
    print("-" * 40)

    ship = Ship(0, 0)
    ship.set_vel(Traj(20, 0))  # Moving right

    # Create obstacles
    obstacles = [
        Ship(50, 5),   # Slightly off path
        Ship(100, 0),  # Directly in path
        Ship(150, -30) # Well clear
    ]

    for obs in obstacles:
        obs.set_size(15)

    print("Checking collision threats:")
    for i, obs in enumerate(obstacles):
        # Simple collision prediction: will paths intersect?
        future_ship = ship.predict_position(5.0)
        future_obs = obs.predict_position(5.0)

        future_dist = future_ship.dist_to(future_obs)
        threat_threshold = ship.size + obs.size + 10  # Safety margin

        if future_dist < threat_threshold:
            print(f"Obstacle {i+1}: THREAT! Future dist={future_dist:.1f}")
            # Calculate avoidance angle
            avoid_angle = ship.pos.angle_to(obs.pos) + PI/2
            print(f"  Suggest turn to angle: {avoid_angle:.2f}")
        else:
            print(f"Obstacle {i+1}: Safe, future dist={future_dist:.1f}")


def test_velocity_composition():
    """Test how velocities add (important for SetOrder in C++)."""
    print("\nVELOCITY COMPOSITION TEST")
    print("-" * 40)

    print("Starting with velocity (30, 0) - moving right at max speed")
    v = Traj(30, 0)

    print("\nTrying to add perpendicular thrust:")
    thrust = Traj(30, PI/2)  # Down
    new_v = v + thrust

    print(f"Original: {v}")
    print(f"Thrust: {thrust}")
    print(f"Result: {new_v}")
    print(f"Result magnitude: {new_v.rho:.2f} (exceeds MAX_SPEED!)")

    if new_v.rho > MAX_SPEED:
        new_v.rho = MAX_SPEED
        print(f"Clamped: {new_v}")

    # Calculate actual acceleration achieved
    actual_accel = new_v - v
    print(f"Actual acceleration: {actual_accel}")
    print(f"Note: Magnitude is {actual_accel.rho:.2f}, not 30!")


# Run all tests
if __name__ == "__main__":
    test_thrust_mechanics()
    test_wrapping_navigation()
    test_interception_strategy()
    test_orbit_calculation()
    test_collision_avoidance()
    test_velocity_composition()