#include "core/model.h"
#include "core/parser.h"
#include "core/places.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unity.h>
void setUp() {}
void tearDown() {}
static Snapshot out;
static char error[80];
static bool parse(const std::string &s) {
  size_t i = 0;
  return parseAircraft([&]() { return i < s.size() ? (unsigned char)s[i++] : -1; }, Settings{}, out,
                       error, sizeof(error));
}
void geo() {
  TEST_ASSERT_FLOAT_WITHIN(1, 70, distanceKm(-20.183608, -49.354661, -20.81, -49.38));
  TEST_ASSERT_FLOAT_WITHIN(.01, 90, bearingDeg(0, 0, 0, 1));
  TEST_ASSERT_FLOAT_WITHIN(.1, 22.239, distanceKm(0, 179.9, 0, -179.9));
}
void empty() {
  TEST_ASSERT_TRUE(parse("{\"now\":1,\"ac\":[],\"total\":0}"));
  TEST_ASSERT_EQUAL(0, out.count);
  TEST_ASSERT_TRUE(out.valid);
}
void bad() {
  TEST_ASSERT_FALSE(parse("{\"error\":\"denied\"}"));
  TEST_ASSERT_FALSE(parse("{\"ac\":["));
  TEST_ASSERT_FALSE(parse("{\"ac\":[]"));
  TEST_ASSERT_FALSE(parse("{\"ac\":[{},]}"));
}
void fields() {
  TEST_ASSERT_TRUE(parse(
      R"({"aircraft":[{"hex":"a1","lat":-20.2,"lon":-49.35,"flight":"GLO1   ","alt_baro":"ground","squawk":"7700","category":"A7","dbFlags":1},{"hex":"a2","lon":-49.35},{"hex":"a3","lat":-20.2,"lon":-49.35,"seen_pos":90}]})"));
  TEST_ASSERT_EQUAL(1, out.count);
  TEST_ASSERT_EQUAL_STRING("GLO1", out.planes[0].callsign);
  TEST_ASSERT_TRUE(out.planes[0].ground);
  TEST_ASSERT_TRUE(emergency(out.planes[0]));
  TEST_ASSERT_TRUE(std::isnan(out.planes[0].heading));
  TEST_ASSERT_EQUAL_STRING("A7", out.planes[0].category);
  TEST_ASSERT_TRUE(out.planes[0].military);
}
void nearest() {
  std::string s = "{\"ac\":[";
  for (int i = 0; i < 1200; i++) {
    char b[200];
    snprintf(b, sizeof(b),
             "%s{\"hex\":\"%06x\",\"lat\":%.6f,\"lon\":-49.35,\"ignored\":{\"nested\":[1,2,3]}}",
             i ? "," : "", i, -21.1 + i * .0007);
    s += b;
  }
  s += "]}";
  TEST_ASSERT_TRUE_MESSAGE(parse(s), error);
  TEST_ASSERT_EQUAL(MAX_AIRCRAFT, out.count);
  TEST_ASSERT_LESS_THAN(20, out.planes[MAX_AIRCRAFT - 1].distance);
}
void config() {
  Settings s;
  TEST_ASSERT_TRUE(validSettings(s));
  s.lat = NAN;
  TEST_ASSERT_FALSE(validSettings(s));
  s = {};
  s.rangeKm = 0;
  TEST_ASSERT_FALSE(validSettings(s));
}
void flightIdentifiers() {
  char value[20];
  TEST_ASSERT_TRUE(normalizeFlight(" la 3030 ", value, sizeof(value)));
  TEST_ASSERT_EQUAL_STRING("LA3030", value);
  TEST_ASSERT_TRUE(normalizeFlight("G31600", value, sizeof(value)));
  TEST_ASSERT_TRUE(normalizeFlight("TAM3030", value, sizeof(value)));
  TEST_ASSERT_TRUE(normalizeFlight("", value, sizeof(value)));
  TEST_ASSERT_EQUAL_STRING("", value);
  TEST_ASSERT_FALSE(normalizeFlight("3030", value, sizeof(value)));
  TEST_ASSERT_FALSE(normalizeFlight("TAM", value, sizeof(value)));
  TEST_ASSERT_FALSE(normalizeFlight("LA1/../2", value, sizeof(value)));
  TEST_ASSERT_FALSE(normalizeFlight("LA123456789", value, sizeof(value)));
}
void globalFlightPosition() {
  const std::string body =
      R"({"ac":[{"hex":"a12345","flight":"TEST123","lat":51.5,"lon":-0.1,"seen_pos":2},{"hex":"b12345","lat":51.4,"lon":-0.1,"seen_pos":80}]})";
  TEST_ASSERT_TRUE(parse(body));
  TEST_ASSERT_EQUAL(0, out.count); // Outside the saved Brazilian scope.
  Settings lookup;
  lookup.rangeKm = 21000;
  size_t i = 0;
  TEST_ASSERT_TRUE(parseAircraft([&]() { return i < body.size() ? (unsigned char)body[i++] : -1; },
                                 lookup, out, error, sizeof(error)));
  TEST_ASSERT_EQUAL(1, out.count); // Global following still rejects stale positions.
  TEST_ASSERT_EQUAL_STRING("a12345", out.planes[0].hex);
  TEST_ASSERT_FLOAT_WITHIN(.001, 51.5, out.planes[0].lat);
}
void landmarks() {
  VisiblePlace places[56];
  size_t count = nearbyPlaces(-20.81, -49.38, 40, places, 56);
  TEST_ASSERT_GREATER_THAN(0, count);
  bool airport = false, airportCity = false;
  for (size_t i = 0; i < count; ++i) {
    airport |= !strcmp(places[i].code, "SBSR");
    airportCity |= !strcmp(places[i].code, "SBSR") && strstr(places[i].name, "Rio Preto");
  }
  TEST_ASSERT_TRUE(airport);
  TEST_ASSERT_TRUE(airportCity);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(geo);
  RUN_TEST(empty);
  RUN_TEST(bad);
  RUN_TEST(fields);
  RUN_TEST(nearest);
  RUN_TEST(config);
  RUN_TEST(flightIdentifiers);
  RUN_TEST(globalFlightPosition);
  RUN_TEST(landmarks);
  return UNITY_END();
}
