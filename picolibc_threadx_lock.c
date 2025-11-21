#include "tx_api.h"
#include <sys/lock.h>

// Wrap ThreadX mutex in a lock object
typedef struct {
    TX_MUTEX mutex;
} threadx_lock_t;

// Initialize a lock
void __retarget_lock_init(_LOCK_T *lock) {
    threadx_lock_t *tl = (threadx_lock_t *)lock;
    tx_mutex_create(&tl->mutex, "picolibc_lock", TX_NO_INHERIT);
}

// Acquire a lock (blocking)
void __retarget_lock_acquire(_LOCK_T lock) {
    threadx_lock_t *tl = (threadx_lock_t *)lock;
    tx_mutex_get(&tl->mutex, TX_WAIT_FOREVER);
}

// Release a lock
void __retarget_lock_release(_LOCK_T lock) {
    threadx_lock_t *tl = (threadx_lock_t *)lock;
    tx_mutex_put(&tl->mutex);
}

// Destroy a lock
void __retarget_lock_close(_LOCK_T lock) {
    threadx_lock_t *tl = (threadx_lock_t *)lock;
    tx_mutex_delete(&tl->mutex);
}
