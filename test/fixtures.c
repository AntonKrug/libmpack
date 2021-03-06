#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fixtures.h"
#include "../src/rpc.h"

static void write_size_be(uint8_t *p, size_t s)
{
  if (s <= UINT8_MAX) {
    p[0] = (uint8_t)s;
  } else if (s <= UINT16_MAX) {
    p[0] = (uint8_t)((s & 0xff00) >> 8);
    p[1] = (uint8_t)(s & 0xff);
  } else {
    p[0] = (uint8_t)((s & 0xff000000) >> 24);
    p[1] = (uint8_t)((s & 0xff0000) >> 16);
    p[2] = (uint8_t)((s & 0xff00) >> 8);
    p[3] = (uint8_t)(s & 0xff);
  }
}

static char jsbuf[0xffffff];
static uint8_t mpbuf[0xffffff];

static void map_generator(char **js, uint8_t **mp, size_t *mplen, size_t s)
{
  assert(s >= 0x10);
  *js = jsbuf;
  *mp = mpbuf;
  (*js)[0] = '{';
  size_t jsoff = 1;
  size_t mpoff = 1;
  const char js_item_pattern[] = "\"k%05x\":[1,\"s:2\",[3,4]]";
  uint8_t mp_item_pattern[] = {
    0xa6, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x93, 0x01, 0xa1, 0x32, 0x92, 0x03, 0x04,
  };

  if (s <= UINT16_MAX) {
    (*mp)[0] = 0xde;
    if (s <= 0xff) {
      /* put an extra byte to the left */
      write_size_be(*mp + mpoff++, 0);
      write_size_be(*mp + mpoff++, s);
    } else {
      write_size_be(*mp + mpoff, s);
      mpoff += 2;
    }
  } else {
    (*mp)[0] = 0xdf;
    write_size_be(*mp + mpoff, s);
    mpoff += 4;
  }

  for (size_t i = 0; i < s; i++) {
    char b[sizeof(js_item_pattern) + 1];
    snprintf(b, sizeof(b), js_item_pattern, (unsigned int)i);
    memcpy(*js + jsoff, b, sizeof(b) - 1);
    jsoff += sizeof(b) - 1;
    (*js)[jsoff++] = ',';
    memcpy(*mp + mpoff, mp_item_pattern, sizeof(mp_item_pattern));
    memcpy(*mp + mpoff + 2, b + 2, 5);
    mpoff += sizeof(mp_item_pattern);
  }
  (*js)[--jsoff] = '}';
  (*js)[++jsoff] = 0;
  *mplen = mpoff;
}

static void array_generator(char **js, uint8_t **mp, size_t *mplen, size_t s)
{
  assert(s >= 0x10);
  *js = jsbuf;
  *mp = mpbuf;
  (*js)[0] = '[';
  size_t jsoff = 1;
  size_t mpoff = 1;
  char js_item_pattern[] = "{\"mpack\":true,\"version\":[1,0,0]}";
  uint8_t mp_item_pattern[] = {
    0x82, 0xa5, 0x6d, 0x70, 0x61, 0x63, 0x6b, 0xc3, 0xa7, 0x76,
    0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x93, 0x01, 0x00, 0x00};


  if (s <= UINT16_MAX) {
    (*mp)[0] = 0xdc;
    if (s <= 0xff) {
      /* put an extra byte to the left */
      write_size_be(*mp + mpoff++, 0);
      write_size_be(*mp + mpoff++, s);
    } else {
      write_size_be(*mp + mpoff, s);
      mpoff += 2;
    }
  } else {
    (*mp)[0] = 0xdd;
    write_size_be(*mp + mpoff, s);
    mpoff += 4;
  }

  for (size_t i = 0; i < s; i++) {
    memcpy(*mp + mpoff, mp_item_pattern, sizeof(mp_item_pattern));
    mpoff += sizeof(mp_item_pattern);
    memcpy(*js + jsoff, js_item_pattern, sizeof(js_item_pattern) - 1);
    jsoff += sizeof(js_item_pattern) - 1;
    (*js)[jsoff++] = ',';
  }
  (*js)[--jsoff] = ']';
  (*js)[++jsoff] = 0;
  *mplen = mpoff;
}

static void blob_pattern_generator(char **js, uint8_t **mp, size_t *mplen,
    size_t s, uint8_t code, char prefix)
{
  *js = jsbuf;
  *mp = mpbuf;
  char *pattern;
  size_t mpoff;
  size_t jsoff = prefix == 'e' ? 6 : 3;
  assert(s + jsoff + 2 <= sizeof(jsbuf));
  (*js)[0] = '"';
  (*js)[1] = prefix;
  (*js)[2] = ':';
  if (prefix == 'e') {
    (*js)[3] = '7';
    (*js)[4] = 'f';
    (*js)[5] = ':';
  }

  if (s <= UINT8_MAX) {
    pattern = "blob08";
    mpoff = 2;
  } else if (s <= UINT16_MAX) {
    pattern = "blob16";
    mpoff = 3;
    code++;
  } else {
    pattern = "blob32";
    mpoff = 5;
    code++; code++;
  }

  if (prefix == 'e') {
    mpoff++;
  }

  assert(s + mpoff <= sizeof(mpbuf));
  (*mp)[0] = code;
  write_size_be(*mp + 1, s);

  if (prefix == 'e') {
    (*mp)[mpoff - 1] = 0x7f;
  }

  size_t patlen = strlen(pattern);
  for (size_t i = 0; i < s; i++) {
    (*mp)[i + mpoff] = (uint8_t )pattern[i % patlen];
    (*js)[i + jsoff] = pattern[i % patlen];
  }
  (*js)[s + jsoff] = '"';
  (*js)[s + jsoff + 1] = 0;
  *mplen = s + mpoff;
}

