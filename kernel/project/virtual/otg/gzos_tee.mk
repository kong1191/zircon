# Copyright 2018 Open Trust Group
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

include kernel/project/virtual/otg/gzos_common.mk

KERNEL_DEFINES += WITH_LIB_SM=1
KERNEL_DEFINES += WITH_DEV_GZOS_SHM=1

MODULES += \
    kernel/dev/gzos/shm/server \
    third_party/lib/sm \