// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was machine generated with:
//
// bash$ fidlc --c-header trace_provider.fidl.h
//   --c-client trace_provider.fidl.client.cpp
//   --tables trace_provider.fidl.tables.cpp
//   --files system/fidl/fuchsia-tracelink/tracelink.fidl
//
// and then this header (up to "End of additions ...") manually inserted.
//   
// There are *no* differences with the machine generated version except the
// addition of this text.
// Minimizing differences with generated code is critical to lessening the
// maintenance burden until we no longer check in machine generated code.
// It is currently checked in to workaround these problems:
// - zircon's build system doesn't support transitive dependencies
// - duplicate copy of tracelink is generated and compiled in garnet,
//   leading to duplicate symbol linker errors

#define fuchsia_tracelink_ProviderStartRequestTable ProviderStartRequestTable
#define fuchsia_tracelink_ProviderStopRequestTable ProviderStopRequestTable
#define fuchsia_tracelink_RegistryRegisterTraceProviderRequestTable RegistryRegisterTraceProviderRequestTable
#define fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyRequestTable RegistryRegisterTraceProviderSynchronouslyRequestTable
#define fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyResponseTable RegistryRegisterTraceProviderSynchronouslyResponseTable

// End of additions to machine-generated file.
// WARNING: This file is machine generated by fidlc.

#pragma once

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <zircon/fidl.h>
#include <zircon/syscalls/object.h>
#include <zircon/types.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Forward declarations

typedef uint8_t fuchsia_tracelink_BufferingMode;
#define fuchsia_tracelink_BufferingMode_ONESHOT UINT8_C(0)
#define fuchsia_tracelink_BufferingMode_CIRCULAR UINT8_C(1)
#define fuchsia_tracelink_BufferingMode_STREAMING UINT8_C(2)

#define fuchsia_tracelink_ProviderStartOrdinal ((uint32_t)0x1)
typedef struct fuchsia_tracelink_ProviderStartRequest fuchsia_tracelink_ProviderStartRequest;
#define fuchsia_tracelink_ProviderStopOrdinal ((uint32_t)0x2)
typedef struct fuchsia_tracelink_ProviderStopRequest fuchsia_tracelink_ProviderStopRequest;
#define fuchsia_tracelink_Registry_Name "fuchsia.tracelink.Registry"
#define fuchsia_tracelink_RegistryRegisterTraceProviderOrdinal ((uint32_t)0x1)
typedef struct fuchsia_tracelink_RegistryRegisterTraceProviderRequest fuchsia_tracelink_RegistryRegisterTraceProviderRequest;
#define fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyOrdinal ((uint32_t)0x2)
typedef struct fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyRequest fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyRequest;
typedef struct fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyResponse fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyResponse;

// Extern declarations

extern const fidl_type_t fuchsia_tracelink_ProviderStartRequestTable;
extern const fidl_type_t fuchsia_tracelink_ProviderStopRequestTable;
extern const fidl_type_t fuchsia_tracelink_RegistryRegisterTraceProviderRequestTable;
extern const fidl_type_t fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyRequestTable;
extern const fidl_type_t fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyResponseTable;

// Declarations

struct fuchsia_tracelink_ProviderStartRequest {
    FIDL_ALIGNDECL
    fidl_message_header_t hdr;
    fuchsia_tracelink_BufferingMode buffering_mode;
    zx_handle_t buffer;
    zx_handle_t fifo;
    fidl_vector_t categories;
};

struct fuchsia_tracelink_ProviderStopRequest {
    FIDL_ALIGNDECL
    fidl_message_header_t hdr;
};

struct fuchsia_tracelink_RegistryRegisterTraceProviderRequest {
    FIDL_ALIGNDECL
    fidl_message_header_t hdr;
    zx_handle_t provider;
};

struct fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyRequest {
    FIDL_ALIGNDECL
    fidl_message_header_t hdr;
    zx_handle_t provider;
    uint64_t pid;
    fidl_string_t name;
};

struct fuchsia_tracelink_RegistryRegisterTraceProviderSynchronouslyResponse {
    FIDL_ALIGNDECL
    fidl_message_header_t hdr;
    int32_t s;
    bool started;
};


// Simple bindings 

zx_status_t fuchsia_tracelink_RegistryRegisterTraceProvider(zx_handle_t _channel, zx_handle_t provider);
zx_status_t fuchsia_tracelink_RegistryRegisterTraceProviderSynchronously(zx_handle_t _channel, zx_handle_t provider, uint64_t pid, const char* name_data, size_t name_size, int32_t* out_s, bool* out_started);

typedef struct fuchsia_tracelink_Registry_ops {
    zx_status_t (*RegisterTraceProvider)(void* ctx, zx_handle_t provider);
    zx_status_t (*RegisterTraceProviderSynchronously)(void* ctx, zx_handle_t provider, uint64_t pid, const char* name_data, size_t name_size, fidl_txn_t* txn);
} fuchsia_tracelink_Registry_ops_t;

zx_status_t fuchsia_tracelink_Registry_dispatch(void* ctx, fidl_txn_t* txn, fidl_msg_t* msg, const fuchsia_tracelink_Registry_ops_t* ops);
zx_status_t fuchsia_tracelink_Registry_try_dispatch(void* ctx, fidl_txn_t* txn, fidl_msg_t* msg, const fuchsia_tracelink_Registry_ops_t* ops);

zx_status_t fuchsia_tracelink_RegistryRegisterTraceProviderSynchronously_reply(fidl_txn_t* _txn, int32_t s, bool started);

#if defined(__cplusplus)
}
#endif
