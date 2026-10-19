#include <unity.h>
#include "protocol.h"

static CanFrame valid() {
  CanFrame f = {};
  f.mesh_id = 42;
  f.can_id = 0x3E9;
  f.len = 8;
  return f;
}

static bool decode(const CanFrame &in, uint32_t mesh_id, int len = sizeof(CanFrame)) {
  CanFrame out;
  return decode_frame((const uint8_t *)&in, len, mesh_id, out);
}

void test_accepts_valid_frame() {
  CanFrame out;
  CanFrame in = valid();
  TEST_ASSERT_TRUE(decode_frame((const uint8_t *)&in, sizeof(in), 0, out));
  TEST_ASSERT_EQUAL_UINT32(0x3E9, out.can_id);
}

void test_rejects_wrong_length() {
  TEST_ASSERT_FALSE(decode(valid(), 0, sizeof(CanFrame) - 1));
  TEST_ASSERT_FALSE(decode(valid(), 0, sizeof(CanFrame) + 1));
}

void test_rejects_len_over_8() {
  CanFrame f = valid();
  f.len = 9;
  TEST_ASSERT_FALSE(decode(f, 0));
}

void test_rejects_wrong_magic_or_version() {
  CanFrame f = valid();
  f.magic = 0;
  TEST_ASSERT_FALSE(decode(f, 0));
  f = valid();
  f.version = FRAME_VERSION + 1;
  TEST_ASSERT_FALSE(decode(f, 0));
}

void test_filters_by_mesh_id() {
  TEST_ASSERT_TRUE(decode(valid(), 42));
  TEST_ASSERT_FALSE(decode(valid(), 7));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_valid_frame);
  RUN_TEST(test_rejects_wrong_length);
  RUN_TEST(test_rejects_len_over_8);
  RUN_TEST(test_rejects_wrong_magic_or_version);
  RUN_TEST(test_filters_by_mesh_id);
  return UNITY_END();
}
