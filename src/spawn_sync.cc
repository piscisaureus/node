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

#include "node.h"
#include "node_buffer.h"
#include "string_bytes.h"

#include <string.h>
#include <stdlib.h>


namespace node {

using v8::Array;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;


class SyncStdioBuffer;
class SyncStdioPipeHandler;
class SpawnSyncHelper;


class SyncStdioBuffer {
  static const unsigned int kBufferSize = 65536;

 public:
  inline SyncStdioBuffer();

  inline uv_buf_t OnAlloc(size_t suggested_size) const;
  inline void OnRead(uv_buf_t buf, size_t nread);
  
  inline size_t Copy(char* dest) const;

  inline unsigned int available() const;
  inline unsigned int used() const;

  inline SyncStdioBuffer* next() const;
  inline void set_next(SyncStdioBuffer* next);

 private:
  // use unsigned int because that's what uv_buf_init takes.
  mutable char data_[kBufferSize];
  unsigned int used_;

  SyncStdioBuffer* next_;
};


class SyncStdioPipeHandler {
 enum pipe_handler_lifecycle {
   kUninitialized = 0,
   kInitialized,
   kStarted,
   kClosing,
   kClosed
 };

 public:
  SyncStdioPipeHandler(SpawnSyncHelper* process_handler, bool readable, bool writable, uv_buf_t input_buffer);
  ~SyncStdioPipeHandler();

  int Initialize(uv_loop_t* loop);
  int Start();
  void Close();

  Local<Object> GetOutputAsBuffer() const;

  inline bool readable() const;
  inline bool writable() const;
  inline ::uv_stdio_flags uv_stdio_flags() const;

  inline uv_pipe_t* uv_pipe() const;
  inline uv_stream_t* uv_stream() const;
  inline uv_handle_t* uv_handle() const;

 private:
  inline size_t OutputLength() const;
  inline size_t CopyOutput(char* dest) const;

  inline uv_buf_t OnAlloc(size_t suggested_size);
  inline void OnRead(uv_buf_t buf, ssize_t nread);
  inline void OnWriteDone(int result);
  inline void OnShutdownDone(int result);
  inline void OnClose();

  inline void SetError(int error);

  static uv_buf_t AllocCallback(uv_handle_t* handle, size_t suggested_size);
  static void ReadCallback(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void WriteCallback(uv_write_t* req, int result);
  static void ShutdownCallback(uv_shutdown_t* req, int result);
  static void CloseCallback(uv_handle_t* handle);
  
  SpawnSyncHelper* process_handler_;

  bool readable_;
  bool writable_;
  uv_buf_t input_buffer_;
  
  SyncStdioBuffer* first_buffer_;
  SyncStdioBuffer* last_buffer_;

  mutable uv_pipe_t uv_pipe_;
  uv_write_t write_req_;
  uv_shutdown_t shutdown_req_;

  pipe_handler_lifecycle lifecycle_;
};


class SpawnSyncHelper {
  enum sync_process_lifecycle {
    kUninitialized = 0,
    kInitialized,
    kHandlesClosed
  };

 public:
  static void Initialize(Handle<Object> target);
  static void Spawn(const FunctionCallbackInfo<Value>& args);

 private:
  SpawnSyncHelper();
  ~SpawnSyncHelper();

  Local<Object> DoSpawn(Local<Value> options);
  void TryInitializeAndSpawn(Local<Value> options);
  void CloseHandles();
  
  void OnExit(int64_t exit_code, int term_sig);
  void OnTimeout(int status);

  void Kill();
  void StopTimer();
  void IncrementBufferSizeAndCheckOverflow(ssize_t length);
  
  Local<Object> BuildResultsObject();
  Local<Array> BuildOutputObject();

  int GetError();
  void SetError(int error);
  void SetPipeError(int pipe_error);
  
  int ParseOptions(Local<Value> js_value);
  int ParseStdioOptions(Local<Value> js_value);
  int ParseStdioOption(int child_fd, Local<Object> js_stdio_option);
  
  inline int AddStdioIgnore(uint32_t child_fd);
  inline int AddStdioPipe(uint32_t child_fd, bool readable, bool writable, uv_buf_t input_buffer);
  inline int AddStdioInheritFD(uint32_t child_fd, int inherit_fd);

