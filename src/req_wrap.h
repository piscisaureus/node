// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef REQ_WRAP_H_
#define REQ_WRAP_H_

#include <wrap.h>

namespace node {
using namespace v8;


template <typename T>
class ReqWrap: public Wrap {
 public:
  ReqWrap()
    : Wrap(Constructor<ReqWrap>()->NewInstance()) {
  }

  ReqWrap(Handle<Object> object)
    : Wrap(object) {
  }

  ~ReqWrap() {
    // Assert that someone has called Dispatched()
    assert(req_.data == this);
  }

  // Call this after the req has been dispatched.
  void Dispatched() {
    req_.data = this;
  }

  T req_;
  void* data_;
};


}  // namespace node


#endif  // REQ_WRAP_H_
