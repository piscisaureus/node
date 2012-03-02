
#ifndef node_unicode_h
#define node_unicode_h

#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include <node.h>
#include <v8.h>


namespace node {

using namespace v8;


class Utf8Writer {
 public:
  Utf8Writer(Handle<String> value)
      : utf8_length_(-1), value_(value) {
  }
  
  ~Utf8Writer() {}

  ssize_t utf8_length();
  ssize_t Write(char* dest, ssize_t size);
  
 private:
  ssize_t utf8_length_;
  Handle<String> value_;
};
 
}  // namespace node

#endif  // node_unicode_h