  static bool IsSet(Local<Value> value);
  template <typename t> static bool CheckRange(Local<Value> js_value);
  static int CopyJsString(Local<Value> js_value, char*& target);
  static int CopyJsStringArray(Local<Value> js_value, char*& target);
  
  static void ExitCallback(uv_process_t* handle, int64_t exit_code, int term_sig);
  static void TimeoutCallback(uv_timer_t* handle, int status);
  static void TimerCloseCallback(uv_handle_t* handle);
   
 private:
  size_t max_buffer_;
  uint64_t timeout_;
  int kill_signal_;

  uv_loop_t* uv_loop_;

  uint32_t stdio_count_;
  uv_stdio_container_t* uv_stdio_containers_;
  SyncStdioPipeHandler** pipe_handlers_;

  uv_process_options_t uv_process_options_;
  char* file_buffer_;
  char* args_buffer_;
  char* env_buffer_;
  char* cwd_buffer_;

  uv_process_t uv_process;
  bool uv_process_killed_;

  size_t buffered_data_;
  int64_t exit_code_;
  int term_sig_;

  uv_timer_t uv_timer;
  bool uv_timer_initialized_;

  // Errors that happen in one of the pipe handlers are stored in the
  // `pipe_error` field. They are treated as "low-priority", only to be
  // reported if no more serious errors happened.
  int error_;
  int pipe_error_;

