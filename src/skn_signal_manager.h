/**
 * skn_signal_manager.h
 *
 * Unix Signal Management
 * - Attempts to use global gi_exit_flag to signal app to close
 */


#ifndef SKN_SIGNAL_MANAGER_H__
#define SKN_SIGNAL_MANAGER_H__

#include "skn_common_headers.h"
#include <sys/utsname.h>

typedef struct _sigManager {
    pthread_t sig_thread;
    sigset_t signal_set;
    long l_thread_complete;
    sig_atomic_t *gi_exit_flag;
}SknSignalManager, *PSknSignalManager;

/*
 * General Utilities
*/
extern void skn_signals_init();
extern void skn_signals_cleanup(int sig);

extern PSknSignalManager sknSignalManagerInit(sig_atomic_t *gi_exit_flag);
extern void sknSignalManagerShutdown(PSknSignalManager pssm);


#endif // SKN_SIGNAL_MANAGER_H__
