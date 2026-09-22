#include <cstdio>

#include "check.h"

void testTimerSelection();
void testRemainingPercent();
void testArcColorStops();
void testArcColorIsGradual();
void testLowBatteryThreshold();
void testColdBootIsRejected();
void testInitialiseStampsAndClears();
void testInitialiseIsDeterministic();
void testSurvivingBlockIsKept();
void testLayoutChangeInvalidates();
void testPauseRoundTrip();
void testPauseOnlyResumesOnItsOwnFace();
void testNothingWorthResumingIsNotStored();
void testClearPauseWipesTheFace();

int main() {
  testTimerSelection();
  testRemainingPercent();
  testArcColorStops();
  testArcColorIsGradual();
  testLowBatteryThreshold();
  testColdBootIsRejected();
  testInitialiseStampsAndClears();
  testInitialiseIsDeterministic();
  testSurvivingBlockIsKept();
  testLayoutChangeInvalidates();
  testPauseRoundTrip();
  testPauseOnlyResumesOnItsOwnFace();
  testNothingWorthResumingIsNotStored();
  testClearPauseWipesTheFace();

  if (checkFailures() > 0) {
    std::printf("%d check(s) failed\n", checkFailures());
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
