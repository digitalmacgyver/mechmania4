#!/usr/bin/env python3
"""
Simple test harness for MechMania IV collision handling modes.

This script runs quick test games to verify that both legacy and new
collision handling modes work correctly after code changes.
"""

import subprocess
import os
import sys
import time
import socket
import argparse


# Paths to executables (relative to project root)
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BUILD_DIR = os.path.join(PROJECT_ROOT, 'build')
SERVER_EXEC = os.path.join(BUILD_DIR, 'mm4serv')
OBSERVER_EXEC = os.path.join(BUILD_DIR, 'mm4obs')
GRAPHICS_REG = os.path.join(PROJECT_ROOT, 'team/src/graphics.reg')

# Available team executables
TEAM_EXECS = {
    'groogroo': os.path.join(BUILD_DIR, 'mm4team_groogroo'),
    'testteam': os.path.join(BUILD_DIR, 'mm4team_testteam'),
    'noop': os.path.join(BUILD_DIR, 'mm4team_noop'),
}

# Test configuration
GAME_TIMEOUT = 240  # seconds (4 minutes - matches ga_optimizer)


def find_free_port():
    """Find an available TCP port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        return s.getsockname()[1]


def check_executables(team_names):
    """Verify all required executables exist."""
    execs = [SERVER_EXEC, OBSERVER_EXEC, GRAPHICS_REG]
    for team_name in team_names:
        execs.append(TEAM_EXECS[team_name])

    missing = [exe for exe in execs if not os.path.exists(exe)]
    if missing:
        print(f"\nError: Missing required files:")
        for m in missing:
            print(f"  - {m}")
        print(f"\nPlease run: cmake --build build")
        sys.exit(1)


def run_test_game(mode_name, server_flags, team1='groogroo', team2='groogroo', test_file=None,
                  show_team_output=False, use_stdin=False, max_turns=None):
    """
    Run a single test game with specified server flags.

    Args:
        mode_name: Human-readable name for the mode (e.g., "Legacy", "New")
        server_flags: List of flags to pass to mm4serv
        team1: Team name for player 1 (default: 'groogroo')
        team2: Team name for player 2 (default: 'groogroo')
        test_file: Path to test moves file for testteam (optional)
        show_team_output: If True, capture and display team output (default: False)
        use_stdin: If True, pipe test file to testteam stdin instead of using --test-file (default: False)

    Returns:
        True if game completed successfully, False otherwise
    """
    port = find_free_port()
    processes = []

    print(f"\n{'='*60}")
    print(f"Testing {mode_name} Mode")
    print(f"{'='*60}")
    print(f"Port: {port}")
    print(f"Teams: {team1} vs {team2}")
    print(f"Server flags: {' '.join(server_flags) if server_flags else '(none)'}")

    # Setup process arguments for headless execution
    popen_kwargs = {
        'stdout': subprocess.DEVNULL,
        'stderr': subprocess.DEVNULL
    }

    if sys.platform != "win32":
        popen_kwargs['preexec_fn'] = os.setsid

    # Determine test name from test file path
    import tempfile
    test_name = "unknown"
    if test_file:
        test_basename = os.path.basename(test_file)
        # Extract test name (e.g., "test1" from "test1_ship_station_collision.txt")
        if test_basename.startswith("test"):
            test_name = test_basename.split('_')[0]

    # Create log files with descriptive names
    mode_suffix = mode_name.replace(' ', '_').replace('Collision_Handling', 'Collision')
    server_log_fd, server_log_path = tempfile.mkstemp(
        prefix=f"mm4serv-{test_name}-{mode_suffix}-",
        suffix=".log",
        dir="/tmp"
    )
    os.close(server_log_fd)

    # Create testteam log file if using testteam
    testteam_log_path = None
    testteam_log_file = None
    if team1 == 'testteam' or team2 == 'testteam':
        testteam_log_fd, testteam_log_path = tempfile.mkstemp(
            prefix=f"mm4testteam-{test_name}-{mode_suffix}-",
            suffix=".log",
            dir="/tmp"
        )
        os.close(testteam_log_fd)

    try:
        # 1. Start server with --verbose flag
        server_cmd = [SERVER_EXEC, f"-p{port}", "-T2", "--verbose"]
        if max_turns is not None:
            server_cmd.extend(["--max-turns", str(max_turns)])
        server_cmd.extend(server_flags)
        print(f"Starting server: {' '.join(server_cmd)}")
        print(f"Server log: {server_log_path}")
        if testteam_log_path:
            print(f"TestTeam log: {testteam_log_path}")

        server_log_file = open(server_log_path, 'w')
        server_process = subprocess.Popen(
            server_cmd,
            stdout=server_log_file,
            stderr=subprocess.STDOUT,
            text=True,
            preexec_fn=os.setsid if sys.platform != "win32" else None
        )
        processes.append(server_process)
        time.sleep(0.2)  # Give server time to bind port

        # 2. Start team 1
        team1_cmd = [TEAM_EXECS[team1], f"-p{port}", "-hlocalhost"]
        team1_stdin = None
        team1_kwargs = popen_kwargs.copy()

        if team1 == 'testteam':
            # Add --verbose flag for testteam
            team1_cmd.append('--verbose')
            if test_file:
                if use_stdin:
                    # Pipe test file to stdin
                    print(f"Starting team 1 ({team1}) with stdin piping...")
                    with open(test_file, 'r') as f:
                        team1_stdin = f.read()
                    team1_kwargs['stdin'] = subprocess.PIPE
                else:
                    # Use --test-file flag
                    team1_cmd.extend(['--test-file', test_file])
                    print(f"Starting team 1 ({team1}) with --test-file...")
            # Redirect output to testteam log file
            if testteam_log_path:
                testteam_log_file = open(testteam_log_path, 'w')
                team1_kwargs['stdout'] = testteam_log_file
                team1_kwargs['stderr'] = subprocess.STDOUT
                team1_kwargs['text'] = True
        else:
            print(f"Starting team 1 ({team1})...")

        if show_team_output and team1 == 'testteam':
            team1_kwargs['stdout'] = subprocess.PIPE
            team1_kwargs['stderr'] = subprocess.STDOUT
            team1_kwargs['text'] = True

        p1 = subprocess.Popen(team1_cmd, **team1_kwargs)
        if team1_stdin:
            p1.stdin.write(team1_stdin)
            p1.stdin.close()
        processes.append(p1)

        # 3. Start team 2
        team2_cmd = [TEAM_EXECS[team2], f"-p{port}", "-hlocalhost"]
        team2_stdin = None
        team2_kwargs = popen_kwargs.copy()

        if team2 == 'testteam':
            # Add --verbose flag for testteam
            team2_cmd.append('--verbose')
            if test_file:
                if use_stdin:
                    # Pipe test file to stdin
                    print(f"Starting team 2 ({team2}) with stdin piping...")
                    with open(test_file, 'r') as f:
                        team2_stdin = f.read()
                    team2_kwargs['stdin'] = subprocess.PIPE
                else:
                    # Use --test-file flag
                    team2_cmd.extend(['--test-file', test_file])
                    print(f"Starting team 2 ({team2}) with --test-file...")
            # Redirect output to testteam log file (append mode for team 2)
            if testteam_log_path:
                if not testteam_log_file:
                    testteam_log_file = open(testteam_log_path, 'a')
                team2_kwargs['stdout'] = testteam_log_file
                team2_kwargs['stderr'] = subprocess.STDOUT
                team2_kwargs['text'] = True
        else:
            print(f"Starting team 2 ({team2})...")

        if show_team_output and team2 == 'testteam':
            team2_kwargs['stdout'] = subprocess.PIPE
            team2_kwargs['stderr'] = subprocess.STDOUT
            team2_kwargs['text'] = True

        p2 = subprocess.Popen(team2_cmd, **team2_kwargs)
        if team2_stdin:
            p2.stdin.write(team2_stdin)
            p2.stdin.close()
        processes.append(p2)

        # 4. Start observer (headless)
        observer_cmd = [
            OBSERVER_EXEC, f"-p{port}", "-hlocalhost",
            "-G", "-g", GRAPHICS_REG
        ]
        print(f"Starting observer (headless)...")
        observer_env = os.environ.copy()
        observer_env["SDL_VIDEODRIVER"] = "dummy"
        observer_env["SDL_AUDIODRIVER"] = "dummy"

        observer_kwargs = popen_kwargs.copy()
        observer_kwargs["env"] = observer_env
        p_obs = subprocess.Popen(observer_cmd, **observer_kwargs)
        processes.append(p_obs)

        # 5. Wait for server to complete
        print(f"Game running... (timeout: {GAME_TIMEOUT}s)")
        try:
            server_process.wait(timeout=GAME_TIMEOUT)
            server_log_file.close()
            if testteam_log_file:
                testteam_log_file.close()

            # Read server log to check for completion
            with open(server_log_path, 'r') as f:
                log_contents = f.read()

            # Display team output if requested
            if show_team_output:
                if team1 == 'testteam' and p1.stdout:
                    team1_output = p1.stdout.read() if not p1.stdout.closed else ""
                    if team1_output:
                        print(f"\nTeam 1 ({team1}) Output:")
                        print("=" * 60)
                        print(team1_output)

                if team2 == 'testteam' and p2.stdout:
                    team2_output = p2.stdout.read() if not p2.stdout.closed else ""
                    if team2_output:
                        print(f"\nTeam 2 ({team2}) Output:")
                        print("=" * 60)
                        print(team2_output)

            # Count and report collisions
            collision_count = log_contents.count("COLLISION_DETECTED:")
            laser_collision_count = log_contents.count("LASER_COLLISION:")
            print(f"\nCollision Summary:")
            print(f"  Physical collisions: {collision_count}")
            print(f"  Laser collisions: {laser_collision_count}")

            # Check if game completed successfully
            if "Final Scores" in log_contents or "vinyl" in log_contents.lower():
                print(f"\n✓ {mode_name} mode test PASSED")
                print(f"  Game completed successfully")
                return True
            else:
                print(f"\n✗ {mode_name} mode test FAILED")
                print(f"  Server output did not contain expected results")
                print(f"  Check log file: {server_log_path}")
                return False

        except subprocess.TimeoutExpired:
            print(f"\n✗ {mode_name} mode test FAILED")
            print(f"  Game timed out after {GAME_TIMEOUT}s")
            return False

    except Exception as e:
        print(f"\n✗ {mode_name} mode test FAILED")
        print(f"  Exception: {e}")
        return False

    finally:
        # Close log files if still open
        try:
            server_log_file.close()
        except:
            pass
        try:
            if testteam_log_file:
                testteam_log_file.close()
        except:
            pass

        # Cleanup all processes
        for p in processes:
            try:
                if sys.platform == "win32":
                    p.terminate()
                else:
                    if p.poll() is None:
                        import signal
                        pgid = os.getpgid(p.pid)
                        os.killpg(pgid, signal.SIGTERM)
                        try:
                            p.wait(timeout=0.5)
                        except subprocess.TimeoutExpired:
                            os.killpg(pgid, signal.SIGKILL)
            except (ProcessLookupError, OSError):
                pass


def main():
    """Run collision handling tests."""
    parser = argparse.ArgumentParser(description='Test MechMania IV collision handling modes')
    parser.add_argument('--team1', default='groogroo', choices=TEAM_EXECS.keys(),
                        help='Team for player 1 (default: groogroo)')
    parser.add_argument('--team2', default='groogroo', choices=TEAM_EXECS.keys(),
                        help='Team for player 2 (default: groogroo)')
    parser.add_argument('--test-file', type=str,
                        help='Path to test moves file for testteam (relative to project root)')
    parser.add_argument('--show-team-output', action='store_true',
                        help='Show testteam output for debugging')
    parser.add_argument('--use-stdin', action='store_true',
                        help='Pipe test file to testteam stdin instead of using --test-file flag')
    parser.add_argument('--max-turns', type=int,
                        help='Maximum number of turns (default: 300)')
    parser.add_argument('--legacy-mode', action='store_true',
                        help='Run with --legacy-mode flag (enables all legacy behaviors)')
    args = parser.parse_args()

    print("MechMania IV - Collision Handling Test Harness")
    print("=" * 60)

    # Check prerequisites
    teams_used = {args.team1, args.team2}
    check_executables(teams_used)

    results = {}

    # Resolve test file path if specified
    test_file_path = None
    if args.test_file:
        test_file_path = os.path.join(PROJECT_ROOT, args.test_file)
        if not os.path.exists(test_file_path):
            print(f"\nError: Test file not found: {test_file_path}")
            sys.exit(1)
        print(f"Using test file: {args.test_file}")

    # If --legacy-mode is specified, only run one test with full legacy mode
    if args.legacy_mode:
        print("\nRunning in LEGACY MODE (all legacy features enabled)")
        results['legacy'] = run_test_game(
            "Full Legacy Mode",
            ["--legacy-mode"],
            team1=args.team1,
            team2=args.team2,
            test_file=test_file_path,
            show_team_output=args.show_team_output,
            use_stdin=args.use_stdin,
            max_turns=args.max_turns
        )
    else:
        # Test 1: Legacy collision handling mode
        results['legacy'] = run_test_game(
            "Legacy Collision Handling",
            ["--legacy-collision-handling"],
            team1=args.team1,
            team2=args.team2,
            test_file=test_file_path,
            show_team_output=args.show_team_output,
            use_stdin=args.use_stdin,
            max_turns=args.max_turns
        )

        time.sleep(1)  # Brief pause between tests

        # Test 2: New collision handling mode (default)
        results['new'] = run_test_game(
            "New Collision Handling",
            [],  # No flags = use new mode
            team1=args.team1,
            team2=args.team2,
            test_file=test_file_path,
            show_team_output=args.show_team_output,
            use_stdin=args.use_stdin,
            max_turns=args.max_turns
        )

    # Summary
    print(f"\n{'='*60}")
    print("Test Summary")
    print(f"{'='*60}")
    print(f"Legacy mode: {'✓ PASSED' if results['legacy'] else '✗ FAILED'}")
    print(f"New mode:    {'✓ PASSED' if results['new'] else '✗ FAILED'}")
    print(f"{'='*60}")

    if all(results.values()):
        print("\n✓ All tests passed!")
        return 0
    else:
        print("\n✗ Some tests failed")
        return 1


if __name__ == '__main__':
    sys.exit(main())