  sync_process_lifecycle lifecycle_;
};


SyncStdioBuffer::SyncStdioBuffer(): 
  used_(0), 
  next_(NULL) {
}

uv_buf_t SyncStdioBuffer::OnAlloc(size_t suggested_size) const {
  if (used() == kBufferSize)
    return uv_buf_init(NULL, 0);

  return uv_buf_init(data_ + used(), available());
}

void SyncStdioBuffer::OnRead(uv_buf_t buf, size_t nread) {
  // If we hand out the same chunk twice, this should catch it.
  assert(buf.base == data_ + used());
  used_ += static_cast<unsigned int>(nread);
}

inline size_t SyncStdioBuffer::Copy(char* dest) const {
  memcpy(dest, data_, used());
  return used();
}

unsigned int SyncStdioBuffer::available() const { 
  return sizeof data_ - used(); 
}

unsigned int SyncStdioBuffer::used() const { 
  return used_; 
}

SyncStdioBuffer* SyncStdioBuffer::next() const { 
  return next_; 
}

void SyncStdioBuffer::set_next(SyncStdioBuffer* next) { 
  next_ = next; 
}


SyncStdioPipeHandler::SyncStdioPipeHandler(SpawnSyncHelper* process_handler, bool readable, bool writable, uv_buf_t input_buffer):
    process_handler_(process_handler),
    readable_(readable),
    writable_(writable),
    input_buffer_(input_buffer),
    first_buffer_(NULL),
    last_buffer_(NULL),
    lifecycle_(kUninitialized) {
  assert(readable || writable);
}

SyncStdioPipeHandler::~SyncStdioPipeHandler() {
  assert(lifecycle_ == kUninitialized || lifecycle_ == kClosed);

  SyncStdioBuffer* buf;
  SyncStdioBuffer* next;

  for (buf = first_buffer_; buf != NULL; buf = next) {
    next = buf->next();
    delete buf;
  }
}

int SyncStdioPipeHandler::Initialize(uv_loop_t* loop) {
  assert(lifecycle_ == kUninitialized);

  int r = uv_pipe_init(loop, uv_pipe(), 0);
  if (r < 0)
    return r;

  uv_pipe()->data = this;

  lifecycle_ = kInitialized;
  return 0;
}

int SyncStdioPipeHandler::Start() {
  assert(lifecycle_ == kInitialized);

  // Set the busy flag already. If this function fails no recovery is
  // possible.
  lifecycle_ = kStarted;

  if (readable()) {
    if (input_buffer_.len > 0) {
      assert(input_buffer_.base != NULL);

      int r = uv_write(&write_req_, uv_stream(), &input_buffer_, 1, WriteCallback);
      if (r < 0)
        return r;
    }

    int r = uv_shutdown(&shutdown_req_, uv_stream(), ShutdownCallback);
    if (r < 0)
      return r;
  }

  if (writable()) {
    int r = uv_read_start(uv_stream(), AllocCallback, ReadCallback);
    if (r < 0)
      return r;
  }

  return 0;
}

void SyncStdioPipeHandler::Close() {
  assert(lifecycle_ == kInitialized || lifecycle_ == kStarted);

  uv_close(uv_handle(), CloseCallback);

  lifecycle_ = kClosing;
}

Local<Object> SyncStdioPipeHandler::GetOutputAsBuffer() const {
  size_t length = OutputLength();
  Local<Object> js_buffer = Buffer::New(length);
  CopyOutput(Buffer::Data(js_buffer));
  return js_buffer;
}

bool SyncStdioPipeHandler::readable() const {
  return readable_;
}

bool SyncStdioPipeHandler::writable() const {
  return writable_;
}

uv_stdio_flags SyncStdioPipeHandler::uv_stdio_flags() const {
  unsigned int flags;

  flags = UV_CREATE_PIPE;
  if (readable())
    flags |= UV_READABLE_PIPE;
  if (writable())
    flags |= UV_WRITABLE_PIPE;

  return static_cast<::uv_stdio_flags>(flags);
}

uv_pipe_t* SyncStdioPipeHandler::uv_pipe() const {
  assert(lifecycle_ < kClosing);
  return &uv_pipe_;
}

uv_stream_t* SyncStdioPipeHandler::uv_stream() const {
  return reinterpret_cast<uv_stream_t*>(uv_pipe());
}

inline uv_handle_t* SyncStdioPipeHandler::uv_handle() const {
  return reinterpret_cast<uv_handle_t*>(uv_pipe());
}

size_t SyncStdioPipeHandler::OutputLength() const {
  size_t size = 0;
  for (SyncStdioBuffer* buf = first_buffer_; buf != NULL; buf = buf->next())
    size += buf->used();

  return size;
}

size_t SyncStdioPipeHandler::CopyOutput(char* dest) const {
  size_t offset = 0;
  for (SyncStdioBuffer* buf = first_buffer_; buf != NULL; buf = buf->next())
    offset += buf->Copy(dest + offset);

  return offset;
}

uv_buf_t SyncStdioPipeHandler::OnAlloc(size_t suggested_size) {
  // This function that libuv will never allocate two buffers for the same
  // stream at the same time. There's an assert in SyncStdioBuffer::OnRead that
  // would fail if this assumption was violated.

  if (last_buffer_ == NULL) {
    // Allocate the first capture buffer.
    first_buffer_ = last_buffer_ = new SyncStdioBuffer();

  } else if (last_buffer_->available() == 0) {
    // The current capture buffer is full so get us a new one.
    SyncStdioBuffer* buf = new SyncStdioBuffer();
    last_buffer_->set_next(buf);
    last_buffer_ = buf;
  }

  return last_buffer_->OnAlloc(suggested_size);
}

void SyncStdioPipeHandler::OnRead(uv_buf_t buf, ssize_t nread) {
  fprintf(stderr, "%d\n", (int) nread);

  if (nread == UV_EOF) {
    // Libuv implicitly stops reading on EOF.

  } else if (nread < 0) {
    SetError(static_cast<int>(nread));
    // At some point libuv should really implicitly stop reading on error.
    uv_read_stop(uv_stream());

  } else {
    last_buffer_->OnRead(buf, nread);
    process_handler_->IncrementBufferSizeAndCheckOverflow(nread);
  }
}

void SyncStdioPipeHandler::OnWriteDone(int result) {
  if (result < 0)
    SetError(result);
}

void SyncStdioPipeHandler::OnShutdownDone(int result) {
  if (result < 0)
    SetError(result);
}

void SyncStdioPipeHandler::OnClose() {
  lifecycle_ = kClosed;
}

void SyncStdioPipeHandler::SetError(int error) {
  assert(error != 0);
  process_handler_->SetPipeError(error);
}

uv_buf_t SyncStdioPipeHandler::AllocCallback(uv_handle_t* handle, size_t suggested_size) {
  SyncStdioPipeHandler* self = reinterpret_cast<SyncStdioPipeHandler*>(handle->data);
  return self->OnAlloc(suggested_size);
}

void SyncStdioPipeHandler::ReadCallback(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  SyncStdioPipeHandler* self = reinterpret_cast<SyncStdioPipeHandler*>(stream->data);
  self->OnRead(buf, nread);
}

void SyncStdioPipeHandler::WriteCallback(uv_write_t* req, int result) {
  SyncStdioPipeHandler* self = reinterpret_cast<SyncStdioPipeHandler*>(req->handle->data);
  self->OnWriteDone(result);
}

void SyncStdioPipeHandler::ShutdownCallback(uv_shutdown_t* req, int result) {
  SyncStdioPipeHandler* self = reinterpret_cast<SyncStdioPipeHandler*>(req->handle->data);
  self->OnShutdownDone(result);
}

void SyncStdioPipeHandler::CloseCallback(uv_handle_t* handle) {
  SyncStdioPipeHandler* self = reinterpret_cast<SyncStdioPipeHandler*>(handle->data);
  self->OnClose();
}




void SpawnSyncHelper::Initialize(Handle<Object> target) {
  NODE_SET_METHOD(target, "spawnSync", Spawn);
}

void SpawnSyncHelper::Spawn(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  SpawnSyncHelper p;
  Local<Value> result = p.DoSpawn(args[0]);
  args.GetReturnValue().Set(result);
}

SpawnSyncHelper():
      error_(0),
      pipe_error_(0),
      exit_code_(-1),
      term_sig_(-1),
      max_buffer_(0),
      buffered_data_(0),
      timeout_(0),
      kill_signal_(SIGTERM),
      stdio_count_(0),
      pipe_handlers_(NULL),
      file_buffer_(NULL),
      args_buffer_(NULL),
      cwd_buffer_(NULL),
      env_buffer_(NULL),
      uv_stdio_containers_(NULL),
      uv_loop_(NULL),
      uv_process_options_(),
      uv_timer_initialized_(false),
      uv_process_killed_(false),
      lifecycle_(kUninitialized)
   {
   }

~SpawnSyncHelper() {
  assert(lifecycle_ == kHandlesClosed);

  if (pipe_handlers_ != NULL) {
    for (size_t i = 0; i < stdio_count_; i++) {
      if (pipe_handlers_[i] != NULL)
        delete pipe_handlers_[i];
    }
  }

  delete[] pipe_handlers_;
  delete[] file_buffer_;
  delete[] args_buffer_;
  delete[] cwd_buffer_;
  delete[] env_buffer_;
  delete[] uv_stdio_containers_;
}

Local<Object> DoSpawn(Local<Value> options) {
  assert(lifecycle_ == kUninitialized);

  HandleScope scope;

  TryInitializeAndSpawn(options);
  CloseHandles();

  Local<Object> result = BuildResultsObject();

  return scope.Close(result);
}

