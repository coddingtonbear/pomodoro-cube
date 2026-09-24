#include <cstdio>

#include "check.h"

void testTimerSelection();
void testRemainingPercent();
void testLapPercent();
void testClockFieldsSwitchToHoursPastAnHour();
void testFlowArcColorRunsBackwards();
void testArcColorStops();
void testArcColorIsGradual();
void testLowBatteryThreshold();
void testColdBootIsRejected();
void testInitialiseStampsAndClears();
void testInitialiseIsDeterministic();
void testSurvivingBlockIsKept();
void testLayoutChangeInvalidates();
void testPauseRoundTrip();
void testAPausedFlowStintResumesCountingUp();
void testFlowEarnedRoundTrip();
void testFlowEarnedIsSeparateFromThePause();
void testPauseOnlyResumesOnItsOwnFace();
void testNothingWorthResumingIsNotStored();
void testClearPauseWipesTheFace();
void testEncodesKnownStateExactly();
void testFitsInALegacyAdvertisement();
void testObjectIdsAscend();
void testFarewellAdvert();
void testOutOfRangeValuesClamp();
void testRefusesABufferItCannotFill();
void testFlowAdvertisesWorkWithNoSelectedLength();
void testBreakAdvertisesNotWork();

int main() {
  testTimerSelection();
  testRemainingPercent();
  testLapPercent();
  testClockFieldsSwitchToHoursPastAnHour();
  testFlowArcColorRunsBackwards();
  testArcColorStops();
  testArcColorIsGradual();
  testLowBatteryThreshold();
  testColdBootIsRejected();
  testInitialiseStampsAndClears();
  testInitialiseIsDeterministic();
  testSurvivingBlockIsKept();
  testLayoutChangeInvalidates();
  testPauseRoundTrip();
  testAPausedFlowStintResumesCountingUp();
  testFlowEarnedRoundTrip();
  testFlowEarnedIsSeparateFromThePause();
  testPauseOnlyResumesOnItsOwnFace();
  testNothingWorthResumingIsNotStored();
  testClearPauseWipesTheFace();
  testEncodesKnownStateExactly();
  testFitsInALegacyAdvertisement();
  testObjectIdsAscend();
  testFarewellAdvert();
  testOutOfRangeValuesClamp();
  testRefusesABufferItCannotFill();
  testFlowAdvertisesWorkWithNoSelectedLength();
  testBreakAdvertisesNotWork();

  if (checkFailures() > 0) {
    std::printf("%d check(s) failed\n", checkFailures());
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
