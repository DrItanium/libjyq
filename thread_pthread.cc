/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#define _XOPEN_SOURCE 600
#include <cerrno>
#include <pthread.h>
#include <cstdlib>
#include <unistd.h>
#include <memory>
#include "thread_pthread.h"
#include "util.h"


namespace jyq::concurrency {
    using RawRWLock = std::shared_ptr<pthread_rwlock_t>;
    using RawMutexLock = std::shared_ptr<pthread_mutex_t>;
    using RawRendez = std::shared_ptr<pthread_cond_t>;
    bool 
    PThreadImpl::init(jyq::RWLock* rw) { 
        if (auto rwlock = std::make_shared<pthread_rwlock_t>(); pthread_rwlock_init(rwlock.get(), nullptr)) {
        //if (auto rwlock = new pthread_rwlock_t; pthread_rwlock_init(rwlock, nullptr)) {
            return true;
        } else {
            rw->aux = std::move(rwlock);
            return false;
        }
    }
    bool 
    PThreadImpl::init(jyq::Rendez* r) {
        if (auto cond = std::make_shared<pthread_cond_t>(); pthread_cond_init(cond.get(), nullptr)) {
            return true;
        } else {
            r->aux = std::move(cond);
            return false;
        }
    }
    bool 
    PThreadImpl::init(jyq::Mutex* m) { 
        if (auto mutex = std::make_shared<pthread_mutex_t>(); pthread_mutex_init(mutex.get(), nullptr)) {
            return true;
        } else {
            m->aux = std::move(mutex);
            return false;
        }
    }
    void 
    PThreadImpl::destroy(jyq::Rendez* r) { 
        auto val = std::any_cast<RawRendez>(r->aux);
        pthread_cond_destroy(val.get());
        r->aux.reset();
    }

    void 
    PThreadImpl::destroy(jyq::Mutex* m) { 
        auto mut = std::any_cast<RawMutexLock>(m->aux);
        pthread_mutex_destroy(mut.get());
        m->aux.reset();
    }

    void 
    PThreadImpl::destroy(jyq::RWLock* rw) { 
        auto val = std::any_cast<RawRWLock>(rw->aux);
        pthread_rwlock_destroy(val.get());
        rw->aux.reset();
    }

    char* 
    PThreadImpl::errbuf() { 
        static pthread_key_t errstr_k;
        auto ret = (char*)pthread_getspecific(errstr_k);
        if (!ret) {
            ret = new char[ErrorMax];
            pthread_setspecific(errstr_k, (void*)ret);
        }
        return ret;
    }
    bool 
    PThreadImpl::wake(jyq::Rendez* r) { 
        auto val = std::any_cast<RawRendez>(r->aux);
        pthread_cond_signal(val.get());
        return false;
    }
    bool 
    PThreadImpl::wakeall(jyq::Rendez* r) { 
        auto val = std::any_cast<RawRendez>(r->aux);
        pthread_cond_broadcast(val.get());
        return 0;
    }   

    void PThreadImpl::rlock(jyq::RWLock* rw) { pthread_rwlock_rdlock(std::any_cast<RawRWLock>(rw->aux).get()); }
    bool PThreadImpl::canrlock(jyq::RWLock* rw) { return !pthread_rwlock_tryrdlock(std::any_cast<RawRWLock>(rw->aux).get()); }
    void PThreadImpl::runlock(jyq::RWLock* rw) { pthread_rwlock_unlock(std::any_cast<RawRWLock>(rw->aux).get()); }
    void PThreadImpl::wlock(jyq::RWLock* rw) { pthread_rwlock_rdlock(std::any_cast<RawRWLock>(rw->aux).get()); }
    bool PThreadImpl::canwlock(jyq::RWLock* rw) { return !pthread_rwlock_tryrdlock(std::any_cast<RawRWLock>(rw->aux).get()); }
    void PThreadImpl::wunlock(jyq::RWLock* rw) { pthread_rwlock_unlock(std::any_cast<RawRWLock>(rw->aux).get()); }
    bool PThreadImpl::canlock(jyq::Mutex* m) { return !pthread_mutex_trylock(std::any_cast<RawMutexLock>(m->aux).get()); }
    void PThreadImpl::lock(jyq::Mutex* m)   { pthread_mutex_lock(std::any_cast<RawMutexLock>(m->aux).get()); }
    void PThreadImpl::unlock(jyq::Mutex* m) { pthread_mutex_unlock(std::any_cast<RawMutexLock>(m->aux).get()); }
    void PThreadImpl::sleep(jyq::Rendez* r) { pthread_cond_wait(std::any_cast<RawRendez>(r->aux).get(), std::any_cast<RawMutexLock>(r->mutex->aux).get()); }
} // end namespace jyq::concurrency 
