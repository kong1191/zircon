// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/syscalls.h>
#include <zircon/types.h>
#include <zx/object_traits.h>
#include <zx/time.h>

namespace zx {

class port;

// Wraps and takes ownership of a handle to an object.
//
// Used for code that wants to operate generically on the zx_handle_t value
// inside a |zx::object| and doesn't otherwise need a template parameter.
//
// The handle is automatically closed when the wrapper is destroyed.
class object_base {
public:
    void reset(zx_handle_t value = ZX_HANDLE_INVALID) {
        close();
        value_ = value;
    }

    bool is_valid() const { return value_ != ZX_HANDLE_INVALID; }
    explicit operator bool() const { return is_valid(); }

    zx_handle_t get() const { return value_; }

    // Reset the underlying handle, and then get the address of the
    // underlying internal handle storage.
    //
    // Note: The intended purpose is to facilitate interactions with C
    // APIs which expect to be provided a pointer to a handle used as
    // an out parameter.
    zx_handle_t* reset_and_get_address() {
        reset();
        return &value_;
    }

    __attribute__((warn_unused_result)) zx_handle_t release() {
        zx_handle_t result = value_;
        value_ = ZX_HANDLE_INVALID;
        return result;
    }

protected:
    constexpr object_base() : value_(ZX_HANDLE_INVALID) {}

    explicit object_base(zx_handle_t value) : value_(value) {}

    ~object_base() { close(); }

    object_base(const object_base&) = delete;

    void operator=(const object_base&) = delete;

    void close() {
        if (value_ != ZX_HANDLE_INVALID) {
            zx_handle_close(value_);
            value_ = ZX_HANDLE_INVALID;
        }
    }

    zx_handle_t value_;
};

// Provides type-safe access to operations on a handle.
template <typename T> class object : public object_base {
public:
    constexpr object() : object_base(ZX_HANDLE_INVALID) {}

    explicit object(zx_handle_t value) : object_base(value) {}

    template <typename U> object(object<U>&& other) : object_base(other.release()) {
        static_assert(is_same<T, void>::value, "Receiver must be compatible.");
    }

    template <typename U> object<T>& operator=(object<U>&& other) {
        static_assert(is_same<T, void>::value, "Receiver must be compatible.");
        reset(other.release());
        return *this;
    }

    void swap(object<T>& other) {
        zx_handle_t tmp = value_;
        value_ = other.value_;
        other.value_ = tmp;
    }

    zx_status_t duplicate(zx_rights_t rights, object<T>* result) const {
        static_assert(object_traits<T>::supports_duplication,
                      "Receiver must support duplication.");
        zx_handle_t h = ZX_HANDLE_INVALID;
        zx_status_t status = zx_handle_duplicate(value_, rights, &h);
        result->reset(h);
        return status;
    }

    zx_status_t replace(zx_rights_t rights, object<T>* result) {
        zx_handle_t h = ZX_HANDLE_INVALID;
        zx_status_t status = zx_handle_replace(value_, rights, &h);
        // We store ZX_HANDLE_INVALID to value_ before calling reset on result
        // in case result == this.
        if (status == ZX_OK)
            value_ = ZX_HANDLE_INVALID;
        result->reset(h);
        return status;
    }

    zx_status_t wait_one(zx_signals_t signals, zx::time deadline,
                         zx_signals_t* pending) const {
        return zx_object_wait_one(value_, signals, deadline.get(), pending);
    }

    zx_status_t wait_async(const object<port>& port, uint64_t key,
                           zx_signals_t signals, uint32_t options) const {
        return zx_object_wait_async(value_, port.get(), key, signals, options);
    }

    static zx_status_t wait_many(zx_wait_item_t* wait_items, uint32_t count, zx::time deadline) {
        return zx_object_wait_many(wait_items, count, deadline.get());
    }

    // TODO(abarth): Not all of these methods apply to every type of object. We
    // should sort out which ones apply where and limit them to the interfaces
    // where they work.

    zx_status_t signal(uint32_t clear_mask, uint32_t set_mask) const {
        static_assert(object_traits<T>::supports_user_signal,
                      "Receiver must support user signals.");
        return zx_object_signal(get(), clear_mask, set_mask);
    }

    zx_status_t signal_peer(uint32_t clear_mask, uint32_t set_mask) const {
        static_assert(object_traits<T>::supports_user_signal,
                      "Receiver must support user signals.");
        static_assert(object_traits<T>::has_peer_handle,
                      "Receiver must have peer object.");
        return zx_object_signal_peer(get(), clear_mask, set_mask);
    }

    zx_status_t get_info(uint32_t topic, void* buffer,
                         size_t buffer_size,
                         size_t* actual_count, size_t* avail_count) const {
        return zx_object_get_info(get(), topic, buffer, buffer_size, actual_count, avail_count);
    }

    zx_status_t get_child(uint64_t koid, zx_rights_t rights,
                          object<T>* result) const {
        zx_handle_t h = ZX_HANDLE_INVALID;
        zx_status_t status = zx_object_get_child(value_, koid, rights, &h);
        result->reset(h);
        return status;
    }

    zx_status_t get_property(uint32_t property, void* value,
                             size_t size) const {
        return zx_object_get_property(get(), property, value, size);
    }

    zx_status_t set_property(uint32_t property, const void* value,
                             size_t size) const {
        return zx_object_set_property(get(), property, value, size);
    }

    zx_status_t get_cookie(zx_handle_t scope, uint64_t *cookie) const {
        return zx_object_get_cookie(get(), scope, cookie);
    }

    zx_status_t set_cookie(zx_handle_t scope, uint64_t cookie) const {
        return zx_object_set_cookie(get(), scope, cookie);
    }

private:
    template <typename A, typename B> struct is_same {
        static const bool value = false;
    };

    template <typename A> struct is_same<A, A> {
        static const bool value = true;
    };
};

template <typename T> bool operator==(const object<T>& a, const object<T>& b) {
    return a.get() == b.get();
}

template <typename T> bool operator!=(const object<T>& a, const object<T>& b) {
    return !(a == b);
}

template <typename T> bool operator==(zx_handle_t a, const object<T>& b) {
    return a == b.get();
}

template <typename T> bool operator!=(zx_handle_t a, const object<T>& b) {
    return !(a == b);
}

template <typename T> bool operator==(const object<T>& a, zx_handle_t b) {
    return a.get() == b;
}

template <typename T> bool operator!=(const object<T>& a, zx_handle_t b) {
    return !(a == b);
}

// Wraps a handle to an object to provide type-safe access to its operations
// but does not take ownership of it.  The handle is not closed when the
// wrapper is destroyed.
//
// All instances of unowned<T> must be const.  They cannot be stored, copied,
// or moved but can be passed by reference in the form of a const T& or used
// as a temporary.
//
// void do_something(const zx::event& event);
//
// void example(zx_handle_t event_handle) {
//     do_something(unowned_event::wrap(event_handle));
// }
template <typename T>
class unowned final : public T {
public:
    unowned(unowned&& other) : T(other.release()) {}

    ~unowned() {
        zx_handle_t h = this->release();
        static_cast<void>(h);
    }

    static const unowned wrap(zx_handle_t h) { return unowned(h); }

private:
    friend T;

    explicit unowned(zx_handle_t h) : T(h) {}
};

} // namespace zx