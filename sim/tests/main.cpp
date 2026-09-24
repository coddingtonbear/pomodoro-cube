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
void testAlertPalette();
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
void testUnchangedStateIsNotResent();
void testPacketIdIgnoresWhatTheCallerPutInIt();
void testPacketIdAdvancesOnlyWhenTheReadingDoes();
void testVoltageJitterBelowAMillivoltIsNotAChange();
void testPacketIdWrapsPastAByte();
void testFarewellClearsAwakeAndRunning();
void testFarewellNeedsSomethingToSayFarewellFrom();
void testFarewellIsNumberedLikeAnyOtherChange();

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
  testAlertPalette();
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
  testUnchangedStateIsNotResent();
  testPacketIdIgnoresWhatTheCallerPutInIt();
  testPacketIdAdvancesOnlyWhenTheReadingDoes();
  testVoltageJitterBelowAMillivoltIsNotAChange();
  testPacketIdWrapsPastAByte();
  testFarewellClearsAwakeAndRunning();
  testFarewellNeedsSomethingToSayFarewellFrom();
  testFarewellIsNumberedLikeAnyOtherChange();

  if (checkFailures() > 0) {
    std::printf("%d check(s) failed\n", checkFailures());
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
