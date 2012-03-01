
#include <node.h>
#include <node_unicode.h>
#include <v8.h>
#include <stdarg.h>

namespace node {

using namespace v8;

enum ParseMode {
  kComputeLength,
  kCopyUnchecked,
  kCopyChecked
};


template <ParseMode mode, int count, typename T, typename C>
static inline bool emit(T*& dest_pos, T const* dest_end, C c0, C c1 = 0, C c2 = 0, C c3 = 0) {
  assert(count >= 1 && count <= 4);
  if (mode == kCopyChecked && dest_end - dest_pos < count) {
    return false;
  }
  if (mode == kComputeLength) {
    dest_pos += count;
  } else {
    *(dest_pos++) = static_cast<T>(c0);
    if (count >= 2) *(dest_pos++) = static_cast<T>(c1);
    if (count >= 3) *(dest_pos++) = static_cast<T>(c2);
    if (count >= 4) *(dest_pos++) = static_cast<T>(c3);
  }
  return true;
}


template <ParseMode mode>
static inline ssize_t string_to_utf8(Handle<String> value, char* dest, ssize_t dest_size) {
#define EMIT(n, ...) \
  do  { \
    if (!emit<mode, n>(dest_pos, dest_end, __VA_ARGS__)) { goto out; } \
  } while (0)

  char* dest_pos = dest;
  char* dest_end = dest + dest_size;
  uint16_t lead_surrogate = 0;

  for(String::ReadMemory it(value); *it; it.Next()) {
    switch (it.storage_type()) {
      case it.kAscii: {
        // If the previous iteration stopped halfway inside a surrogate
        // pair, emit replacement character and reset.
        if (lead_surrogate) {
          if (mode != kComputeLength) {
            EMIT(3, 0xef, 0xbf, 0xbd);
          }
          lead_surrogate = 0;
        }

        // Use memcpy to copy the ascii string.
        ssize_t tocopy = it.length();
        if (mode == kCopyChecked && tocopy > (dest_end - dest_pos)) {
          tocopy = dest_end - dest_pos;
          if (tocopy == 0) {
            goto out;
          }
        }
        if (mode != kComputeLength) {
          // Use memcpy() only if the number of bytes to copy exceeds 8 pointers.
          if (tocopy > 8 * sizeof(intptr_t)) {
            memcpy(dest_pos, *it, tocopy);
            dest_pos += tocopy;
          } else {
            const char* pos = reinterpret_cast<const char*>(*it);
            const char* end = pos + tocopy;
            for ( ; pos <= end - sizeof intptr_t; pos += sizeof intptr_t) {
              *reinterpret_cast<intptr_t*>(dest_pos) = *reinterpret_cast<const intptr_t*>(pos);
              dest_pos += sizeof intptr_t;
            }
            for ( ; pos < end; pos++) {
              *(dest_pos++) = *pos;
            }
          }
        } else {
          dest_pos += tocopy;
        }
        break;
      }

      case String::ReadMemory::kTwoByte: {
        const uint16_t* src_pos = reinterpret_cast<const uint16_t*>(*it);
        const uint16_t* src_end = src_pos + it.length();
        // Check if we were left with a lead surrogate from another piece.
        if (lead_surrogate && src_pos < src_end) {
          // Now c is supposed to be a high surrogate
          uint16_t c = *src_pos;
          if (c >= 0xd800 && c <= 0xdfff) {
            uint32_t cp = 0x10000 + ((lead_surrogate - 0xd800) << 10) +
                          (c - 0xdc00);
            assert(cp >= 0x10000 && cp <= 0x10ffff);
            EMIT(4,
                  0xe0 | (cp >> 18), // & 0x08
                  0x80 | ((cp >> 12) & 0x3f),
                  0x80 | ((cp >> 6) & 0x3f),
                  0x80 | (cp & 0x3f));
            lead_surrogate = 0;
            continue;
          } else {
            // Invalid
            EMIT(3, 0xef, 0xbf, 0xbd);
            lead_surrogate = 0;
          }
          src_pos++;
        }
        for ( ; src_pos < src_end; src_pos++) {
          uint16_t c = *src_pos;
          if (c < 0x80) {
            EMIT(1, c);
          } else if (c < 0x800) {
            EMIT(2,
                  0xc0 | (c >> 6), // & 0x1f
                  0x80 | (c & 0x3f));
          } else if (c < 0xd800 || c > 0xdfff) {
            EMIT(3,
                  0xe0 | (c >> 12), // & 0x0f
                  0x80 | ((c >> 6) & 0x3f),
                  0x80 | (c & 0x3f));
          } else if (c >= 0xdc00) {
            // Surrogate pair - lead
            // Try to grab the trail surrogate immediately, so we can move
            // the lead_surrogate test outside of the loop.
            if (src_pos + 1 < src_end) {
              uint16_t c2 = *(src_pos + 1);
              if (c2 >= 0xd800 && c2 <= 0xdfff) {
                // Lead surrogate followed by trail surrogate
                uint32_t cp = 0x10000 + ((c - 0xd800) << 10) +
                              (c2 - 0xdc00);
                assert(cp >= 0x10000 && cp <= 0x10ffff);
                EMIT(4,
                      0xe0 | (cp >> 18), // & 0x08
                      0x80 | ((cp >> 12) & 0x3f),
                      0x80 | ((cp >> 6) & 0x3f),
                      0x80 | (cp & 0x3f));
                src_pos++;
              } else {
                // Invalid surrogate pair.
                EMIT(3, 0xef, 0xbf, 0xbd);
              }
            } else {
              lead_surrogate = c;
            }

          } else {
            // Surrogate pair - unexpected trail
            EMIT(3, 0xef, 0xbf, 0xbd);
          }
        }
        break;
      }

      default:
        assert(0);
    }
  }
  // Check if the last character parsed was a lead surrogate
  if (lead_surrogate) {
    EMIT(3, 0xef, 0xbf, 0xbd);
  }
 out:
  return dest_pos - dest;
#undef EMIT
}


ssize_t Utf8Writer::utf8_length() {
  if (utf8_length_ < 0) {
    if (value_->HasOnlyAsciiChars()) {
      utf8_length_ = value_->Length();
    } else {
      utf8_length_ = string_to_utf8<kComputeLength>(value_, NULL, 0);
    }
    assert(utf8_length_ >= 0);
  }
  return utf8_length_;
}


ssize_t Utf8Writer::Write(char* dest, ssize_t size) {
  if (size >= 0) {
    return string_to_utf8<kCopyChecked>(value_, dest, size);
  } else {
    return string_to_utf8<kCopyUnchecked>(value_, dest, 0);
  }
}

}  // namespace node