  void TryInitializeAndSpawn(Local<Value> options) {
    int r;

    // There is no recovery from failure inside TryInitializeAndSpawn.
    assert(lifecycle_ == kUninitialized);
    lifecycle_ = kInitialized;

    uv_loop_ = uv_loop_new();
    if (uv_loop_ == NULL)
      return SetError(UV_ENOMEM);

    r = ParseOptions(options);
    if (r < 0)
      return SetError(r);

    if (timeout_ > 0) {
      r = uv_timer_init(uv_loop_, &uv_timer);
      if (r < 0)
        return SetError(r);

      uv_unref(reinterpret_cast<uv_handle_t*>(&uv_timer));

      uv_timer.data = this;
      uv_timer_initialized_ = true;

      // Start the timer immediately. If uv_spawn fails then CloseHandles()
      // will immediately close the timer handle which implicitly stops it,
      // so there is no risk that the timeout callback runs when the process
      // didn't start.
      r = uv_timer_start(&uv_timer, TimeoutCallback, timeout_, 0);
      if (r < 0)
        return SetError(r);
    }

    uv_process_options_.exit_cb = ExitCallback;
    r = uv_spawn(uv_loop_, &uv_process, uv_process_options_);
    if (r < 0)
      return SetError(r);
    uv_process.data = this;

    for (uint32_t i = 0; i < stdio_count_; i++) {
      SyncStdioPipeHandler* h = pipe_handlers_[i];
      if (h != NULL) {
        r = h->Start();
        if (r < 0)
          return SetError(r);
      }
    }


    r = uv_DoSpawn(uv_loop_, UV_RUN_DEFAULT);
    if (r < 0)
      // We can't handle uv_run failure.
      abort();
  }


