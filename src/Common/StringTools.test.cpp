#include "gtest/gtest.h"

#include <string>
#include <exception>

#include "StringTools.h"

TEST(StringUtilsTest, AsStringConvertsBinaryBufferToString) {
  uint8_t binaries[] = {84, 82, 84, 76, 112, 111, 119, 101, 114};
  uint64_t size = 9;

  EXPECT_EQ(
    Common::asString(&binaries,size),
    "TRTLpower"
  );
}

TEST(StringUtilsTest, AsStringConvertsCharBufferToString) {
  char chars[] = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r'};
  uint64_t size = 9;

  EXPECT_EQ(
    Common::asString(&chars,size),
    "TRTLpower"
  );
}

TEST(StringUtilsTest, AsStringConvertsBinaryVectorToString) {
  std::vector<uint8_t> binaries = {84, 82, 84, 76, 112, 111, 119, 101, 114};

  EXPECT_EQ(
    Common::asString(binaries),
    "TRTLpower"
  );
}

TEST(StringUtilsTest, AsBinaryArrayConvertsStringToBinaryArray) {
  std::string str = "TRTLpower";
  std::vector<uint8_t> binary = {84, 82, 84, 76, 112, 111, 119, 101, 114};

  EXPECT_EQ(
    Common::asBinaryArray(str),
    binary
  );
}

TEST(StringUtilsTest, FromHexConvertsHexCharToBinary) {
  char character = 'F';
  uint8_t binary = 0xF;

  EXPECT_EQ(
    Common::fromHex(character),
    binary
  );
}

TEST(StringUtilsTest, FromHexThrowsErrorForNonHexChar) {
  char character = 'K';

  try {
    Common::fromHex(character);
  }
  catch (std::runtime_error& e) {

    EXPECT_EQ(
        std::string("fromHex: invalid character"),
        e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexWritesBinaryToValue) {
  char character = 'F';
  uint8_t binary = 0xF;
  uint8_t value;

  Common::fromHex(character, value);

  EXPECT_EQ(
    value,
    binary
  );
}

TEST(StringUtilsTest, FromHexReturnsTrueForValidHexChar) {
  char character = 'F';
  uint8_t value;

  EXPECT_TRUE(
    Common::fromHex(character, value)
  );
}

TEST(StringUtilsTest, FromHexReturnsFalseForInvalidHexChar) {
  char character = 'G';
  uint8_t value;

  EXPECT_FALSE(
    Common::fromHex(character, value)
  );
}

TEST(StringUtilsTest, FromHexWritesStringToBuffer) {
  std::string text = "5452544C706f77657200";
  char data[10];
  char str[] = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r', 0};

  Common::fromHex(text, &data, (*(&data + 1) - data));

  EXPECT_STREQ(
    data,
    str
  );
}

TEST(StringUtilsTest, FromHexReturnsTheDataSize) {
  std::string text = "706f77657200";
  char data[6];

  EXPECT_EQ(
    Common::fromHex(text, &data, (*(&data + 1) - data)),
    6
  );
}

TEST(StringUtilsTest, FromHexThrowsErrorForNonHexChars) {
  std::string text = "545CE44C706f77657200";
  char data[10];

  try {
    Common::fromHex(text, &data, (*(&data + 1) - data));
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid character"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexThrowsErrorForInvalidStringSize) {
  std::string text = "452544C4706f77657200";
  char data[10];

  try {
    Common::fromHex(text, &data, (*(&data + 1) - data));
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid string size"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexThrowsErrorForInvalidBufferSize) {
  std::string text = "5452544C706f77657200";
  char data[5];

  try {
    Common::fromHex(text, &data, 5);
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid buffer size"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexWithSizeWritesStringToBufferAndSizeToValue) {
  std::string text = "5452544C706f77657200";
  char data[10];
  char str[] = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r', 0};
  uint64_t size;

  Common::fromHex(text, &data, (*(&data + 1) - data), size);

  EXPECT_STREQ(
    data,
    str
  );

  EXPECT_EQ(
    size,
    10
  );
}

TEST(StringUtilsTest, FromHexWithSizeThrowsErrorForNonHexChars) {
  std::string text = "545CE44C706f77657200";
  char data[10];
  uint64_t size;

  try {
    Common::fromHex(text, &data, (*(&data + 1) - data), size);
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid character"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexWithSizeThrowsErrorForInvalidStringSize) {
  std::string text = "452544C4706f77657200";
  char data[10];
  uint64_t size;

  try {
    Common::fromHex(text, &data, (*(&data + 1) - data), size);
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid string size"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexWithSizeThrowsErrorForInvalidBufferSize) {
  std::string text = "5452544C706f77657200";
  char data[5];
  uint64_t size;

  try {
    Common::fromHex(text, &data, 5, size);
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid buffer size"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexToVectorConvertsHexStringToVector)  {
  std::string text         = "5452544C706f77657200";
  std::vector<uint8_t> str =  {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r', 0};

  EXPECT_EQ(
    Common::fromHex(text),
    str
  );
}

TEST(StringUtilsTest, FromHexToVectorThrowsErrorForNonHexChars) {
  std::string text = "545G544C706f77657200";
  try {
    Common::fromHex(text);
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid character"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexToVectorThrowsErrorForInvalidStringSize) {
  std::string text = "452544C706f77657200";
  try {
    Common::fromHex(text);
  }
  catch(std::runtime_error& e) {
    EXPECT_EQ(
      std::string("fromHex: invalid string size"),
      e.what()
    );
  }
}

TEST(StringUtilsTest, FromHexAppendsStringToVector)  {
  std::string text = "5452544C706f77657200";
  std::vector<uint8_t> str  = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r', 0};
  std::vector<uint8_t> data = {};

  Common::fromHex(text, data);
  EXPECT_EQ(
    data,
    str
  );
}

TEST(StringUtilsTest, FromHexReturnsFalseForInvalidStringLength)  {
  std::string text = "452544C706f77657200";
  std::vector<uint8_t> data = {};

  EXPECT_FALSE(
    Common::fromHex(text, data)
  );
}

TEST(StringUtilsTest, FromHexReturnsFalseForNonHexCharacters)  {
  std::string text = "545G544C706f77657200";
  std::vector<uint8_t> data = {};

  EXPECT_FALSE(
    Common::fromHex(text, data)
  );
}

TEST(StringUtilsTest, ToHexConvertsStringToHex) {
  char data[]      = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r'};
  std::string hex  = "5452544c706f776572";

  EXPECT_EQ(
    Common::toHex(&data, 9),
    hex
  );
}

TEST(StringUtilsTest, ToHexAppendsHexToString) {
  char data[]      = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r'};
  std::string hex  = "5452544c706f776572";
  std::string text = "";

  Common::toHex(&data, 9, text);

  EXPECT_EQ(
    text,
    hex
  );
}

TEST(StringUtilsTest, ToHexReturnsHexString) {
  std::vector<uint8_t> data = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r'};
  std::string hex           = "5452544c706f776572";

  EXPECT_EQ(
    Common::toHex(data),
    hex
  );
}

TEST(StringUtilsTest, ToHexAppendsHexString) {
  std::vector<uint8_t> data = {'T', 'R', 'T', 'L', 'p', 'o', 'w', 'e', 'r'};
  std::string hex           = "5452544c706f776572";
  std::string text          = "";

  Common::toHex(data, text);

  EXPECT_EQ(
    text,
    hex
  );
}
