// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "StringTools.h"
#include <fstream>
#include <iomanip>

namespace Common {

namespace {

const uint8_t characterValues[256] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

}

std::string asString(const void* data, uint64_t size) {
  return std::string(static_cast<const char*>(data), size);
}

std::string asString(const std::vector<uint8_t>& data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<uint8_t> asBinaryArray(const std::string& data) {
  auto dataPtr = reinterpret_cast<const uint8_t*>(data.data());
  return std::vector<uint8_t>(dataPtr, dataPtr + data.size());
}

uint8_t fromHex(char character) {
  uint8_t value = characterValues[static_cast<unsigned char>(character)];
  if (value > 0x0f) {
    throw std::runtime_error("fromHex: invalid character");
  }

  return value;
}

bool fromHex(char character, uint8_t& value) {
  if (characterValues[static_cast<unsigned char>(character)] > 0x0f) {
    return false;
  }

  value = characterValues[static_cast<unsigned char>(character)];
  return true;
}

uint64_t fromHex(const std::string& text, void* data, uint64_t bufferSize) {
  if ((text.size() & 1) != 0) {
    throw std::runtime_error("fromHex: invalid string size");
  }

  if (text.size() >> 1 > bufferSize) {
    throw std::runtime_error("fromHex: invalid buffer size");
  }

  for (uint64_t i = 0; i < text.size() >> 1; ++i) {
    static_cast<uint8_t*>(data)[i] = fromHex(text[i << 1]) << 4 | fromHex(text[(i << 1) + 1]);
  }

  return text.size() >> 1;
}

bool fromHex(const std::string& text, void* data, uint64_t bufferSize, uint64_t& size) {
  if ((text.size() & 1) != 0) {
    return false;
  }

  if (text.size() >> 1 > bufferSize) {
    return false;
  }

  for (uint64_t i = 0; i < text.size() >> 1; ++i) {
    uint8_t value1;
    if (!fromHex(text[i << 1], value1)) {
      return false;
    }

    uint8_t value2;
    if (!fromHex(text[(i << 1) + 1], value2)) {
      return false;
    }

    static_cast<uint8_t*>(data)[i] = value1 << 4 | value2;
  }

  size = text.size() >> 1;
  return true;
}

std::vector<uint8_t> fromHex(const std::string& text) {
  if ((text.size() & 1) != 0) {
    throw std::runtime_error("fromHex: invalid string size");
  }

  std::vector<uint8_t> data(text.size() >> 1);
  for (uint64_t i = 0; i < data.size(); ++i) {
    data[i] = fromHex(text[i << 1]) << 4 | fromHex(text[(i << 1) + 1]);
  }

  return data;
}

bool fromHex(const std::string& text, std::vector<uint8_t>& data) {
  if ((text.size() & 1) != 0) {
    return false;
  }

  for (uint64_t i = 0; i < text.size() >> 1; ++i) {
    uint8_t value1;
    if (!fromHex(text[i << 1], value1)) {
      return false;
    }

    uint8_t value2;
    if (!fromHex(text[(i << 1) + 1], value2)) {
      return false;
    }

    data.push_back(value1 << 4 | value2);
  }

  return true;
}

std::string toHex(const void* data, uint64_t size) {
  std::string text;
  for (uint64_t i = 0; i < size; ++i) {
    text += "0123456789abcdef"[static_cast<const uint8_t*>(data)[i] >> 4];
    text += "0123456789abcdef"[static_cast<const uint8_t*>(data)[i] & 15];
  }

  return text;
}

void toHex(const void* data, uint64_t size, std::string& text) {
  for (uint64_t i = 0; i < size; ++i) {
    text += "0123456789abcdef"[static_cast<const uint8_t*>(data)[i] >> 4];
    text += "0123456789abcdef"[static_cast<const uint8_t*>(data)[i] & 15];
  }
}

std::string toHex(const std::vector<uint8_t>& data) {
  std::string text;
  for (uint64_t i = 0; i < data.size(); ++i) {
    text += "0123456789abcdef"[data[i] >> 4];
    text += "0123456789abcdef"[data[i] & 15];
  }

  return text;
}

void toHex(const std::vector<uint8_t>& data, std::string& text) {
  for (uint64_t i = 0; i < data.size(); ++i) {
    text += "0123456789abcdef"[data[i] >> 4];
    text += "0123456789abcdef"[data[i] & 15];
  }
}

std::string extract(std::string& text, char delimiter) {
  uint64_t delimiterPosition = text.find(delimiter);
  std::string subText;
  if (delimiterPosition != std::string::npos) {
    subText = text.substr(0, delimiterPosition);
    text = text.substr(delimiterPosition + 1);
  } else {
    subText.swap(text);
  }

  return subText;
}

std::string extract(const std::string& text, char delimiter, uint64_t& offset) {
  uint64_t delimiterPosition = text.find(delimiter, offset);
  if (delimiterPosition != std::string::npos) {
    offset = delimiterPosition + 1;
    return text.substr(offset, delimiterPosition);
  } else {
    offset = text.size();
    return text.substr(offset);
  }
}

std::string ipAddressToString(uint32_t ip) {
  uint8_t bytes[4];
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;

  char buf[16];
  sprintf(buf, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);

  return std::string(buf);
}

bool parseIpAddressAndPort(uint32_t& ip, uint32_t& port, const std::string& addr) {
  uint32_t v[4];
  uint32_t localPort;

  if (sscanf(addr.c_str(), "%d.%d.%d.%d:%d", &v[0], &v[1], &v[2], &v[3], &localPort) != 5) {
    return false;
  }

  for (int i = 0; i < 4; ++i) {
    if (v[i] > 0xff) {
      return false;
    }
  }

  ip = (v[3] << 24) | (v[2] << 16) | (v[1] << 8) | v[0];
  port = localPort;
  return true;
}

std::string timeIntervalToString(uint64_t intervalInSeconds) {
  auto tail = intervalInSeconds;

  auto days = tail / (60 * 60 * 24);
  tail = tail % (60 * 60 * 24);
  auto hours = tail / (60 * 60);
  tail = tail % (60 * 60);
  auto  minutes = tail / (60);
  tail = tail % (60);
  auto seconds = tail;

  std::stringstream ss;
  ss << "d" << days <<
    std::setfill('0') <<
    ".h" << std::setw(2) << hours <<
    ".m" << std::setw(2) << minutes <<
    ".s" << std::setw(2) << seconds;

  return ss.str();
}

}
