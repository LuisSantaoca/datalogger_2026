#include "FORMATModule.h"

FormatModule::FormatModule() {
  reset();
}

void FormatModule::fillZeros(char* dst, uint8_t width) {
  for (uint8_t i = 0; i < width; i++) {
    dst[i] = '0';
  }
  dst[width] = '\0';
}

void FormatModule::copyRightAligned(char* dst, uint8_t width, const char* src) {
  fillZeros(dst, width);

  size_t srcLen = strlen(src);
  bool hasNegative = (srcLen > 0 && src[0] == '-');
  
  if (srcLen > width) {
    // Si tiene signo negativo y excede el ancho, preservar el signo
    if (hasNegative) {
      dst[0] = '-';
      const char* srcDigits = src + 1;
      size_t digitsLen = srcLen - 1;
      size_t copyLen = width - 1;
      if (digitsLen > copyLen) {
        srcDigits += (digitsLen - copyLen);
        digitsLen = copyLen;
      }
      uint8_t offset = static_cast<uint8_t>(width - digitsLen);
      for (size_t i = 0; i < digitsLen; i++) {
        dst[offset + i] = srcDigits[i];
      }
    } else {
      src += (srcLen - width);
      srcLen = width;
      uint8_t offset = static_cast<uint8_t>(width - srcLen);
      for (size_t i = 0; i < srcLen; i++) {
        dst[offset + i] = src[i];
      }
    }
  } else {
    uint8_t offset = static_cast<uint8_t>(width - srcLen);
    for (size_t i = 0; i < srcLen; i++) {
      dst[offset + i] = src[i];
    }
  }
}

void FormatModule::copyCoordAligned(char* dst, uint8_t width, const char* src) {
  // Simplemente copiar el string tal cual
  size_t srcLen = strlen(src);
  size_t copyLen = (srcLen < width) ? srcLen : width;
  
  for (size_t i = 0; i < copyLen; i++) {
    dst[i] = src[i];
  }
  dst[copyLen] = '\0';
}

void FormatModule::reset() {
  fillZeros(iccid_, ICCID_LEN);
  fillZeros(epoch_, EPOCH_LEN);
  fillZeros(lat_, COORD_LEN);
  fillZeros(lng_, COORD_LEN);
  fillZeros(alt_, ALT_LEN);

  for (uint8_t i = 0; i < VAR_COUNT; i++) {
    fillZeros(vars_[i], VAR_LEN);
  }
}

void FormatModule::setIccid(const char* iccid) {
  if (iccid == nullptr) {
    fillZeros(iccid_, ICCID_LEN);
    return;
  }
  copyRightAligned(iccid_, ICCID_LEN, iccid);
}

void FormatModule::setEpoch(const char* epoch) {
  if (epoch == nullptr) {
    fillZeros(epoch_, EPOCH_LEN);
    return;
  }
  copyRightAligned(epoch_, EPOCH_LEN, epoch);
}

void FormatModule::setLat(const char* lat) {
  if (lat == nullptr) {
    fillZeros(lat_, COORD_LEN);
    return;
  }
  copyCoordAligned(lat_, COORD_LEN, lat);
}

void FormatModule::setLng(const char* lng) {
  if (lng == nullptr) {
    fillZeros(lng_, COORD_LEN);
    return;
  }
  copyCoordAligned(lng_, COORD_LEN, lng);
}

void FormatModule::setAlt(const char* alt) {
  if (alt == nullptr) {
    fillZeros(alt_, ALT_LEN);
    return;
  }
  copyRightAligned(alt_, ALT_LEN, alt);
}

bool FormatModule::setVar(uint8_t index, const char* value) {
  if (index >= VAR_COUNT) {
    return false;
  }
  if (value == nullptr) {
    fillZeros(vars_[index], VAR_LEN);
    return true;
  }
  copyRightAligned(vars_[index], VAR_LEN, value);
  return true;
}