static void bin_generator(char **js, uint8_t **mp, size_t *mplen, size_t s)
{
  blob_pattern_generator(js, mp, mplen, s, 0xc4, 'b');
}

static void str_generator(char **js, uint8_t **mp, size_t *mplen, size_t s)
{
  assert(s > 31);
  blob_pattern_generator(js, mp, mplen, s, 0xd9, 's');
}

static void ext_generator(char **js, uint8_t **mp, size_t *mplen, size_t s)
{
  assert(s != 1 && s != 2 && s != 4 && s != 8 && s != 16);
  blob_pattern_generator(js, mp, mplen, s, 0xc7, 'e');
}

const struct fixture fixtures[] = {

#define F(j, ...) {                                          \
  .json = j,                                                 \
  .msgpack = (uint8_t []){__VA_ARGS__},                      \
  .msgpacklen = (sizeof((int[]){__VA_ARGS__})/sizeof(int)),  \
  .generator = NULL,                                         \
  .generator_size = 0                                        \
}

#define DF(g, s) {                                           \
  .json = NULL,                                              \
  .msgpack = NULL,                                           \
  .msgpacklen = 0,                                           \
  .generator = g,                                            \
  .generator_size = s                                        \
}

  /* positive fixint */
  F("0", 0x00),
  F("1", 0x01),
  F("2", 0x02),
  F("3", 0x03),
  F("4", 0x04),
  F("5", 0x05),
  F("6", 0x06),
  F("7", 0x07),
  F("8", 0x08),
  F("9", 0x09),
  F("10", 0x0a),
  F("11", 0x0b),
  F("12", 0x0c),
  F("13", 0x0d),
  F("14", 0x0e),
  F("15", 0x0f),
  F("16", 0x10),
  F("17", 0x11),
  F("18", 0x12),
  F("19", 0x13),
  F("20", 0x14),
  F("21", 0x15),
  F("22", 0x16),
  F("23", 0x17),
  F("24", 0x18),
  F("25", 0x19),
  F("26", 0x1a),
  F("27", 0x1b),
  F("28", 0x1c),
  F("29", 0x1d),
  F("30", 0x1e),
  F("31", 0x1f),
  F("32", 0x20),
  F("33", 0x21),
  F("34", 0x22),
  F("35", 0x23),
  F("36", 0x24),
  F("37", 0x25),
  F("38", 0x26),
  F("39", 0x27),
  F("40", 0x28),
  F("41", 0x29),
  F("42", 0x2a),
  F("43", 0x2b),
  F("44", 0x2c),
  F("45", 0x2d),
  F("46", 0x2e),
  F("47", 0x2f),
  F("48", 0x30),
  F("49", 0x31),
  F("50", 0x32),
  F("51", 0x33),
  F("52", 0x34),
  F("53", 0x35),
  F("54", 0x36),
  F("55", 0x37),
  F("56", 0x38),
  F("57", 0x39),
  F("58", 0x3a),
  F("59", 0x3b),
  F("60", 0x3c),
  F("61", 0x3d),
  F("62", 0x3e),
  F("63", 0x3f),
  F("64", 0x40),
  F("65", 0x41),
  F("66", 0x42),
  F("67", 0x43),
  F("68", 0x44),
  F("69", 0x45),
  F("70", 0x46),
  F("71", 0x47),
  F("72", 0x48),
  F("73", 0x49),
  F("74", 0x4a),
  F("75", 0x4b),
  F("76", 0x4c),
  F("77", 0x4d),
  F("78", 0x4e),
  F("79", 0x4f),
  F("80", 0x50),
  F("81", 0x51),
  F("82", 0x52),
  F("83", 0x53),
  F("84", 0x54),
  F("85", 0x55),
  F("86", 0x56),
  F("87", 0x57),
  F("88", 0x58),
  F("89", 0x59),
  F("90", 0x5a),
  F("91", 0x5b),
  F("92", 0x5c),
  F("93", 0x5d),
  F("94", 0x5e),
  F("95", 0x5f),
  F("96", 0x60),
  F("97", 0x61),
  F("98", 0x62),
  F("99", 0x63),
  F("100", 0x64),
  F("101", 0x65),
  F("102", 0x66),
  F("103", 0x67),
  F("104", 0x68),
  F("105", 0x69),
  F("106", 0x6a),
  F("107", 0x6b),
  F("108", 0x6c),
  F("109", 0x6d),
  F("110", 0x6e),
  F("111", 0x6f),
  F("112", 0x70),
  F("113", 0x71),
  F("114", 0x72),
  F("115", 0x73),
  F("116", 0x74),
  F("117", 0x75),
  F("118", 0x76),
  F("119", 0x77),
  F("120", 0x78),
  F("121", 0x79),
  F("122", 0x7a),
  F("123", 0x7b),
  F("124", 0x7c),
  F("125", 0x7d),
  F("126", 0x7e),
  F("127", 0x7f),
  /* fixmap */
  F("{}",
      0x80),
  F("{\"k\":\"s:val\"}",
      0x81, 0xa1, 0x6b, 0xa3, 0x76, 0x61, 0x6c),
  F("{\"k1\":1,\"k2\":2}",
      0x82, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02),
  F("{\"k1\":1,\"k2\":2,\"k3\":3}",
      0x83, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4}",
      0x84, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5}",
      0x85, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6}",
      0x86, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7}",
      0x87, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8}",
      0x88, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9}",
      0x89, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9,\"k10\":10}",
      0x8a, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09, 0xa3, 0x6b, 0x31, 0x30, 0x0a),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9,\"k10\":10,\"k11\":11}",
      0x8b, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09, 0xa3, 0x6b, 0x31, 0x30, 0x0a, 0xa3, 0x6b, 0x31, 0x31, 0x0b),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9,\"k10\":10,\"k11\":11,\"k12\":12}",
      0x8c, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09, 0xa3, 0x6b, 0x31, 0x30, 0x0a, 0xa3, 0x6b, 0x31, 0x31, 0x0b, 0xa3,
      0x6b, 0x31, 0x32, 0x0c),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9,\"k10\":10,\"k11\":11,\"k12\":12,\"k13\":13}",
      0x8d, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09, 0xa3, 0x6b, 0x31, 0x30, 0x0a, 0xa3, 0x6b, 0x31, 0x31, 0x0b, 0xa3,
      0x6b, 0x31, 0x32, 0x0c, 0xa3, 0x6b, 0x31, 0x33, 0x0d),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9,\"k10\":10,\"k11\":11,\"k12\":12,\"k13\":13,"
     "\"k14\":14}",
      0x8e, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09, 0xa3, 0x6b, 0x31, 0x30, 0x0a, 0xa3, 0x6b, 0x31, 0x31, 0x0b, 0xa3,
      0x6b, 0x31, 0x32, 0x0c, 0xa3, 0x6b, 0x31, 0x33, 0x0d, 0xa3, 0x6b, 0x31,
      0x34, 0x0e),
  F("{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,"
     "\"k8\":8,\"k9\":9,\"k10\":10,\"k11\":11,\"k12\":12,\"k13\":13,"
     "\"k14\":14,\"k15\":15}",
      0x8f, 0xa2, 0x6b, 0x31, 0x01, 0xa2, 0x6b, 0x32, 0x02, 0xa2, 0x6b, 0x33,
      0x03, 0xa2, 0x6b, 0x34, 0x04, 0xa2, 0x6b, 0x35, 0x05, 0xa2, 0x6b, 0x36,
      0x06, 0xa2, 0x6b, 0x37, 0x07, 0xa2, 0x6b, 0x38, 0x08, 0xa2, 0x6b, 0x39,
      0x09, 0xa3, 0x6b, 0x31, 0x30, 0x0a, 0xa3, 0x6b, 0x31, 0x31, 0x0b, 0xa3,
      0x6b, 0x31, 0x32, 0x0c, 0xa3, 0x6b, 0x31, 0x33, 0x0d, 0xa3, 0x6b, 0x31,
      0x34, 0x0e, 0xa3, 0x6b, 0x31, 0x35, 0x0f),
  F("{\"k1\":[1,\"s:2\",[3,4]]}",
    0x81, 0xa2, 0x6b, 0x31, 0x93, 0x01, 0xa1, 0x32, 0x92, 0x03, 0x04),
  F("{\"k1\":[1,\"s:2\",[3,4]],\"k2\":[1,\"s:2\",[3,4]]}",
    0x82, 0xa2, 0x6b, 0x31, 0x93, 0x01, 0xa1, 0x32, 0x92, 0x03, 0x04,
          0xa2, 0x6b, 0x32, 0x93, 0x01, 0xa1, 0x32, 0x92, 0x03, 0x04),
  /* fixarray */
  F("[]",
      0x90),
  F("[1]",
      0x91, 0x01),
  F("[1,2]",
      0x92, 0x01, 0x02),
  F("[1,2,3]",
      0x93, 0x01, 0x02, 0x03),
  F("[1,2,3,4]",
      0x94, 0x01, 0x02, 0x03, 0x04),
  F("[1,2,3,4,5]",
      0x95, 0x01, 0x02, 0x03, 0x04, 0x05),
  F("[1,2,3,4,5,6]",
      0x96, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06),
  F("[1,2,3,4,5,6,7]",
      0x97, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07),
  F("[1,2,3,4,5,6,7,8]",
      0x98, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08),
  F("[1,2,3,4,5,6,7,8,9]",
      0x99, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09),
  F("[1,2,3,4,5,6,7,8,9,10]",
      0x9a, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a),
  F("[1,2,3,4,5,6,7,8,9,10,11]",
      0x9b, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b),
  F("[1,2,3,4,5,6,7,8,9,10,11,12]",
      0x9c, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c),
  F("[1,2,3,4,5,6,7,8,9,10,11,12,13]",
      0x9d, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d),
  F("[1,2,3,4,5,6,7,8,9,10,11,12,13,14]",
      0x9e, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e),
  F("[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]",
      0x9f, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f),
  F("[{\"mpack\":true,\"version\":[1,0,0]}]",
      0x91, 0x82, 0xa5, 0x6d, 0x70, 0x61, 0x63, 0x6b, 0xc3, 0xa7,
      0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x93, 0x01, 0x00,
      0x00),
  F("[{\"mpack\":true,\"version\":[1,0,0]},"
     "{\"mpack\":true,\"version\":[1,0,0]}]",
      0x92, 0x82, 0xa5, 0x6d, 0x70, 0x61, 0x63, 0x6b, 0xc3, 0xa7,
      0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x93, 0x01, 0x00,
      0x00, 0x82, 0xa5, 0x6d, 0x70, 0x61, 0x63, 0x6b, 0xc3, 0xa7,
      0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x93, 0x01, 0x00,
      0x00),
  /* fixstr */
  F("\"s:\"",
      0xa0),
  F("\"s:a\"",
      0xa1, 0x61),
  F("\"s:ab\"",
      0xa2, 0x61, 0x62),
  F("\"s:abc\"",
      0xa3, 0x61, 0x62, 0x63),
  F("\"s:abcd\"",
      0xa4, 0x61, 0x62, 0x63, 0x64),
  F("\"s:abcde\"",
      0xa5, 0x61, 0x62, 0x63, 0x64, 0x65),
  F("\"s:abcdef\"",
      0xa6, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66),
  F("\"s:abcdefg\"",
      0xa7, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67),
  F("\"s:abcdefgh\"",
      0xa8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68),
  F("\"s:abcdefghi\"",
      0xa9, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69),
  F("\"s:abcdefghij\"",
      0xaa, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a),
  F("\"s:abcdefghijk\"",
      0xab, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b),
  F("\"s:abcdefghijkl\"",
      0xac, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c),
  F("\"s:abcdefghijklm\"",
      0xad, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d),
  F("\"s:abcdefghijklmn\"",
      0xae, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e),
  F("\"s:abcdefghijklmno\"",
      0xaf, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f),
  F("\"s:abcdefghijklmnop\"",
      0xb0, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70),
  F("\"s:abcdefghijklmnopq\"",
      0xb1, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71),
  F("\"s:abcdefghijklmnopqr\"",
      0xb2, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72),
  F("\"s:abcdefghijklmnopqrs\"",
      0xb3, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73),
  F("\"s:abcdefghijklmnopqrst\"",
      0xb4, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74),
  F("\"s:abcdefghijklmnopqrstu\"",
      0xb5, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75),
  F("\"s:abcdefghijklmnopqrstuv\"",
      0xb6, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76),
  F("\"s:abcdefghijklmnopqrstuvw\"",
      0xb7, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77),
  F("\"s:abcdefghijklmnopqrstuvwx\"",
      0xb8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78),
  F("\"s:abcdefghijklmnopqrstuvwxy\"",
      0xb9, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79),
  F("\"s:abcdefghijklmnopqrstuvwxyz\"",
      0xba, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a),
  F("\"s:abcdefghijklmnopqrstuvwxyz0\"",
      0xbb, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a, 0x30),
  F("\"s:abcdefghijklmnopqrstuvwxyz01\"",
      0xbc, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a, 0x30, 0x31),
  F("\"s:abcdefghijklmnopqrstuvwxyz012\"",
      0xbd, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a, 0x30, 0x31, 0x32),
  F("\"s:abcdefghijklmnopqrstuvwxyz0123\"",
      0xbe, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33),
  F("\"s:abcdefghijklmnopqrstuvwxyz01234\"",
      0xbf, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
      0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33, 0x34),
  F("null", 0xc0),
  F("false", 0xc2),
  F("true", 0xc3),
  /* bin 8 */
  DF(bin_generator, 0x00),
  DF(bin_generator, 0x01),
  DF(bin_generator, 0x7f),
  DF(bin_generator, 0xff),
  /* bin 16 */
  DF(bin_generator, 0x100),
  DF(bin_generator, 0x101),
  DF(bin_generator, 0x7fff),
  DF(bin_generator, 0xffff),
  /* bin 32, skipping very large values to avoid memory starvation */
  DF(bin_generator, 0x10000),
  DF(bin_generator, 0x10001),
  DF(bin_generator, 0x7ffff),
  DF(bin_generator, 0xfffff),
  /* ext 8 */ 
  DF(ext_generator, 0x03),
  DF(ext_generator, 0x07),
  DF(ext_generator, 0x0f),
  /* ext 16 */
  DF(ext_generator, 0x100),
  DF(ext_generator, 0x101),
  DF(ext_generator, 0x7fff),
  DF(ext_generator, 0xffff),
  /* ext 32 */
  DF(ext_generator, 0x10000),
  DF(ext_generator, 0x10001),
  DF(ext_generator, 0x7ffff),
  DF(ext_generator, 0xfffff),
  /* float 32(converted to double) */
  F("0.0", 0xca, 0x00, 0x00, 0x00, 0x00),
  F("1.0", 0xca, 0x3f, 0x80, 0x00, 0x00),
  F("-1.0", 0xca, 0xbf, 0x80, 0x00, 0x00),
  F("-79.967178344726562", 0xca, 0xc2, 0x9f, 0xef, 0x32),
  F("-0.00071464438224211335", 0xca, 0xba, 0x3b, 0x56, 0xf9),
  F("2.460578477798307e+29", 0xca, 0x70, 0x46, 0xc3, 0x92),
  F("1.3502847071725796e+34", 0xca, 0x78, 0x26, 0x6f, 0x79),
  F("-2.1088339408059653e-10", 0xca, 0xaf, 0x67, 0xde, 0x66),
  F("-2.7540517940439024e+30", 0xca, 0xf2, 0x0b, 0x0b, 0x49),
  F("2.1027235734516251e-23", 0xca, 0x19, 0xcb, 0x5c, 0xea),
  /* subnormal float 32 */
  F("1.4012984643248171e-45", 0xca, 0x00, 0x00, 0x00, 0x01),
  F("2.8025969286496341e-45", 0xca, 0x00, 0x00, 0x00, 0x02),
  F("3.5873240686715317e-43", 0xca, 0x00, 0x00, 0x01, 0x00),
  F("-3.5873240686715317e-43", 0xca, 0x80, 0x00, 0x01, 0x00),
  /* float 64 */
  F("4.2487020772743464e-149", 0xcb, 0x21, 0x21, 0x62, 0x73,
                                     0xa0, 0xc9, 0x61, 0xe4),
  F("-3.64378489333011e-199", 0xcb, 0x96, 0xbb, 0xe4, 0x30,
                                    0xd8, 0x4f, 0x48, 0x84),
  F("-3.7908138174749756e+148", 0xcb, 0xde, 0xc7, 0xb7, 0x9e,
                                     0x91, 0x92, 0x99, 0xef),
  F("1.9423862325881795e+243", 0xcb, 0x72, 0x72, 0x34, 0xc8,
                                    0xe2, 0xd1, 0xbc, 0x6c),
  F("-4.5970653956423213e-225", 0xcb, 0x91, 0x5b, 0x39, 0xc0,
                                      0xcc, 0xaf, 0xb4, 0xa4),
  F("1.5856901593454632e-299", 0xcb, 0x01, 0xe5, 0x3d, 0x0e,
                                     0xdf, 0x2f, 0x2b, 0x97),
  F("-2.8033297048969783e-123", 0xcb, 0xa6, 0x7d, 0xa6, 0x88,
                                      0xd4, 0x2f, 0x74, 0xcc),
  F("-2.8033297048969783e-123", 0xcb, 0xa6, 0x7d, 0xa6, 0x88,
                                      0xd4, 0x2f, 0x74, 0xcc),
  /* subnormal float 64 */
  F("4.9406564584124654e-324", 0xcb, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x01),
  F("9.8813129168249309e-324", 0xcb, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x02),
  F("3.3951932655444357e-313", 0xcb, 0x00, 0x00, 0x00, 0x10,
                                     0x00, 0x00, 0x00, 0x00),
  F("-3.3951932655444357e-313", 0xcb, 0x80, 0x00, 0x00, 0x10,
                                      0x00, 0x00, 0x00, 0x00),
  F("-4.9406564584124654e-324", 0xcb, 0x80, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x01),
  F("-9.8813129168249309e-324", 0xcb, 0x80, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x02),
  /* uint 8 */
  F("128", 0xcc, 0x80),
  F("255", 0xcc, 0xff),
  /* uint 16 */
  F("256", 0xcd, 0x01, 0x00),
  F("65535", 0xcd, 0xff, 0xff),
  /* uint 32 */
  F("65536", 0xce, 0x00, 0x01, 0x00, 0x00),
