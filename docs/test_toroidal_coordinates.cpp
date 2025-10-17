/* test_toroidal_coordinates.cpp
 *
 * Test program to verify that CCoord::DistTo() and CCoord::AngleTo()
 * correctly handle toroidal topology (world wrapping).
 *
 * World dimensions: [-512, 512) x [-512, 512) (1024 x 1024 units)
 * Edges wrap: leaving one edge brings you to the opposite edge
 */

#include <cmath>
#include <cstdio>
#include <vector>
#include <string>

// Include the coordinate classes
#include "../team/src/Coord.h"
#include "../team/src/Traj.h"

// Helper constants (PI is defined in stdafx.h)
const double EPSILON = 1e-6;

// Test result structure
struct TestResult {
    std::string description;
    CCoord from;
    CCoord to;
    double expected_distance;
    double actual_distance;
    double expected_angle;  // in radians
    double actual_angle;    // in radians
    bool distance_passed;
    bool angle_passed;
};

// Helper function to compare doubles with epsilon
bool approx_equal(double a, double b, double epsilon = EPSILON) {
    return fabs(a - b) < epsilon;
}

// Helper function to normalize angle to [-PI, PI]
double normalize_angle(double angle) {
    while (angle > PI) angle -= 2.0 * PI;
    while (angle < -PI) angle += 2.0 * PI;
    return angle;
}

// Helper to convert radians to degrees for display
double rad_to_deg(double rad) {
    return rad * 180.0 / PI;
}

// Run a single test
TestResult run_test(const std::string& description,
                    const CCoord& from,
                    const CCoord& to,
                    double expected_dist,
                    double expected_angle) {
    TestResult result;
    result.description = description;
    result.from = from;
    result.to = to;
    result.expected_distance = expected_dist;
    result.expected_angle = normalize_angle(expected_angle);

    result.actual_distance = from.DistTo(to);
    result.actual_angle = normalize_angle(from.AngleTo(to));

    result.distance_passed = approx_equal(result.actual_distance, result.expected_distance);
    result.angle_passed = approx_equal(result.actual_angle, result.expected_angle);

    return result;
}

// Print test result
void print_test_result(const TestResult& result, int test_num) {
    printf("\n=== Test %d: %s ===\n", test_num, result.description.c_str());
    printf("  From:     (%.2f, %.2f)\n", result.from.fX, result.from.fY);
    printf("  To:       (%.2f, %.2f)\n", result.to.fX, result.to.fY);
    printf("  Distance: Expected=%.4f, Actual=%.4f [%s]\n",
           result.expected_distance, result.actual_distance,
           result.distance_passed ? "PASS" : "FAIL");
    printf("  Angle:    Expected=%.4f rad (%.2f°), Actual=%.4f rad (%.2f°) [%s]\n",
           result.expected_angle, rad_to_deg(result.expected_angle),
           result.actual_angle, rad_to_deg(result.actual_angle),
           result.angle_passed ? "PASS" : "FAIL");
}