void FormatModule::setVar1(const char* value) {
  setVar(0, value);
}

void FormatModule::setVar2(const char* value) {
  setVar(1, value);
}

void FormatModule::setVar3(const char* value) {
  setVar(2, value);
}

void FormatModule::setVar4(const char* value) {
  setVar(3, value);
}

void FormatModule::setVar5(const char* value) {
  setVar(4, value);
}

void FormatModule::setVar6(const char* value) {
  setVar(5, value);
}

void FormatModule::setVar7(const char* value) {
  setVar(6, value);
}

bool FormatModule::buildFrame(char* outBuffer, size_t outSize) const {
  if (outBuffer == nullptr) {
    return false;
  }
  if (outSize < FRAME_MAX_LEN) {
    return false;
  }

  size_t pos = 0;

  outBuffer[pos++] = '$';
  outBuffer[pos++] = ',';

  memcpy(&outBuffer[pos], iccid_, ICCID_LEN);
  pos += ICCID_LEN;
  outBuffer[pos++] = ',';

  memcpy(&outBuffer[pos], epoch_, EPOCH_LEN);
  pos += EPOCH_LEN;
  outBuffer[pos++] = ',';

  size_t latLen = strlen(lat_);
  memcpy(&outBuffer[pos], lat_, latLen);
  pos += latLen;
  outBuffer[pos++] = ',';

  size_t lngLen = strlen(lng_);
  memcpy(&outBuffer[pos], lng_, lngLen);
  pos += lngLen;
  outBuffer[pos++] = ',';

  memcpy(&outBuffer[pos], alt_, ALT_LEN);
  pos += ALT_LEN;

  for (uint8_t i = 0; i < VAR_COUNT; i++) {
    outBuffer[pos++] = ',';
    memcpy(&outBuffer[pos], vars_[i], VAR_LEN);
    pos += VAR_LEN;
  }

  outBuffer[pos++] = ',';
  outBuffer[pos++] = '#';
  outBuffer[pos] = '\0';

  return true;
}

size_t FormatModule::encodeBase64(const uint8_t* data,
                                 size_t dataLen,
                                 char* outBuffer,
                                 size_t outSize) {
  static const char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (outBuffer == nullptr) {
    return 0;
  }

  size_t outLen = ((dataLen + 2) / 3) * 4;
  if (outSize < (outLen + 1)) {
    return 0;
  }

  size_t i = 0;
  size_t o = 0;

  while (i < dataLen) {
    uint32_t a = i < dataLen ? data[i++] : 0;
    uint32_t b = i < dataLen ? data[i++] : 0;
    uint32_t c = i < dataLen ? data[i++] : 0;

    uint32_t triple = (a << 16) | (b << 8) | c;

    outBuffer[o++] = table[(triple >> 18) & 0x3F];
    outBuffer[o++] = table[(triple >> 12) & 0x3F];
    outBuffer[o++] = (i - 1) < dataLen ? table[(triple >> 6) & 0x3F] : '=';
    outBuffer[o++] = i < (dataLen + 1) ? table[triple & 0x3F] : '=';
  }

  if ((dataLen % 3) == 1) {
    outBuffer[outLen - 2] = '=';
    outBuffer[outLen - 1] = '=';
  } else if ((dataLen % 3) == 2) {
    outBuffer[outLen - 1] = '=';
  }

  outBuffer[outLen] = '\0';
  return outLen;
}

bool FormatModule::buildFrameBase64(char* outBuffer, size_t outSize) const {
  char frame[FRAME_MAX_LEN];
  bool ok = buildFrame(frame, sizeof(frame));
  if (!ok) {
    return false;
  }

  size_t frameLen = strlen(frame);
  size_t outLen = encodeBase64(reinterpret_cast<const uint8_t*>(frame),
                               frameLen,
                               outBuffer,
                               outSize);
  return outLen > 0;
}