  void CloseHandles() {
    assert(lifecycle_ < kHandlesClosed);
    lifecycle_ = kHandlesClosed;

    if (pipe_handlers_ != NULL) {
      assert(uv_loop_ != NULL);
      for (uint32_t i = 0; i < stdio_count_; i++) {
        if (pipe_handlers_[i] != NULL)
          pipe_handlers_[i]->Close();
      }
    }

    if (uv_timer_initialized_) {
      uv_handle_t* uv_timer_handle = reinterpret_cast<uv_handle_t*>(&uv_timer);
      uv_ref(uv_timer_handle);
      uv_close(uv_timer_handle, TimerCloseCallback);
    }

    if (uv_loop_ != NULL) {
      // Give closing watchers a chance to finish closing and get their close callbacks called.
      int r = uv_DoSpawn(uv_loop_, UV_RUN_DEFAULT);
      if (r < 0)
        abort();

      uv_loop_delete(uv_loop_);
    }
  }

  
  void OnExit(int64_t exit_code, int term_sig) {
    if (exit_code < 0)
      return SetError(static_cast<int>(exit_code));

    exit_code_ = exit_code;
    term_sig_ = term_sig;

    // Stop the timeout timer if it is running.
    StopTimer();
  }

  
  void OnTimeout(int status) {
    assert(status == 0);
    SetError(UV_ETIMEDOUT);
    Kill();
  }

  
  void Kill() {
    int r;

    // Only attempt to kill once.
    if (uv_process_killed_)
      return;
    uv_process_killed_ = true;

    // When the timer fires, kill the process we've spawned.
    r = uv_process_kill(&uv_process, kill_signal_);

    // If uv_kill failed with an error that isn't ESRCH, the user probably
    // specified an invalid or unsupported signal. Signal this to the user as
    // and error and kill the process with SIGKILL instead.
    if (r < 0 && r != UV_ESRCH) {
      SetError(r);

      r = uv_process_kill(&uv_process, kill_signal_);
      assert(r >= 0 || r == UV_ESRCH);
    }

    // Stop the timeout timer if it is running.
    StopTimer();
  }

  void StopTimer() {
    assert((timeout_ > 0) == uv_timer_initialized_);
    if (uv_timer_initialized_) {
      int r = uv_timer_stop(&uv_timer);
      assert(r == 0);
    }
  }

  
  void IncrementBufferSizeAndCheckOverflow(ssize_t length) {
    buffered_data_ += length;
    if (max_buffer_ > 0 && buffered_data_ > max_buffer_)
      Kill();
  }

  Local<Object> BuildResultsObject() {
    HandleScope scope(node_isolate);

    Local<String> error_key = FIXED_ONE_BYTE_STRING(node_isolate, "error");
    Local<String> status_key = FIXED_ONE_BYTE_STRING(node_isolate, "status");
    Local<String> signal_key = FIXED_ONE_BYTE_STRING(node_isolate, "signal");
    Local<String> output_key = FIXED_ONE_BYTE_STRING(node_isolate, "output");

    Local<Object> js_result = Object::New();

    if (GetError() != 0)
      js_result->Set(error_key, Integer::New(GetError()));

    if (exit_code_ >= 0) {
      js_result->Set(status_key, Number::New(node_isolate, static_cast<double>(exit_code_)));

    } else
      // If exit_code_ < 0 the process was never started because of some error.
      js_result->Set(status_key, Null());

    if (term_sig_ > 0)
      js_result->Set(signal_key, String::NewFromUtf8(node_isolate, signo_string(term_sig_)));
    else
      js_result->Set(signal_key, Null());

    if (exit_code_ >= 0)
      js_result->Set(output_key, BuildOutputObject());
    else
      js_result->Set(output_key, Null());

    return scope.Close(js_result);
  }

  Local<Array> BuildOutputObject() {
    assert(lifecycle_ >= kInitialized);
    assert(pipe_handlers_ != NULL);

    HandleScope scope(node_isolate);
    Local<Array> js_output = Array::New(stdio_count_);

    for (uint32_t i = 0; i < stdio_count_; i++) {
      SyncStdioPipeHandler* h = pipe_handlers_[i];
      if (h != NULL && h->writable())
        js_output->Set(i, h->GetOutputAsBuffer());
      else
        js_output->Set(i, Null());
    }

    return scope.Close(js_output);
  }

