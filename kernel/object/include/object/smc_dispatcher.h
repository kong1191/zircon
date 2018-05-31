// Copyright 2018 Open Trust Group
// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <object/dispatcher.h>

#include <kernel/event.h>

#include <fbl/canary.h>
#include <fbl/mutex.h>
#include <zircon/syscalls/smc.h>

class SmcDispatcher final : public SoloDispatcher {
public:
    static zx_status_t Create(uint32_t options, fbl::RefPtr<SmcDispatcher>* dispatcher,
                              zx_rights_t* rights, fbl::RefPtr<VmObject>* shm_vmo);
    static SmcDispatcher* GetDispatcherByEntity(uint32_t entity_nr);

    ~SmcDispatcher() final;
    zx_obj_type_t get_type() const final { return ZX_OBJ_TYPE_SMC; }
    bool has_state_tracker() const final { return true; }

    /* called by libsm */
    zx_status_t NotifyUser(smc32_args_t* args);
    long WaitForResult();

    /* called by smc service via syscalls */
    zx_status_t ReadArgs(smc32_args_t* args);
    zx_status_t SetResult(long result);
    zx_info_smc_t GetSmcInfo();

private:
    explicit SmcDispatcher(uint32_t options, zx_info_smc_t info);

    fbl::Canary<fbl::magic("SMCD")> canary_;

    const uint32_t options;
    smc32_args_t* smc_args TA_GUARDED(get_lock());
    long smc_result TA_GUARDED(get_lock());
    bool can_serve_next_smc TA_GUARDED(get_lock());
    event_t result_event_;
    zx_info_smc_t smc_info;
};