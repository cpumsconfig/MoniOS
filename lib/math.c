#include "math.h"
double sin(double x) {
    // 把 x 归一化到 -π ~ π
    while (x > 3.14159265) x -= 6.28318531;
    while (x < -3.14159265) x += 6.28318531;

    // 泰勒展开近似
    double x2 = x * x;
    return x - (x2 * x)/6.0 + (x2 * x2 * x)/120.0 - (x2 * x2 * x2 * x)/5040.0;
}