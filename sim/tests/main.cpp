#include <cstdio>

#include "check.h"

void testTimerSelection();
void testRemainingPercent();
void testArcColorStops();
void testArcColorIsGradual();
void testLowBatteryThreshold();

int main() {
  testTimerSelection();
  testRemainingPercent();
  testArcColorStops();
  testArcColorIsGradual();
  testLowBatteryThreshold();

  if (checkFailures() > 0) {
    std::printf("%d check(s) failed\n", checkFailures());
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
