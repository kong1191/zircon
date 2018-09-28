// Copyright 2018 Open Trust Group
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <inttypes.h>

#include <vm/arch_vm_aspace.h>
#include <vm/vm_aspace.h>
#include <vm/vm_object_physical.h>
#include <object/smc_dispatcher.h>
#include <object/c_user_smc_service.h>

#include <zircon/rights.h>

#include <err.h>
#include <string.h>
#include <trace.h>

#include <kernel/thread.h>
#include <fbl/alloc_checker.h>
#include <fbl/auto_lock.h>

#define LOCAL_TRACE 0
#if WITH_LIB_SM
#include <lib/sm.h>
#define ENABLE_SMC_TEST 1
#endif

static fbl::Mutex alloc_lock;
static SmcDispatcher* smc_disp TA_GUARDED(alloc_lock);

#if ENABLE_SMC_TEST
static void* map_shm(zx_info_smc_t* smc_info) {
    void* shm_vaddr = nullptr;

    zx_status_t status = VmAspace::kernel_aspace()->AllocPhysical(
            "smc_ns_shm", smc_info->ns_shm.size, &shm_vaddr, PAGE_SIZE_SHIFT,
            static_cast<paddr_t>(smc_info->ns_shm.base_phys), VmAspace::VMM_FLAG_COMMIT,
            ARCH_MMU_FLAG_PERM_READ | ARCH_MMU_FLAG_PERM_WRITE | ARCH_MMU_FLAG_NS);
    if (status != ZX_OK) {
        TRACEF("failed to map shm into kernel address space, status %d\n", status);
        return nullptr;
    }
    return shm_vaddr;
}

static void unmap_shm(void* va) {
    if (va) {
        VmAspace::kernel_aspace()->FreeRegion(reinterpret_cast<vaddr_t>(va));
    }
}

static long write_shm() {
    bool is_fail = false;
    zx_info_smc_t smc_info = smc_disp->GetSmcInfo();

    uint8_t* shm_va = static_cast<uint8_t*>(map_shm(&smc_info));
    if (shm_va == nullptr) {
        is_fail = true;
        goto exit;
    }

    for (size_t i = 0; i < smc_info.ns_shm.size; i++) {
        shm_va[i] = static_cast<uint8_t>((i & 0xff) ^ 0xaa);
    }

exit:
    unmap_shm(shm_va);
    return is_fail ? SM_ERR_INTERNAL_FAILURE : 0;
}

static long verify_shm() {
    bool is_fail = false;
    zx_info_smc_t smc_info = smc_disp->GetSmcInfo();

    uint8_t* shm_va = static_cast<uint8_t*>(map_shm(&smc_info));
    if (shm_va == nullptr) {
        is_fail = true;
        goto exit;
    }

    for (uint32_t i = 0; i < smc_info.ns_shm.size; i++) {
        if (shm_va[i] != (i & 0xff)) {
            TRACEF("error: shm_va[%u] 0x%02x, expected 0x%02x\n",
                        i, shm_va[i], (i & 0xff));
            is_fail = true;
            break;
        }
    }

exit:
    unmap_shm(shm_va);
    return is_fail ? SM_ERR_INTERNAL_FAILURE : 0;
}

static long invoke_smc_test(smc32_args_t* args) {
    switch (args->smc_nr) {
    case SMC_SC_WRITE_SHM:
        return write_shm();
    case SMC_SC_VERIFY_SHM:
        return verify_shm();
    default:
        return SM_ERR_UNDEFINED_SMC;
    }
}

#endif

zx_status_t SmcDispatcher::Create(uint32_t options, fbl::RefPtr<SmcDispatcher>* dispatcher,
                                  zx_rights_t* rights, fbl::RefPtr<VmObject>* shm_vmo) {
#if WITH_LIB_SM
    fbl::AutoLock al(&alloc_lock);

    if (smc_disp == nullptr) {
        ns_shm_info_t info = {};

        sm_get_shm_config(&info);
        if (info.size == 0) return ZX_ERR_INTERNAL;

        fbl::RefPtr<VmObject> vmo;
        uintptr_t shm_pa = static_cast<uintptr_t>(info.pa);
        size_t shm_size = ROUNDUP_PAGE_SIZE(static_cast<size_t>(info.size));

        zx_status_t status = VmObjectPhysical::Create(shm_pa, shm_size, &vmo);
        if (status != ZX_OK) return status;

        if (info.use_cache) {
            status = vmo->SetMappingCachePolicy(ARCH_MMU_FLAG_CACHED);
            if (status != ZX_OK) return status;
        }

        zx_info_smc_t smc_info = {
            .ns_shm = {
                .base_phys = info.pa,
                .size = info.size,
                .use_cache = info.use_cache,
            },
        };

        fbl::AllocChecker ac;
        auto disp = new (&ac) SmcDispatcher(options, smc_info);
        if (!ac.check()) {
            return ZX_ERR_NO_MEMORY;
        }

        *rights = ZX_DEFAULT_SMC_RIGHTS;
        *dispatcher = fbl::AdoptRef<SmcDispatcher>(disp);
        *shm_vmo = fbl::move(vmo);
        smc_disp = dispatcher->get();
        LTRACEF("create smc object, koid=%" PRIu64 "\n", smc_disp->get_koid());
        return ZX_OK;
    }

    TRACEF("error: smc kernel object already existed\n");
    return ZX_ERR_BAD_STATE;
#else
    TRACEF("error: libsm is not enabled\n");
    return ZX_ERR_NOT_SUPPORTED;
#endif

}

