#include <cstdio>

#include "check.h"

void testTimerSelection();
void testRestingFaceParking();
void testBacklightPolicy();
void testRemainingPercent();
void testLapPercent();
void testClockFieldsSwitchToHoursPastAnHour();
void testFlowArcColorMatchesTheCountdownRamp();
void testArcColorStops();
void testPalette();
void testArcColorIsGradual();
void testLowBatteryThreshold();
void testColdBootIsRejected();
void testInitialiseStampsAndClears();
void testInitialiseIsDeterministic();
void testSurvivingBlockIsKept();
void testLayoutChangeInvalidates();
void testPauseRoundTrip();
void testAPausedFlowStintResumesCountingUp();
void testFlowBankAccumulates();
void testFlowBankIsWrittenBackAsABreakIsSpent();
void testFlowBankIsClamped();
void testFlowBankIsSeparateFromThePause();
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
  testRestingFaceParking();
  testBacklightPolicy();
  testRemainingPercent();
  testLapPercent();
  testClockFieldsSwitchToHoursPastAnHour();
  testFlowArcColorMatchesTheCountdownRamp();
  testArcColorStops();
  testPalette();
  testArcColorIsGradual();
  testLowBatteryThreshold();
  testColdBootIsRejected();
  testInitialiseStampsAndClears();
  testInitialiseIsDeterministic();
  testSurvivingBlockIsKept();
  testLayoutChangeInvalidates();
  testPauseRoundTrip();
  testAPausedFlowStintResumesCountingUp();
  testFlowBankAccumulates();
  testFlowBankIsWrittenBackAsABreakIsSpent();
  testFlowBankIsClamped();
  testFlowBankIsSeparateFromThePause();
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