#ifndef FORCE_32BIT_INTS
  F("4294967295", 0xce, 0xff, 0xff, 0xff, 0xff),
  /* uint 64 */
  F("4294967296", 0xcf, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00),
  F("9007199254740991", 0xcf, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
  F("9223372036854775807", 0xcf, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
#endif
  /* int 8 */
  F("-128", 0xd0, 0x80),
  F("-127", 0xd0, 0x81),
  /* int 16 */
  F("-32768", 0xd1, 0x80, 0x00),
  F("-32767", 0xd1, 0x80, 0x01),
  /* int 32 */
  F("-2147483648", 0xd2, 0x80, 0x00, 0x00, 0x00),
  F("-2147483647", 0xd2, 0x80, 0x00, 0x00, 0x01),
#ifndef FORCE_32BIT_INTS
  /* int 64 */
  F("-2147483649", 0xd3, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff),
  F("-2147483650", 0xd3, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfe),
  F("-4294967296", 0xd3, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00),
  F("-9007199254740991", 0xd3, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01),
  F("-9223372036854775808", 0xd3, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
#endif
  /* fixext 1 */
  F("\"e:79:a\"",
      0xd4, 0x79, 0x61),
  /* fixext 2 */
  F("\"e:79:ab\"",
      0xd5, 0x79, 0x61, 0x62),
  /* fixext 4 */
  F("\"e:78:abcd\"",
      0xd6, 0x78, 0x61, 0x62, 0x63, 0x64),
  /* fixext 8 */
  F("\"e:77:abcdefgh\"",
      0xd7, 0x77, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68),
  /* fixext 16 */
  F("\"e:76:abcdefghijklmnop\"",
      0xd8, 0x76, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
                  0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70),
  /* str 8 */
  DF(str_generator, 0x20),
  DF(str_generator, 0x7f),
  DF(str_generator, 0xff),
  /* str 16 */
  DF(str_generator, 0x100),
  DF(str_generator, 0x101),
  DF(str_generator, 0x7fff),
  DF(str_generator, 0xffff),
  /* str 32 */
  DF(str_generator, 0x10000),
  DF(str_generator, 0x10001),
  DF(str_generator, 0x7ffff),
  DF(str_generator, 0xfffff),
  /* array 16 */
  DF(array_generator, 0x10),
  DF(array_generator, 0xff),
  DF(array_generator, 0x100),
  /* array 32 */
  DF(array_generator, 0x10000),
  /* map 16 */
  DF(map_generator, 0x10),
  DF(map_generator, 0xff),
  DF(map_generator, 0x100),
  /* map 32 */
  DF(map_generator, 0x10000),
  /* negative fixint */
  F("-32", 0xe0),
  F("-31", 0xe1),
  F("-30", 0xe2),
  F("-29", 0xe3),
  F("-28", 0xe4),
  F("-27", 0xe5),
  F("-26", 0xe6),
  F("-25", 0xe7),
  F("-24", 0xe8),
  F("-23", 0xe9),
  F("-22", 0xea),
  F("-21", 0xeb),
  F("-20", 0xec),
  F("-19", 0xed),
  F("-18", 0xee),
  F("-17", 0xef),
  F("-16", 0xf0),
  F("-15", 0xf1),
  F("-14", 0xf2),
  F("-13", 0xf3),
  F("-12", 0xf4),
  F("-11", 0xf5),
  F("-10", 0xf6),
  F("-9", 0xf7),
  F("-8", 0xf8),
  F("-7", 0xf9),
  F("-6", 0xfa),
  F("-5", 0xfb),
  F("-4", 0xfc),
  F("-3", 0xfd),
  F("-2", 0xfe),
  F("-1", 0xff),
};

const int fixture_count = ARRAY_SIZE(fixtures);

const struct fixture number_fixtures[] = {
  F("0", 0x00),
  F("1", 0x01),
  F("2", 0x02),
  F("3", 0x03),
  F("4", 0x04),
  F("5", 0x05),
  F("6", 0x06),
  F("7", 0x07),
  F("8", 0x08),
  F("9", 0x09),
  F("10", 0x0a),
  F("11", 0x0b),
  F("12", 0x0c),
  F("13", 0x0d),
  F("14", 0x0e),
  F("15", 0x0f),
  F("16", 0x10),
  F("17", 0x11),
  F("18", 0x12),
  F("19", 0x13),
  F("20", 0x14),
  F("21", 0x15),
  F("22", 0x16),
  F("23", 0x17),
  F("24", 0x18),
  F("25", 0x19),
  F("26", 0x1a),
  F("27", 0x1b),
  F("28", 0x1c),
  F("29", 0x1d),
  F("30", 0x1e),
  F("31", 0x1f),
  F("32", 0x20),
  F("33", 0x21),
  F("34", 0x22),
  F("35", 0x23),
  F("36", 0x24),
  F("37", 0x25),
  F("38", 0x26),
  F("39", 0x27),
  F("40", 0x28),
  F("41", 0x29),
  F("42", 0x2a),
  F("43", 0x2b),
  F("44", 0x2c),
  F("45", 0x2d),
  F("46", 0x2e),
  F("47", 0x2f),
  F("48", 0x30),
  F("49", 0x31),
  F("50", 0x32),
  F("51", 0x33),
  F("52", 0x34),
  F("53", 0x35),
  F("54", 0x36),
  F("55", 0x37),
  F("56", 0x38),
  F("57", 0x39),
  F("58", 0x3a),
  F("59", 0x3b),
  F("60", 0x3c),
  F("61", 0x3d),
  F("62", 0x3e),
  F("63", 0x3f),
  F("64", 0x40),
  F("65", 0x41),
  F("66", 0x42),
  F("67", 0x43),
  F("68", 0x44),
  F("69", 0x45),
  F("70", 0x46),
  F("71", 0x47),
  F("72", 0x48),
  F("73", 0x49),
  F("74", 0x4a),
  F("75", 0x4b),
  F("76", 0x4c),
  F("77", 0x4d),
  F("78", 0x4e),
  F("79", 0x4f),
  F("80", 0x50),
  F("81", 0x51),
  F("82", 0x52),
  F("83", 0x53),
  F("84", 0x54),
  F("85", 0x55),
  F("86", 0x56),
  F("87", 0x57),
  F("88", 0x58),
  F("89", 0x59),
  F("90", 0x5a),
  F("91", 0x5b),
  F("92", 0x5c),
  F("93", 0x5d),
  F("94", 0x5e),
  F("95", 0x5f),
  F("96", 0x60),
  F("97", 0x61),
  F("98", 0x62),
  F("99", 0x63),
  F("100", 0x64),
  F("101", 0x65),
  F("102", 0x66),
  F("103", 0x67),
  F("104", 0x68),
  F("105", 0x69),
  F("106", 0x6a),
  F("107", 0x6b),
  F("108", 0x6c),
  F("109", 0x6d),
  F("110", 0x6e),
  F("111", 0x6f),
  F("112", 0x70),
  F("113", 0x71),
  F("114", 0x72),
  F("115", 0x73),
  F("116", 0x74),
  F("117", 0x75),
  F("118", 0x76),
  F("119", 0x77),
  F("120", 0x78),
  F("121", 0x79),
  F("122", 0x7a),
  F("123", 0x7b),
  F("124", 0x7c),
  F("125", 0x7d),
  F("126", 0x7e),
  F("127", 0x7f),
  F("-79.967178344726562", 0xca, 0xc2, 0x9f, 0xef, 0x32),
  F("-0.00071464438224211335", 0xca, 0xba, 0x3b, 0x56, 0xf9),
  F("128", 0xcc, 0x80),
  F("255", 0xcc, 0xff),
  F("256", 0xcd, 0x01, 0x00),
  F("65535", 0xcd, 0xff, 0xff),
  F("65536", 0xce, 0x00, 0x01, 0x00, 0x00),
  F("4294967295", 0xce, 0xff, 0xff, 0xff, 0xff),
  F("4294967296", 0xcf, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00),
  F("9007199254740991", 0xcf, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
  F("-128", 0xd0, 0x80),
  F("-127", 0xd0, 0x81),
  F("-32768", 0xd1, 0x80, 0x00),
  F("-32767", 0xd1, 0x80, 0x01),
  F("-2147483648", 0xd2, 0x80, 0x00, 0x00, 0x00),
  F("-2147483647", 0xd2, 0x80, 0x00, 0x00, 0x01),
  F("-2147483649", 0xd3, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff),
  F("-2147483650", 0xd3, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfe),
  F("-4294967296", 0xd3, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00),
  F("-9007199254740991", 0xd3, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01),
  F("-32", 0xe0),
  F("-31", 0xe1),
  F("-30", 0xe2),
  F("-29", 0xe3),
  F("-28", 0xe4),
  F("-27", 0xe5),
  F("-26", 0xe6),
  F("-25", 0xe7),
  F("-24", 0xe8),
  F("-23", 0xe9),
  F("-22", 0xea),
  F("-21", 0xeb),
  F("-20", 0xec),
  F("-19", 0xed),
  F("-18", 0xee),
  F("-17", 0xef),
  F("-16", 0xf0),
  F("-15", 0xf1),
  F("-14", 0xf2),
  F("-13", 0xf3),
  F("-12", 0xf4),
  F("-11", 0xf5),
  F("-10", 0xf6),
  F("-9", 0xf7),
  F("-8", 0xf8),
  F("-7", 0xf9),
  F("-6", 0xfa),
  F("-5", 0xfb),
  F("-4", 0xfc),
  F("-3", 0xfd),
  F("-2", 0xfe),
  F("-1", 0xff),
};

const int number_fixture_count = ARRAY_SIZE(number_fixtures);

const struct rpc_fixture rpc_fixtures[] = {
#define REQ(p, i, m, a) {                                                    \
  .payload = p,                                                              \
  .type = MPACK_RPC_REQUEST,                                                 \
  .id = i,                                                                   \
  .method = m,                                                               \
  .args = a,                                                                 \
  .error = NULL,                                                             \
  .result = NULL                                                             \
}
#define RES(p, i, e, r) {                                                    \
  .payload = p,                                                              \
  .type = MPACK_RPC_RESPONSE,                                                \
  .id = i,                                                                   \
  .method = NULL,                                                            \
  .args = NULL,                                                              \
  .error = e,                                                                \
  .result = r                                                                \
}
#define NOT(p, m, a) {                                                       \
  .payload = p,                                                              \
  .type = MPACK_RPC_NOTIFICATION,                                            \
  .method = m,                                                               \
  .args = a,                                                                 \
  .error = NULL,                                                             \
  .result = NULL                                                             \
}

#define ERR(p, t) {                                                          \
  .payload = p,                                                              \
  .type = t,                                                                 \
  .method = NULL,                                                            \
  .args = NULL,                                                              \
  .error = NULL,                                                             \
  .result = NULL                                                             \
}

