#pragma once
#include <cstdint>

// Runtime pattern parameters. Mirrors the TS PatternParams blob: a small
// keyed bag of number | bool | string. Patterns read with a default, exactly
// like num(params.foo, 4) in TS, so an empty Params yields every pattern's
// defaults. Fixed capacity, no heap.

namespace cube {

class Params {
 public:
  static constexpr int kMax = 32;
  static constexpr int kKeyLen = 24;
  static constexpr int kStrLen = 20;

  void clear() { count_ = 0; }

  float num(const char* key, float def) const;
  bool boolean(const char* key, bool def) const;
  const char* str(const char* key, const char* def) const;

  bool setNum(const char* key, float v);
  bool setBool(const char* key, bool v);
  bool setStr(const char* key, const char* v);

 private:
  enum Type : uint8_t { NUM, BOOL, STR };
  struct Entry {
    char key[kKeyLen];
    Type type;
    float num;
    bool b;
    char str[kStrLen];
  };

  const Entry* find(const char* key) const;
  Entry* upsert(const char* key);

  Entry entries_[kMax];
  int count_ = 0;
};

}  // namespace cube