SmcDispatcher::SmcDispatcher(uint32_t options, zx_info_smc_t info)
    : options(options), smc_args(nullptr),
      smc_result(SM_ERR_INTERNAL_FAILURE),
      can_serve_next_smc(true),
      smc_info(info) {
    event_init(&result_event_, false, EVENT_FLAG_AUTOUNSIGNAL);

    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        event_init(&req_nop_event_[i], false, EVENT_FLAG_AUTOUNSIGNAL);
    }
}

SmcDispatcher::~SmcDispatcher() {
    fbl::AutoLock al(&alloc_lock);
    LTRACEF("free smc object, koid=%" PRIu64 "\n", smc_disp->get_koid());
    smc_disp = nullptr;
}

zx_info_smc_t SmcDispatcher::GetSmcInfo() {
    return smc_info;
}

zx_status_t SmcDispatcher::NotifyUser(smc32_args_t* args) {
    canary_.Assert();

    Guard<fbl::Mutex> guard{get_lock()};

    if (can_serve_next_smc) {
        smc_args = args;
        /* notify smc service to get smc by ReadArgs() */
        UpdateStateLocked(0, ZX_SMC_READABLE);
        can_serve_next_smc = false;
        return ZX_OK;
    }

    return ZX_ERR_SHOULD_WAIT;
}

long SmcDispatcher::WaitForResult() {
    canary_.Assert();

    zx_status_t status = event_wait_deadline(&result_event_, ZX_TIME_INFINITE, true);

    Guard<fbl::Mutex> guard{get_lock()};

    DEBUG_ASSERT(!can_serve_next_smc);

    can_serve_next_smc = true;
    return status == ZX_OK ? smc_result : SM_ERR_INTERNAL_FAILURE;
}

zx_status_t SmcDispatcher::ReadArgs(smc32_args_t* args) {
    canary_.Assert();

    if (!args) return ZX_ERR_INVALID_ARGS;

    Guard<fbl::Mutex> guard{get_lock()};

    zx_signals_t signals = GetSignalsStateLocked();

    if (signals & ZX_SMC_READABLE) {
        memcpy(args, smc_args, sizeof(smc32_args_t));
        /* set ZX_SMC_SIGNALED to allow smc service can do SetResult() */
        UpdateStateLocked(ZX_SMC_READABLE, ZX_SMC_SIGNALED);
        return ZX_OK;
    }

    return ZX_ERR_SHOULD_WAIT;
}

zx_status_t SmcDispatcher::SetResult(long result) {
    canary_.Assert();

    Guard<fbl::Mutex> guard{get_lock()};

    zx_signals_t signals = GetSignalsStateLocked();

    if (signals & ZX_SMC_SIGNALED) {
        smc_result = result;
        UpdateStateLocked(ZX_SMC_SIGNALED, 0);
        /* notify libsm to get smc result */
        event_signal(&result_event_, false);
        return ZX_OK;
    }

    return ZX_ERR_BAD_STATE;
}

zx_status_t SmcDispatcher::WriteNopRequest(uint32_t cpu_num,
                                           smc32_args_t* args) {
    canary_.Assert();

    if (!args || !is_valid_cpu_num(cpu_num))
        return ZX_ERR_INVALID_ARGS;

    LTRACEF("thread %s, cpu_num %u\n", get_current_thread()->name, cpu_num);

    {
        Guard<fbl::Mutex> guard{get_lock()};
        memcpy(&req_nop_args[cpu_num], args, sizeof(smc32_args_t));
    }
    event_signal(&req_nop_event_[cpu_num], true);
    return ZX_OK;
}

zx_status_t SmcDispatcher::ReadNopRequest(uint32_t cpu_num,
                                          smc32_args_t* args) {
    canary_.Assert();

    if (!args || !is_valid_cpu_num(cpu_num))
        return ZX_ERR_INVALID_ARGS;

    LTRACEF("thread %s, cpu_num %u\n", get_current_thread()->name, cpu_num);

    zx_status_t status = event_wait_deadline(&req_nop_event_[cpu_num],
                                             ZX_TIME_INFINITE, true);
    if (status != ZX_OK)
        return status;

    Guard<fbl::Mutex> guard{get_lock()};
    memcpy(args, &req_nop_args[cpu_num], sizeof(smc32_args_t));
    return ZX_OK;
}

zx_status_t SmcDispatcher::CancelReadNopRequest() {
    uint32_t cpu;

    for (cpu = 0; cpu < SMP_MAX_CPUS; cpu++) {
        event_signal_etc(&req_nop_event_[cpu], false, ZX_ERR_CANCELED);
    }
    return ZX_OK;
}


#if WITH_LIB_SM
long notify_smc_service(smc32_args_t* args) {
    if (args == nullptr) return SM_ERR_INVALID_PARAMETERS;

    if (smc_disp == nullptr) return smc_undefined(args);

#if ENABLE_SMC_TEST
    if (SMC_ENTITY(args->smc_nr) == SMC_ENTITY_TEST) {
        return invoke_smc_test(args);
    }
#endif

    zx_status_t status = smc_disp->NotifyUser(args);
    if (status != ZX_OK) return SM_ERR_BUSY;

    return smc_disp->WaitForResult();
}

long notify_nop_thread(smc32_args_t* args) {
    if (args == nullptr) return SM_ERR_INVALID_PARAMETERS;

    if (smc_disp == nullptr) return smc_undefined(args);

    zx_status_t status = smc_disp->WriteNopRequest(arch_curr_cpu_num(), args);
    if (status != ZX_OK) {
        TRACEF("warning: Failed to send nop request, status %d\n", status);
    }

    return SM_OK;
}
#endif