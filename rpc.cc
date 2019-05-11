/* From Plan 9's libmux.
 * Copyright (c) 2003 Russ Cox, Massachusetts Institute of Technology
 * Distributed under the same terms as libjyq.
 */
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "Rpc.h"
#include "Msg.h"
#include "thread.h"
#include "Client.h"
#include "socket.h"
#include "util.h"
#include "PrintFunctions.h"

namespace jyq {
Rpc::Rpc(Client& m) : mux(m) {
    waiting = true;
    r.mutex = &m.lk;
    p = nullptr;
}

Fcall*
Client::muxrecv()
{
	//Fcall *f = nullptr;
    concurrency::Locker<Mutex> theRlock(rlock);
    if (fd.recvmsg(rmsg) == 0) {
        return nullptr;
    }
	if(auto f = new Fcall(); msg2fcall(&rmsg, f) == 0) {
        delete f;
        return nullptr;
	} else {
        return f;
    }
}


void
Client::electmuxer()
{
	/* if there is anyone else sleeping, wake them to mux */
	for(auto rpc=sleep.next; rpc != &sleep; rpc = rpc->next){
        if (!rpc->isAsync()) {
			muxer = rpc;
			concurrency::threadModel->wake(&rpc->r);
			return;
		}
	}
	muxer = nullptr;
}
int 
Client::gettag(Rpc &r)
{
	int i, mw;
	Rpc **w;
    auto Found = [this, &r](auto index) {
        nwait++;
        wait[index] = &r;
        r.setTag(index + mintag);
        return r.getTag();
    };
	for(;;){
		/* wait for a free tag */
		while(nwait == mwait){
			if(mwait < maxtag-mintag){
				mw = mwait;
				if(mw == 0) {
					mw = 1;
                } else {
					mw <<= 1;
                }
				w = (decltype(w))realloc(wait, mw * sizeof *w);
                if (!w) {
					return -1;
                }
				memset(w+mwait, 0, (mw-mwait) * sizeof *w);
				wait = w;
				freetag = mwait;
				mwait = mw;
				break;
			}
            tagrend.sleep();
		}

		i=freetag;
		if(wait[i] == 0) {
            return Found(i);
        }
		for(; i<mwait; i++) {
			if(wait[i] == 0) {
                return Found(i);
            }
        }
		for(i=0; i<freetag; i++) {
			if(wait[i] == 0) {
                return Found(i);
            }
        }
		/* should not fall out of while without free tag */
        throw "Fell out of loop without free tag!";
	}

}

void
Client::puttag(Rpc& r)
{
	auto i = r.getTag() - mintag;
	assert(wait[i] == &r);
	wait[i] = nullptr;
	nwait--;
	freetag = i;
    tagrend.wake();
    r.getRendez().deactivate();
}
int
Rpc::sendrpc(Fcall *f)
{
	auto ret = 0;
	/* assign the tag, add selves to response queue */
    {
        concurrency::Locker<Mutex> lk(mux.lk);
        tag = mux.gettag(this);
        f->setTag(tag);
        mux.enqueue(this);
    }

    {
        concurrency::Locker<Mutex> a(mux.wlock);
        if(!fcall2msg(&mux.wmsg, f) || !mux.getConnection().sendmsg(mux.wmsg)) {
            concurrency::Locker<Mutex> lk(mux.lk);
            mux.dequeue(this);
            mux.puttag(this);
            ret = -1;
        }
    }
    return ret;
}
void
Client::dispatchandqlock(Fcall *f)
{
	int tag = f->getTag() - mintag;
    lk.lock();
	/* hand packet to correct sleeper */
	if(tag < 0 || tag >= mwait) {
		fprintf(stderr, "libjyq: received unfeasible tag: %d (min: %d, max: %d)\n", f->getTag(), mintag, mintag+mwait);
        //Fcall::free(f);
        delete f;
        return;
	}
	auto r2 = wait[tag];
    if (!r2 || !(r2->prev)) {
		fprintf(stderr, "libjyq: received message with bad tag\n");
        //Fcall::free(f);
        delete f;
        return;
	}
	r2->p = f;
    dequeue(r2);
    r2->r.wake();
}
void
Client::enqueue(Rpc* r) {
	r->next = sleep.next;
	r->prev = &sleep;
	r->next->prev = r;
	r->prev->next = r;
}

void
Client::dequeue(Rpc* r) {
	r->next->prev = r->prev;
	r->prev->next = r->next;
	r->prev = nullptr;
	r->next = nullptr;

}

Fcall*
Client::muxrpc(Fcall *tx)
{
	Rpc r(*this);
	Fcall *p = nullptr;

    if (r.sendrpc(tx) < 0) {
		return nullptr;
    }

    lk.lock();
	/* wait for our packet */
	while(muxer && muxer != &r && !r.p) {
        r.getRendez().sleep();
    }

	/* if not done, there's no muxer; start muxing */
	if(!r.p){
		assert(muxer == nullptr || muxer == &r);
		muxer = &r;
		while(!r.p){
            lk.unlock();
			p = muxrecv();
            if (!p) {
				/* eof -- just give up and pass the buck */
                lk.lock();
                dequeue(&r);
				break;
			}
			dispatchandqlock(p);
		}
		electmuxer();
	}
	p = r.p;
	puttag(&r);
    lk.unlock();
    if (!p) {
        wErrorString("unexpected eof");
    }
	return p;
}
} // end namespace jyq