  void SetError(int error) {
    if (error_ == 0)
      error_ = error;
  }

  void SetPipeError(int pipe_error) {
    if (pipe_error_ == 0)
      pipe_error_ = pipe_error;
  }

  int GetError() {
    if (error_ != 0)
      return error_;
    else
      return pipe_error_;
  }
  
  int ParseOptions(Local<Value> js_value) {
    HandleScope scope(node_isolate);
    int r;

    Local<String> file_sym = FIXED_ONE_BYTE_STRING(node_isolate, "file");
    Local<String> args_sym = FIXED_ONE_BYTE_STRING(node_isolate, "args");
    Local<String> cwd_sym = FIXED_ONE_BYTE_STRING(node_isolate, "cwd");
    Local<String> envPairs_sym = FIXED_ONE_BYTE_STRING(node_isolate, "envPairs");
    Local<String> uid_sym = FIXED_ONE_BYTE_STRING(node_isolate, "uid");
    Local<String> gid_sym = FIXED_ONE_BYTE_STRING(node_isolate, "gid");
    Local<String> detached_sym = FIXED_ONE_BYTE_STRING(node_isolate, "detached");
    Local<String> windowsVerbatimArguments_sym = FIXED_ONE_BYTE_STRING(node_isolate, "windowsVerbatimArguments");
    Local<String> stdio_sym = FIXED_ONE_BYTE_STRING(node_isolate, "stdio");
    Local<String> timeout_sym = FIXED_ONE_BYTE_STRING(node_isolate, "timeout");
    Local<String> maxBuffer_sym = FIXED_ONE_BYTE_STRING(node_isolate, "maxBuffer");
    Local<String> killSignal_sym = FIXED_ONE_BYTE_STRING(node_isolate, "killSignal");

    if (!js_value->IsObject())
      return UV_EINVAL;

    Local<Object> js_options = js_value.As<Object>();

    Local<Value> js_file = js_options->Get(file_sym);
    if (r = CopyJsString(js_file, file_buffer_) < 0)
      return r;
    uv_process_options_.file = file_buffer_;

    Local<Value> js_args = js_options->Get(args_sym);
    if (r = CopyJsStringArray(js_args, args_buffer_) < 0)
      return r;
    uv_process_options_.args = reinterpret_cast<char**>(args_buffer_);


    Local<Value> js_cwd = js_options->Get(cwd_sym);
    if (IsSet(js_cwd)) {
      if (r = CopyJsString(js_cwd, uv_process_options_.cwd) < 0)
        return r;
      uv_process_options_.cwd = cwd_buffer_;
    }

    Local<Value> js_env_pairs = js_options->Get(envPairs_sym);
    if (IsSet(js_env_pairs)) {
      if (r = CopyJsStringArray(js_env_pairs, env_buffer_) < 0)
        return r;
      uv_process_options_.args = reinterpret_cast<char**>(env_buffer_);
    }

    Local<Value> js_uid = js_options->Get(uid_sym);
    if (IsSet(js_uid)) {
      if (!CheckRange<uv_uid_t>(js_uid))
        return UV_EINVAL;
      uv_process_options_.uid = static_cast<uv_gid_t>(js_uid->Int32Value());
      uv_process_options_.flags |= UV_PROCESS_SETUID;
    }

    Local<Value> js_gid = js_options->Get(gid_sym);
    if (IsSet(js_gid)) {
      if (!CheckRange<uv_gid_t>(js_gid))
        return UV_EINVAL;
      uv_process_options_.gid = static_cast<uv_gid_t>(js_gid->Int32Value());
      uv_process_options_.flags |= UV_PROCESS_SETGID;
    }

    if (js_options->Get(detached_sym)->BooleanValue())
      uv_process_options_.flags |= UV_PROCESS_DETACHED;

    if (js_options->Get(windowsVerbatimArguments_sym)->BooleanValue())
      uv_process_options_.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;

    Local<Value> js_timeout = js_options->Get(timeout_sym);
    if (IsSet(js_timeout)) {
      if (!js_timeout->IsNumber())
        return UV_EINVAL;
      int64_t timeout = js_timeout->IntegerValue();
      if (timeout < 0)
        return UV_EINVAL;
      timeout_ = static_cast<uint64_t>(timeout);
    }

    Local<Value> js_max_buffer = js_options->Get(maxBuffer_sym);
    if (IsSet(js_max_buffer)) {
      if (!CheckRange<uint32_t>(js_max_buffer))
        return UV_EINVAL;
      max_buffer_ = js_max_buffer->Uint32Value();
    }

    Local<Value> js_kill_signal = js_options->Get(killSignal_sym);
    if (IsSet(js_kill_signal)) {
      if (!js_kill_signal->IsInt32())
        return UV_EINVAL;
      int kill_signal_ = js_kill_signal->Int32Value();
      if (kill_signal_ == 0)
        return UV_EINVAL;
    }

    Local<Value> js_stdio = js_options->Get(stdio_sym);
    if (r = ParseStdioOptions(js_stdio) < 0)
      return r;

    return 0;
  }