int main() {
    printf("========================================\n");
    printf("Toroidal Coordinate System Test Program\n");
    printf("========================================\n");
    printf("World bounds: [%.0f, %.0f) x [%.0f, %.0f)\n",
           fWXMin, fWXMax, fWYMin, fWYMax);
    printf("World size: %.0f x %.0f units\n\n", kWorldSizeX, kWorldSizeY);

    std::vector<TestResult> results;
    int test_num = 1;

    // ========================================
    // Section A: Non-wrapping cases (simple Euclidean)
    // ========================================
    printf("\n========================================\n");
    printf("Section A: Non-Wrapping Cases\n");
    printf("========================================\n");

    // Test A1: Origin to nearby point
    results.push_back(run_test(
        "Origin to (1,1) - no wrapping needed",
        CCoord(0, 0), CCoord(1, 1),
        sqrt(2.0),  // distance = sqrt(1^2 + 1^2)
        atan2(1, 1) // angle = 45° = PI/4
    ));
    print_test_result(results.back(), test_num++);

    // Test A2: Larger distance, still no wrapping
    results.push_back(run_test(
        "From (100,100) to (200,150) - no wrapping",
        CCoord(100, 100), CCoord(200, 150),
        sqrt(100*100 + 50*50),  // sqrt(10000 + 2500)
        atan2(50, 100)
    ));
    print_test_result(results.back(), test_num++);

    // Test A3: Negative coordinates, no wrapping
    results.push_back(run_test(
        "From (-100,-100) to (-50,-50) - no wrapping",
        CCoord(-100, -100), CCoord(-50, -50),
        sqrt(50*50 + 50*50),
        atan2(50, 50)  // 45° northeast
    ));
    print_test_result(results.back(), test_num++);

    // ========================================
    // Section B: Single-edge wrapping
    // ========================================
    printf("\n========================================\n");
    printf("Section B: Single-Edge Wrapping\n");
    printf("========================================\n");

    // Test B1: Right edge wrapping (positive X wraps around)
    // From (500, 0) to (-500, 0)
    // Direct distance would be 1000 left, but wrapping right gives 24 units
    // The vector goes: 500 -> 512 (12 units right) then wraps to -512 -> -500 (12 units right)
    results.push_back(run_test(
        "Right edge wrap: (500,0) to (-500,0)",
        CCoord(500, 0), CCoord(-500, 0),
        24.0,  // wraps: 500 to 512 = 12, -512 to -500 = 12, total = 24
        0.0    // pointing right (east) = 0° (shortest path goes right and wraps)
    ));
    print_test_result(results.back(), test_num++);

    // Test B2: Left edge wrapping (negative X wraps around)
    // From (-500, 0) to (500, 0)
    // Direct distance would be 1000 right, but wrapping left gives 24 units
    // The vector goes: -500 -> -512 (12 units left) then wraps to 512 -> 500 (12 units left)
    results.push_back(run_test(
        "Left edge wrap: (-500,0) to (500,0)",
        CCoord(-500, 0), CCoord(500, 0),
        24.0,
        PI     // pointing left (west) = 180° (shortest path goes left and wraps)
    ));
    print_test_result(results.back(), test_num++);

    // Test B3: Top edge wrapping (positive Y wraps around)
    // From (0, 500) to (0, -500)
    results.push_back(run_test(
        "Top edge wrap: (0,500) to (0,-500)",
        CCoord(0, 500), CCoord(0, -500),
        24.0,
        PI / 2.0  // pointing down (south) = 90°
    ));
    print_test_result(results.back(), test_num++);

    // Test B4: Bottom edge wrapping (negative Y wraps around)
    // From (0, -500) to (0, 500)
    results.push_back(run_test(
        "Bottom edge wrap: (0,-500) to (0,500)",
        CCoord(0, -500), CCoord(0, 500),
        24.0,
        -PI / 2.0  // pointing up (north) = -90°
    ));
    print_test_result(results.back(), test_num++);

    // ========================================
    // Section C: Corner wrapping (two adjacent edges)
    // ========================================
    printf("\n========================================\n");
    printf("Section C: Corner Wrapping (Two Adjacent Edges)\n");
    printf("========================================\n");

    // Test C1: Top-right corner
    // From (500, 500) to (-500, -500)
    // Wraps through both top and right edges
    results.push_back(run_test(
        "Top-right corner: (500,500) to (-500,-500)",
        CCoord(500, 500), CCoord(-500, -500),
        sqrt(24.0 * 24.0 + 24.0 * 24.0),  // sqrt(576 + 576) = 33.94
        atan2(24.0, 24.0)  // 45° toward bottom-right in wrapped space
    ));
    print_test_result(results.back(), test_num++);

    // Test C2: Top-left corner
    // From (-500, 500) to (500, -500)
    results.push_back(run_test(
        "Top-left corner: (-500,500) to (500,-500)",
        CCoord(-500, 500), CCoord(500, -500),
        sqrt(24.0 * 24.0 + 24.0 * 24.0),
        atan2(24.0, -24.0)  // -45° toward bottom-left in wrapped space
    ));
    print_test_result(results.back(), test_num++);

    // Test C3: Bottom-right corner
    // From (500, -500) to (-500, 500)
    results.push_back(run_test(
        "Bottom-right corner: (500,-500) to (-500,500)",
        CCoord(500, -500), CCoord(-500, 500),
        sqrt(24.0 * 24.0 + 24.0 * 24.0),
        atan2(-24.0, 24.0)  // -45° toward top-right in wrapped space
    ));
    print_test_result(results.back(), test_num++);

    // Test C4: Bottom-left corner
    // From (-500, -500) to (500, 500)
    results.push_back(run_test(
        "Bottom-left corner: (-500,-500) to (500,500)",
        CCoord(-500, -500), CCoord(500, 500),
        sqrt(24.0 * 24.0 + 24.0 * 24.0),
        atan2(-24.0, -24.0)  // 135° toward top-left in wrapped space
    ));
    print_test_result(results.back(), test_num++);

    // ========================================
    // Section D: Three-edge wrapping analysis
    // ========================================
    printf("\n========================================\n");
    printf("Section D: Three-Edge Wrapping Analysis\n");
    printf("========================================\n");

    printf("\nQuestion: Can a shortest path pass through 3 edges in a 2D toroidal topology?\n\n");
    printf("Answer: NO - This is geometrically impossible.\n\n");
    printf("Explanation:\n");
    printf("  In a 2D torus, coordinates have exactly 2 dimensions (X and Y).\n");
    printf("  Each dimension can wrap independently through at most ONE boundary:\n");
    printf("    - X can wrap through left OR right edge (not both)\n");
    printf("    - Y can wrap through top OR bottom edge (not both)\n\n");
    printf("  Maximum edges crossed: 2 (one per dimension)\n\n");
    printf("  Example showing this limitation:\n");
    printf("    From: (400, 300)  To: (-400, -300)\n");
    printf("      X-direction: 400 to -400\n");
    printf("        Option A: Go left 800 units (no wrap)\n");
    printf("        Option B: Go right 224 units (wrap through right edge)\n");
    printf("        Shortest: Option B (224 units, 1 edge)\n");
    printf("      Y-direction: 300 to -300\n");
    printf("        Option A: Go down 600 units (no wrap)\n");
    printf("        Option B: Go up 424 units (wrap through bottom edge)\n");
    printf("        Shortest: Option A (600 units, 0 edges)\n");
    printf("      Total edges crossed: 1 (only the right edge)\n\n");

    // Demonstrate with an actual test
    // From (400, 300) to (-400, -300)
    // Raw difference: (-800, -600)
    // After Normalize():
    //   X: -800 mod 1024 = 224 (wraps right)
    //   Y: -600 mod 1024 = 424 (wraps down)
    // Both dimensions wrap! Shortest path is (+224, +424)
    results.push_back(run_test(
        "Demonstration: (400,300) to (-400,-300) - wraps both X and Y",
        CCoord(400, 300), CCoord(-400, -300),
        sqrt(224.0 * 224.0 + 424.0 * 424.0),  // both dimensions wrap
        atan2(424.0, 224.0)  // angle toward wrapped direction (northeast in wrapped space)
    ));
    print_test_result(results.back(), test_num++);

    printf("\n  Geometric constraint:\n");
    printf("    A path in 2D space can only traverse through edges perpendicular\n");
    printf("    to the axis of movement. Since we have 2 axes (X,Y), we can cross\n");
    printf("    at most 2 edges (one per axis). Three edges would require a third\n");
    printf("    dimension, which doesn't exist in 2D toroidal topology.\n");

    // ========================================
    // Section E: Additional edge cases
    // ========================================
    printf("\n========================================\n");
    printf("Section E: Additional Edge Cases\n");
    printf("========================================\n");

    // Test E1: Exact boundary points
    results.push_back(run_test(
        "Boundary to boundary: (-512,0) to (511,0)",
        CCoord(-512, 0), CCoord(511, 0),
        1.0,  // wraps: -512 to 511 is just 1 unit via wrapping
        PI    // pointing left (wraps around)
    ));
    print_test_result(results.back(), test_num++);

    // Test E2: Same point (distance should be 0)
    results.push_back(run_test(
        "Same point: (100,100) to (100,100)",
        CCoord(100, 100), CCoord(100, 100),
        0.0,
        0.0   // angle is 0 when distance is 0
    ));
    print_test_result(results.back(), test_num++);

    // ========================================
    // Summary
    // ========================================
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < results.size(); i++) {
        bool test_passed = results[i].distance_passed && results[i].angle_passed;
        if (test_passed) {
            passed++;
        } else {
            failed++;
            printf("FAILED: Test %zu - %s\n", i+1, results[i].description.c_str());
            if (!results[i].distance_passed) {
                printf("  Distance mismatch: expected %.4f, got %.4f\n",
                       results[i].expected_distance, results[i].actual_distance);
            }
            if (!results[i].angle_passed) {
                printf("  Angle mismatch: expected %.4f rad, got %.4f rad\n",
                       results[i].expected_angle, results[i].actual_angle);
            }
        }
    }

    printf("\nTotal tests: %zu\n", results.size());
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);

    if (failed == 0) {
        printf("\n✓ All tests passed! Toroidal coordinate system is working correctly.\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}
