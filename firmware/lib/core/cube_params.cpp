#include "cube_params.h"

#include <cstring>

namespace cube {

const Params::Entry* Params::find(const char* key) const {
  for (int i = 0; i < count_; i++) {
    if (std::strcmp(entries_[i].key, key) == 0) return &entries_[i];
  }
  return nullptr;
}

Params::Entry* Params::upsert(const char* key) {
  for (int i = 0; i < count_; i++) {
    if (std::strcmp(entries_[i].key, key) == 0) return &entries_[i];
  }
  if (count_ >= kMax) return nullptr;
  Entry* e = &entries_[count_++];
  std::strncpy(e->key, key, kKeyLen - 1);
  e->key[kKeyLen - 1] = '\0';
  return e;
}

float Params::num(const char* key, float def) const {
  const Entry* e = find(key);
  return (e && e->type == NUM) ? e->num : def;
}

bool Params::boolean(const char* key, bool def) const {
  const Entry* e = find(key);
  return (e && e->type == BOOL) ? e->b : def;
}

const char* Params::str(const char* key, const char* def) const {
  const Entry* e = find(key);
  return (e && e->type == STR) ? e->str : def;
}

bool Params::setNum(const char* key, float v) {
  Entry* e = upsert(key);
  if (!e) return false;
  e->type = NUM;
  e->num = v;
  return true;
}

bool Params::setBool(const char* key, bool v) {
  Entry* e = upsert(key);
  if (!e) return false;
  e->type = BOOL;
  e->b = v;
  return true;
}

bool Params::setStr(const char* key, const char* v) {
  if (!v) return false;  // e.g. JSON as<const char*>() on a non-string value
  Entry* e = upsert(key);
  if (!e) return false;
  e->type = STR;
  std::strncpy(e->str, v, kStrLen - 1);
  e->str[kStrLen - 1] = '\0';
  return true;
}

}  // namespace cube