  int ParseStdioOptions(Local<Value> js_value) {
    HandleScope scope;
    Local<Array> js_stdio_options;

    if (!js_value->IsArray())
      return UV_EINVAL;

    js_stdio_options = js_value.As<Array>();

    stdio_count_ = js_stdio_options->Length();

    pipe_handlers_ = new SyncStdioPipeHandler*[stdio_count_]();
    uv_stdio_containers_ = new uv_stdio_container_t[stdio_count_];

    for (uint32_t i = 0; i < stdio_count_; i++) {
      Local<Value> js_stdio_option = js_stdio_options->Get(i);

      if (!js_stdio_option->IsObject())
        return UV_EINVAL;

      int r = ParseStdioOption(i, js_stdio_option.As<Object>());
      if (r < 0)
        return r;
    }

    uv_process_options_.stdio = uv_stdio_containers_;
    uv_process_options_.stdio_count = stdio_count_;

    return 0;
  }

  int ParseStdioOption(int child_fd, Local<Object> js_stdio_option) {
    Local<String> type_sym = FIXED_ONE_BYTE_STRING(node_isolate, "type");
    Local<String> ignore_sym = FIXED_ONE_BYTE_STRING(node_isolate, "ignore");
    Local<String> pipe_sym = FIXED_ONE_BYTE_STRING(node_isolate, "pipe");
    Local<String> inherit_sym = FIXED_ONE_BYTE_STRING(node_isolate, "inherit");
    Local<String> readable_sym = FIXED_ONE_BYTE_STRING(node_isolate, "readable");
    Local<String> writable_sym = FIXED_ONE_BYTE_STRING(node_isolate, "writable");
    Local<String> input_sym = FIXED_ONE_BYTE_STRING(node_isolate, "input");
    Local<String> fd_sym = FIXED_ONE_BYTE_STRING(node_isolate, "fd");

    Local<Value> js_type = js_stdio_option->Get(type_sym);

    if (js_type->StrictEquals(ignore_sym)) {
      return AddStdioIgnore(child_fd);

    } else if (js_type->StrictEquals(pipe_sym)) {
      bool readable = js_stdio_option->Get(readable_sym)->BooleanValue();
      bool writable = js_stdio_option->Get(writable_sym)->BooleanValue();

      uv_buf_t buf = uv_buf_init(NULL, 0);

      if (readable) {
        Local<Value> input = js_stdio_option->Get(input_sym);
        if (!Buffer::HasInstance(input))
          // We can only deal with buffers for now.
          assert(input->IsUndefined());
        else
          buf = uv_buf_init(Buffer::Data(input),
                            static_cast<unsigned int>(Buffer::Length(input)));
      }

      return AddStdioPipe(child_fd, readable, writable, buf);

    } else if (js_type->StrictEquals(fd_sym)) {
      int inherit_fd = js_stdio_option->Get(fd_sym)->Int32Value();
      return AddStdioInheritFD(child_fd, inherit_fd);

    } else {
      assert(0 && "invalid child stdio type");
      return UV_EINVAL;
    }
  }

  
  inline int AddStdioIgnore(uint32_t child_fd) {
    assert(child_fd < stdio_count_);
    assert(pipe_handlers_[child_fd] == NULL);

    uv_stdio_containers_[child_fd].flags = UV_IGNORE;

    return 0;
  }

