/*
 * skn_signal_manager.c
 *
 *  Created on: Jul 25, 2015
 *      Author: jscott
 */

#include "skn_network_helpers.h"



static int skn_signal_manager_process_signals(siginfo_t *signal_info);
static void *skn_signal_manager_handler_thread(void *l_thread_complete);

/*
 * process_signals()
 *
 * Handle/Process linux signals for the whole multi-threaded application.
 *
 * Params:
 *    sig -- current linux signal
 *
 * Returns/Affects:
 *   returns current value of the atomic int gi_exit_flag
 *   returns true (or current value) if nothing needs done
 *   returns 0 or false if exit is required
 */
static int skn_signal_manager_process_signals(siginfo_t *signal_info) {
    int rval = gi_exit_flag; /* use existing value */
    int sig = 0;
    char *pch = "<unknown>";

    assert(signal_info != NULL);

    sig = signal_info->si_signo;

    /*
     * look to see what signal has been caught
     */
    switch (sig) {
        case SIGHUP: /* often used to reload configuration */
//      rval = 33; /* flag a reload of the ip address info */
            skn_logger(SD_NOTICE, "%s received: Requesting IP Address Info reload => [pid=%d, uid=%d]", strsignal(sig), signal_info->si_pid,
                       signal_info->si_uid);
            break;
        case SIGUSR1: /* Any user function */
            switch (signal_info->si_code) {
                case SI_USER:
                    pch = "kill(2) or raise(3)";
                    break;
                case SI_KERNEL:
                    pch = "Sent by the kernel.";
                    break;
                case SI_QUEUE:
                    pch = "sigqueue(2)";
                    break;
                case SI_TIMER:
                    pch = "POSIX timer expired";
                    break;
                case SI_MESGQ:
                    pch = "POSIX message queue state changed";
                    break;
                case SI_ASYNCIO:
                    pch = "AIO completed";
                    break;
                case SI_SIGIO:
                    pch = "queued SIGIO";
                    break;
                case SI_TKILL:
                    pch = "tkill(2) or tgkill(2)";
                    break;
                default:
                    pch = "<unknown>";
                    break;
            }
            skn_logger(SD_NOTICE, "%s received from => %s ?[pid=%d, uid=%d] signaling application shutdown.", strsignal(sig), pch, signal_info->si_pid, signal_info->si_uid);
            rval = sig;
            break;
        case SIGCHLD: /* some child ended */
            switch (signal_info->si_code) {
                case CLD_EXITED:
                    pch = "child has exited";
                    break;
                case CLD_KILLED:
                    pch = "child was killed";
                    break;
                case CLD_DUMPED:
                    pch = "child terminated abnormally";
                    break;
                case CLD_TRAPPED:
                    pch = "traced child has trapped";
                    break;
                case CLD_STOPPED:
                    pch = "child has stopped";
                    break;
                case CLD_CONTINUED:
                    pch = "stopped child has continued";
                    break;
                default:
                    pch = "<unknown>";
                    break;
            }
            skn_logger(SD_NOTICE, "%s received for pid => %d, w/rc => %d for this reason => %s {Ignored}", strsignal(sig), signal_info->si_pid,
                       signal_info->si_status, pch);
            break;
        case SIGQUIT: /* often used to signal an orderly shutdown */
        case SIGINT: /* often used to signal an orderly shutdown */
        case SIGPWR: /* Power Failure */
        case SIGKILL: /* Fatal Exit flag */
        case SIGTERM: /* Immediately Fatal Exit flag */
        default:
            switch (signal_info->si_code) {
                case SI_USER:
                    pch = "kill(2) or raise(3)";
                    break;
                case SI_KERNEL:
                    pch = "Sent by the kernel.";
                    break;
                case SI_QUEUE:
                    pch = "sigqueue(2)";
                    break;
                case SI_TIMER:
                    pch = "POSIX timer expired";
                    break;
                case SI_MESGQ:
                    pch = "POSIX message queue state changed";
                    break;
                case SI_ASYNCIO:
                    pch = "AIO completed";
                    break;
                case SI_SIGIO:
                    pch = "queued SIGIO";
                    break;
                case SI_TKILL:
                    pch = "tkill(2) or tgkill(2)";
                    break;
                default:
                    pch = "<unknown>";
                    break;
            }
            skn_logger(SD_NOTICE, "%s received from => %s ?[pid=%d, uid=%d]{Exiting}", strsignal(sig), pch, signal_info->si_pid, signal_info->si_uid);
            rval = sig;
            break;
    } /* end switch */

    return rval;
}

