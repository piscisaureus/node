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

#include <node.h>
#include <node_buffer.h>
#include <req_wrap.h>
#include <handle_wrap.h>
#include <stream_wrap.h>

namespace node {

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;
using v8::Undefined;

#define UNWRAP \
  TTYWrap* wrap = Unwrap<TTYWrap>(args); \
  if (!wrap) { \
    uv_err_t err; \
    err.code = UV_EBADF; \
    SetErrno(err); \
    return scope.Close(Integer::New(-1)); \
  }


class TTYWrap : StreamWrap {
 public:
  uv_stream_t* GetStream() {
    return reinterpret_cast<uv_stream_t*>(&handle_);
  }

  static void InitializeTemplate(Handle<FunctionTemplate> tpl) {
    StreamWrap::InitializeTemplate(tpl);

    NODE_SET_PROTOTYPE_METHOD(tpl, "getWindowSize", TTYWrap::GetWindowSize);
    NODE_SET_PROTOTYPE_METHOD(tpl, "setRawMode", SetRawMode);
  }

  static void Initialize(Handle<Object> target) {
    StreamWrap::Initialize(target);

    NODE_SET_METHOD(target, "isTTY", IsTTY);
    NODE_SET_METHOD(target, "guessHandleType", GuessHandleType);

    target->Set(String::NewSymbol("TTY"), Constructor<TTYWrap>());
  }

 private:
  static Handle<Value> GuessHandleType(const Arguments& args) {
    HandleScope scope;
    int fd = args[0]->Int32Value();
    assert(fd >= 0);

    uv_handle_type t = uv_guess_handle(fd);

    switch (t) {
      case UV_TTY:
        return scope.Close(String::New("TTY"));

      case UV_NAMED_PIPE:
        return scope.Close(String::New("PIPE"));

      case UV_FILE:
        return scope.Close(String::New("FILE"));

      default:
        assert(0);
        return v8::Undefined();
    }
  }

  static Handle<Value> IsTTY(const Arguments& args) {
    HandleScope scope;
    int fd = args[0]->Int32Value();
    assert(fd >= 0);
    return uv_guess_handle(fd) == UV_TTY ? v8::True() : v8::False();
  }

  static Handle<Value> GetWindowSize(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int width, height;
    int r = uv_tty_get_winsize(&wrap->handle_, &width, &height);

    if (r) {
      SetErrno(uv_last_error(uv_default_loop()));
      return v8::Undefined();
    }

    Local<v8::Array> a = v8::Array::New(2);
    a->Set(0, Integer::New(width));
    a->Set(1, Integer::New(height));

    return scope.Close(a);
  }

  static Handle<Value> SetRawMode(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_tty_set_mode(&wrap->handle_, args[0]->IsTrue());

    if (r) {
      SetErrno(uv_last_error(uv_default_loop()));
    }

    return scope.Close(Integer::New(r));
  }

  friend class Wrap;
  template <class T>
  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    int fd = args[0]->Int32Value();
    assert(fd >= 0);

    TTYWrap* wrap = new TTYWrap(args.This(), fd, args[1]->IsTrue());
    assert(wrap);
    wrap->UpdateWriteQueueSize();

    return scope.Close(args.This());
  }
  
  TTYWrap(Handle<Object> object, int fd, bool readable)
      : StreamWrap(object) {
    uv_tty_init(uv_default_loop(), &handle_, fd, readable);
    handle_.data = this;
  }

  uv_tty_t handle_;
};

}  // namespace node

NODE_MODULE(node_tty_wrap, node::TTYWrap::Initialize)