  inline int AddStdioPipe(uint32_t child_fd, bool readable, bool writable, uv_buf_t input_buffer) {
    assert(child_fd < stdio_count_);
    assert(pipe_handlers_[child_fd] == NULL);

    SyncStdioPipeHandler* h = new SyncStdioPipeHandler(this, readable, writable, input_buffer);

    int r = h->Initialize(uv_loop_);
    if (r < 0) {
      delete h;
      return r;
    }

    pipe_handlers_[child_fd] = h;

    uv_stdio_containers_[child_fd].flags = h->uv_stdio_flags();
    uv_stdio_containers_[child_fd].data.stream = h->uv_stream();

    return 0;
  }

  inline int AddStdioInheritFD(uint32_t child_fd, int inherit_fd) {
    assert(child_fd < stdio_count_);
    assert(pipe_handlers_[child_fd] == NULL);

    uv_stdio_containers_[child_fd].flags = UV_INHERIT_FD;
    uv_stdio_containers_[child_fd].data.fd = inherit_fd;

    return 0;
  }

  static bool IsSet(Local<Value> value) {
    return !value->IsUndefined() && !value->IsNull();
  }

  template <typename t>
  static bool CheckRange(Local<Value> js_value) {
    if ((t) -1 > 0) {
      // Unsigned range check.
      if (!js_value->IsUint32())
        return false;
      if (js_value->Uint32Value() & ~((t) ~0))
        return false;
    } else {
      // Unsigned range check.
      if (!js_value->IsUint32())
        return false;
      if (js_value->Uint32Value() & ~((t) ~0))
        return false;
    }
    return true;
  }

  static int CopyJsString(Local<Value> js_value, char*& target) {
    Local<String> js_string;
    size_t size, written;
    char* buffer;

    if (js_value->IsString())
      js_string = js_value.As<String>();
    else
      js_string = js_value->ToString();

    // Include space for null terminator byte.
    size = StringBytes::StorageSize(js_string, UTF8) + 1;

    buffer = new char[size];

    written = StringBytes::Write(buffer, -1, js_string, UTF8);
    buffer[written] = '\0';

    target = buffer;
    return 0;
  }

  static int CopyJsStringArray(Local<Value> js_value, char*& target) {
    Local<Array> js_array;
    uint32_t length;
    size_t list_size, data_size, data_offset;
    char** list;
    char* buffer;

    if (!js_value->IsArray())
      return UV_EINVAL;

    js_array = js_value.As<Array>()->Clone().As<Array>();
    length = js_array->Length();

    // Convert all array elements to string. Modify the js object itself if
    // needed - it's okay since we cloned the original object.
    for (uint32_t i = 0; i < length; i++) {
      if (!js_array->Get(i)->IsString())
        js_array->Set(i, js_array->Get(i)->ToString());
    }

    // Index has a pointer to every string element, plus one more for a final
    // null pointer.
    list_size = (length + 1) * sizeof *list;

    // Compute the length of all strings. Include room for null terminator
    // after every string. Align strings to cache lines.
    data_size = 0;
    for (uint32_t i = 0; i < length; i++) {
      data_size += StringBytes::StorageSize(js_array->Get(i), UTF8) + 1;
      data_size = ROUND_UP(data_size, sizeof(void*));
    }

    buffer = new char[list_size + data_size];

    list = reinterpret_cast<char**>(buffer);
    data_offset = list_size;

    for (uint32_t i = 0; i < length; i++) {
      list[i] = buffer + data_offset;
      data_offset += StringBytes::Write(buffer + data_offset,
                                        -1,
                                        js_array->Get(i),
                                        UTF8);
      buffer[data_offset++] = '\0';
      data_offset = ROUND_UP(data_offset, sizeof(void*));
    }

    list[length] = NULL;

    target = buffer;
    return 0;
  }

static void ExitCallback(uv_process_t* handle, int64_t exit_code, int term_sig) {
    SpawnSyncHelper* self = reinterpret_cast<SpawnSyncHelper*>(handle->data);
    self->OnExit(exit_code, term_sig);
  }

  static void TimeoutCallback(uv_timer_t* handle, int status) {
    SpawnSyncHelper* self = reinterpret_cast<SpawnSyncHelper*>(handle->data);
    self->OnTimeout(status);
  }

  static void TimerCloseCallback(uv_handle_t* handle) {
    // No-op.
  }
  

  
  

  






}  // namespace node

NODE_MODULE(node_spawn_sync, node::SpawnSyncHelper::Initialize)