#define S(...) SC(0, __VA_ARGS__)

#define SC(c, ...) {                                                         \
  .capacity = c,                                                             \
  .messages = (struct rpc_message[]){__VA_ARGS__},                           \
  .count =                                                                   \
  (sizeof((struct rpc_message[]){__VA_ARGS__})/sizeof(struct rpc_message))   \
}

  S(
      REQ("->[0, 0, \"add\", [1, 2]]", 0, "\"add\"", "[1, 2]"),
      RES("<-[1, 0, null, 3]", 0, "null", "3"),
      REQ("->[0, 1, \"sub\", [5, 4]]", 1, "\"sub\"", "[5, 4]"),
      RES("<-[1, 1, null, 1]", 1, "null", "1"),
   ),
  S(
      REQ("<-[0, 0, \"add\", [1, 2]]", 0, "\"add\"", "[1, 2]"),
      REQ("<-[0, 1, \"sub\", [5, 4]]", 1, "\"sub\"", "[5, 4]"),
      RES("->[1, 0, null, 3]", 0, "null", "3"),
      RES("->[1, 1, null, 1]", 1, "null", "1"),
   ),
  S(
      REQ("<-[0, 0, \"add\", [1, 2]]", 0, "\"add\"", "[1, 2]"),
      NOT("<-[2, \"event\", [\"arg\"]]", "\"event\"", "[\"arg\"]"),
      REQ("<-[0, 1, \"sub\", [5, 4]]", 1, "\"sub\"", "[5, 4]"),
      RES("->[1, 1, null, 1]", 1, "null", "1"),
      NOT("<-[2, \"event\", [\"arg\"]]", "\"event\"", "[\"arg\"]"),
      RES("->[1, 0, null, 3]", 0, "null", "3"),
      NOT("<-[2, \"event\", [\"arg\"]]", "\"event\"", "[\"arg\"]"),
   ),
  /* validation errors */
  S(ERR("<-1", MPACK_RPC_EARRAY)),
  S(ERR("<-[0, 0, \"add\", [1, 2], 3]", MPACK_RPC_EARRAYL)),
  S(ERR("<-[0, 0, \"add\"]", MPACK_RPC_EARRAYL)),
  S(ERR("<-[1, 0, \"add\", [1, 2], 3]", MPACK_RPC_EARRAYL)),
  S(ERR("<-[1, 0, \"add\"]", MPACK_RPC_EARRAYL)),
  S(ERR("<-[2, 0, \"add\", [1, 2]]", MPACK_RPC_EARRAYL)),
  S(ERR("<-[2, 0]", MPACK_RPC_EARRAYL)),
  S(ERR("<-[3, 0, 0]", MPACK_RPC_ETYPE)),
  S(ERR("<-[-1, 0, 0]", MPACK_RPC_ETYPE)),
  S(ERR("<-[0, 5.323, 0, 0]", MPACK_RPC_EMSGID)),
  S(ERR("<-[1, 5.323, 0, 0]", MPACK_RPC_EMSGID)),
  S(ERR("<-[1, 4294967296, 0, 0]", MPACK_RPC_EMSGID)),
  S(ERR("<-[1, 0, 0, 0]", MPACK_RPC_ERESPID)),
  S(ERR("<-[1, 4294967295, 0, 0]", MPACK_RPC_ERESPID)),
  SC(2,
      REQ("->[0, 0, \"m\", []]", 0, "\"m\"", "[]"),
      REQ("->[0, 1, \"m\", []]", 1, "\"m\"", "[]"),
      ERR("->[0, 2, \"m\", []]", MPACK_NOMEM),
    ),
  /* hash table tests */
  SC(2,
      REQ("->[0, 0, \"add\", [1, 2]]", 0, "\"add\"", "[1, 2]"),
      REQ("->[0, 1, \"sub\", [1, 2]]", 1, "\"sub\"", "[1, 2]"),
      RES("<-[1, 1, null, -1]", 1, "null", "-1"),
      RES("<-[1, 0, null, 3]", 0, "null", "3"),
    ),
  SC(3,
      REQ("->[0, 0, \"m\", []]", 0, "\"m\"", "[]"),
      RES("<-[1, 0, null, null]", 0, "null", "null"),
      REQ("->[0, 1, \"m\", []]", 1, "\"m\"", "[]"),
      REQ("->[0, 2, \"m\", []]", 2, "\"m\"", "[]"),
      RES("<-[1, 2, null, null]", 2, "null", "null"),
      REQ("->[0, 3, \"m\", []]", 3, "\"m\"", "[]"),
      RES("<-[1, 3, null, null]", 3, "null", "null"),
      REQ("->[0, 4, \"m\", []]", 4, "\"m\"", "[]"),
      REQ("->[0, 5, \"m\", []]", 5, "\"m\"", "[]"),
      RES("<-[1, 5, null, null]", 5, "null", "null"),
    ),
  SC(5,
      REQ("->[0, 0, \"m\", []]", 0, "\"m\"", "[]"),
      RES("<-[1, 0, null, null]", 0, "null", "null"),
      REQ("->[0, 1, \"m\", []]", 1, "\"m\"", "[]"),
      RES("<-[1, 1, null, null]", 1, "null", "null"),
      REQ("->[0, 2, \"m\", []]", 2, "\"m\"", "[]"),
      REQ("->[0, 3, \"m\", []]", 3, "\"m\"", "[]"),
      RES("<-[1, 3, null, null]", 3, "null", "null"),
      REQ("->[0, 4, \"m\", []]", 4, "\"m\"", "[]"),
      RES("<-[1, 4, null, null]", 4, "null", "null"),
      REQ("->[0, 5, \"m\", []]", 5, "\"m\"", "[]"),
      RES("<-[1, 5, null, null]", 5, "null", "null"),
      REQ("->[0, 6, \"m\", []]", 6, "\"m\"", "[]"),
      RES("<-[1, 6, null, null]", 6, "null", "null"),
      REQ("->[0, 7, \"m\", []]", 7, "\"m\"", "[]"),
      REQ("->[0, 8, \"m\", []]", 8, "\"m\"", "[]"),
      REQ("->[0, 9, \"m\", []]", 9, "\"m\"", "[]"),
      RES("<-[1, 2, null, null]", 2, "null", "null"),
      RES("<-[1, 7, null, null]", 7, "null", "null"),
      RES("<-[1, 9, null, null]", 9, "null", "null"),
      RES("<-[1, 8, null, null]", 8, "null", "null"),
      REQ("->[0, 10, \"m\", []]", 10, "\"m\"", "[]"),
      RES("<-[1, 10, null, null]", 10, "null", "null"),
      REQ("->[0, 11, \"m\", []]", 11, "\"m\"", "[]"),
      REQ("->[0, 12, \"m\", []]", 12, "\"m\"", "[]"),
      RES("<-[1, 12, null, null]", 12, "null", "null"),
      REQ("->[0, 13, \"m\", []]", 13, "\"m\"", "[]"),
      RES("<-[1, 13, null, null]", 13, "null", "null"),
      REQ("->[0, 14, \"m\", []]", 14, "\"m\"", "[]"),
      REQ("->[0, 15, \"m\", []]", 15, "\"m\"", "[]"),
      REQ("->[0, 16, \"m\", []]", 16, "\"m\"", "[]"),
      REQ("->[0, 17, \"m\", []]", 17, "\"m\"", "[]"),
      RES("<-[1, 15, null, null]", 15, "null", "null"),
      RES("<-[1, 17, null, null]", 17, "null", "null"),
      RES("<-[1, 14, null, null]", 14, "null", "null"),
      RES("<-[1, 11, null, null]", 11, "null", "null"),
    ),

};

const int rpc_fixture_count = ARRAY_SIZE(rpc_fixtures);