/**
 *  handler_thread()
 *
 *  Trap linux signals for the whole multi-threaded application.
 *
 *  Params:
 *    real_user_id  -- demos passing a ptr or value into thread context.
 *
 *  Returns/Affects:
 *      returns and/or set the atomic gint gi_exit_flag
 *      returns last signal
 */
static void *skn_signal_manager_handler_thread(void *l_thread_complete) {
    sigset_t signal_set;
    siginfo_t signal_info;
//    struct timespec timeout;
    int sig = 0;
    int rval = 0;
    long *threadC = (long *)l_thread_complete;

    *threadC = 1;

//    timeout.tv_nsec = 0;
//    timeout.tv_sec = 8;
    sigfillset(&signal_set);
    skn_logger(SD_NOTICE, "SignalManager: Startup Successful...");

    while (gi_exit_flag == SKN_RUN_MODE_RUN) {
        /* wait for any and all signals */
        /* OLD: sigwait (&signal_set, &sig); */
        sig = sigwaitinfo(&signal_set, &signal_info);
        if (sig == PLATFORM_ERROR) {
            if (errno == EAGAIN) {
                continue;
            }
            skn_logger(SD_WARNING, "SignalManager: sigwaitinfo() returned an error => {%s}", strerror(errno));
            gi_exit_flag = SKN_RUN_MODE_STOP;
            break;
        }
        /* when we get this far, we've  caught a signal */
        rval = skn_signal_manager_process_signals(&signal_info);
        gi_exit_flag = rval;

    } /* end-while */

    pthread_sigmask(SIG_UNBLOCK, &signal_set, NULL);

    skn_logger(SD_NOTICE, "SignalManager: Thread Shutdown Complete.");

    *threadC = 0;

    pthread_exit((void *) (long int) sig);

    return NULL;
}

/**
 * Final step
 * - may send trapped signal to app, so requester knows it was honored
 */
int skn_signal_manager_shutdown(pthread_t sig_thread, sigset_t *psignal_set, long *l_thread_complete) {
    void *trc = NULL;
    int rc = EXIT_SUCCESS;

    if (gi_exit_flag <= SKN_RUN_MODE_STOP) {
        gi_exit_flag = SKN_RUN_MODE_STOP; /* shut down the system -- work is done */
        // need to force theads down or interrupt them
        skn_logger(SD_WARNING, "shutdown caused by application!");
        sleep(1);
        if (*l_thread_complete != 0) {
            pthread_cancel(sig_thread);
            sleep(1);
        }
        skn_logger(SD_WARNING, "Collecting (cleanup) threads.");
        pthread_join(sig_thread, &trc);
    } else {
        rc = EXIT_FAILURE;
        skn_logger(SD_NOTICE, "Collecting signal thread's return code.");
        pthread_join(sig_thread, &trc);
        skn_logger(SD_NOTICE, "Signal thread was ended by a %d:%s signal.", gi_exit_flag, strsignal((int) (long int) trc));
    }
    pthread_sigmask(SIG_UNBLOCK, psignal_set, NULL);

    return rc;
}

/**
 * Initialize signal manager
 */
int skn_signal_manager_startup(pthread_t *psig_thread, sigset_t *psignal_set, long *l_thread_complete) {
    int i_thread_rc = 0; // EXIT_SUCCESS

    sigfillset(psignal_set);
    pthread_sigmask(SIG_BLOCK, psignal_set, NULL);

    i_thread_rc = pthread_create(psig_thread, NULL, skn_signal_manager_handler_thread, (void*) l_thread_complete);
    if (i_thread_rc == PLATFORM_ERROR) {
        skn_logger(SD_ERR, "Create signal thread failed: %d:%s", errno, strerror(errno));
        pthread_sigmask(SIG_UNBLOCK, psignal_set, NULL);
        i_thread_rc = EXIT_FAILURE;
    }
    sleep(1); // give thread a chance to start

    return i_thread_rc;
}
