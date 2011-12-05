
#ifndef _WRAP_H
#define _WRAP_H

#include <assert.h>
#include <node.h>
#include <v8.h>


namespace node {
using namespace v8;

template <class T>
class WrapData {
 public:
  static Persistent<Function> ctor;
};

template <class T> Persistent<Function> WrapData<T>::ctor;


class Wrap {
 public:
  template <class T>
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    T* wrap = new T(args.This());
    assert(wrap);

    return scope.Close(args.This());
  }

 public:
  template <typename T>
  static T* Unwrap(Handle<Object> object) {
    assert(!object.IsEmpty());
    assert(object->InternalFieldCount() > 0);
    T* native_object = static_cast<T*>(object->GetPointerFromInternalField(0));
    assert(native_object != NULL);
    return native_object;
  }
   
  template <typename T> 
  static T* Unwrap(const Arguments& args) {
    return Unwrap<T>(args.Holder());
  }

  Wrap(Handle<Object> object) {
    object_ = Persistent<Object>::New(object);
    object_->SetPointerInInternalField(0, this);
  }
  
  Wrap(const Arguments& args) {
    assert(args.IsConstructCall());
    Wrap(args.Holder());
  }

  void DisposeWrap() {
    assert(!object_.IsEmpty());
    assert(object_->InternalFieldCount() > 0);
    assert(object_->GetPointerFromInternalField(0) != NULL);

    object_->SetPointerInInternalField(0, NULL);
    object_.Dispose();
    object_.Clear();
  }

  virtual ~Wrap() {
    if (!object_.IsEmpty()) {
      DisposeWrap();
    }
  }

  static void InitializeTemplate(Handle<FunctionTemplate> t) {
    t->SetClassName(String::NewSymbol("Wrap"));
    t->InstanceTemplate()->SetInternalFieldCount(1);
  }

  template <class T>
  static Handle<Function> const Constructor(InvocationCallback callback) {
    if (WrapData<T>::ctor.IsEmpty()) {
      HandleScope scope;
      Local<FunctionTemplate> templ = FunctionTemplate::New(callback);
      T::InitializeTemplate(templ);
      WrapData<T>::ctor = Persistent<Function>::New(templ->GetFunction());
    }
    return WrapData<T>::ctor;
  }

  template <class T>
  static Handle<Function> const Constructor() {
    InvocationCallback cb = T::New<T>;
    return Constructor<T>(cb);
  }

  static void Initialize(Handle<Object> target) {
  }

 public:
  Handle<Object> GetObject() {
    return object_;
  }

  template <class T>
  static Local<Object> Instantiate() {
    return Constructor<T>()->NewInstance();    
  }
  
 private:
  Wrap() {
  }

 protected:
  Persistent<Object> object_;
};


}  // namespace node

#endif  // _WRAP_H