#include "gtest/gtest.h"
#include "minmea.h"
namespace {
constexpr double OneCentimeterOnEquatorInDeg = 360.0 / (40.0 * 1000.0 * 100.0);
constexpr double OneMillimiterOnEquatorInDeg = 360.0 / (40.0 * 1000.0 * 1000.0);

TEST(NmeaParsertTest, SanityCheckOnFloatDoublePrecision1) {
  EXPECT_GT(OneCentimeterOnEquatorInDeg, double(FLT_MIN));
  EXPECT_GT(OneCentimeterOnEquatorInDeg, double(FLT_MIN));

}
TEST(NmeaParsertTest, SanityCheckOnFloatDoublePrecision2) {
  const auto OneCentimeterOnEquatorInDegFloat = static_cast<float>(OneCentimeterOnEquatorInDeg);
  EXPECT_NEAR(OneCentimeterOnEquatorInDeg, double(OneCentimeterOnEquatorInDegFloat), OneMillimiterOnEquatorInDeg);
}
TEST(NmeaParserTest, DummyTest) {
  //test checks if minmea library can process
  minmea_sentence_gga gga1;
  gga1.latitude = minmea_float(OneCentimeterOnEquatorInDeg);
  minmea_sentence_gga gga2;
  gga1.latitude = OneCentimeterOnEquatorInDeg;

  EXPECT_EQ(1,2);
}
}