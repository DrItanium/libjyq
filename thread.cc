/* Public Domain --Kris Maglione */
#include <unistd.h>
#include "ixp_local.h"

namespace ixp {
Mutex::Mutex() { concurrency::threadModel->init(this); }
Mutex::~Mutex() { concurrency::threadModel->destroy(this); }

Rendez::Rendez() { concurrency::threadModel->init(this); }
Rendez::~Rendez() { concurrency::threadModel->destroy(this); }

RWLock::RWLock() { concurrency::threadModel->init(this); }
RWLock::~RWLock() { concurrency::threadModel->destroy(this); }
namespace concurrency {

ssize_t 
ThreadImpl::read(int fd, void* buf, size_t count) {
    return ::read(fd, buf, count);
}
ssize_t 
ThreadImpl::write(int fd, const void* buf, size_t count) {
    return ::write(fd, buf, count);
}
int 
ThreadImpl::select(int fd, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, timeval* timeout) {
    return ::select(fd, readfds, writefds, exceptfds, timeout);
}
char*
NoThreadImpl::errbuf() {
    static char errbuf[ErrorMax];
    return errbuf;
}
void
NoThreadImpl::sleep(Rendez*) {
    throw "unimplemented";
}
std::unique_ptr<ThreadImpl> threadModel = std::make_unique<NoThreadImpl>();

} // end namespace concurrency

void Mutex::lock() { concurrency::threadModel->lock(this); }
void Mutex::unlock() { concurrency::threadModel->unlock(this); }
bool Mutex::canlock() { return concurrency::threadModel->canlock(this); }
void RWLock::readLock() { concurrency::threadModel->rlock(this); }
void RWLock::readUnlock() { concurrency::threadModel->runlock(this); }
bool RWLock::canReadLock() { return concurrency::threadModel->canrlock(this); }
void RWLock::writeLock() { concurrency::threadModel->wlock(this); }
void RWLock::writeUnlock() { concurrency::threadModel->wunlock(this); }
bool RWLock::canWriteLock() { concurrency::threadModel->canwlock(this); }
bool Rendez::wake() { return concurrency::threadModel->wake(this); }
bool Rendez::wakeall() { return concurrency::threadModel->wakeall(this); }
void Rendez::sleep() { concurrency::threadModel->sleep(this); }


}
