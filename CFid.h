/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#ifndef LIBJYQ_CFID_H__
#define LIBJYQ_CFID_H__
#include <string>
#include <functional>
#include <list>
#include <memory>
#include "types.h"
#include "thread.h"
#include "qid.h"
#include "Fcall.h"
#include "stat.h"

namespace jyq {
    struct CFid {
        public:
            uint32_t fid;
            Qid      qid;
        private:
            uint8_t  _mode;
            uint     _open;
            uint     _iounit;
            uint32_t _offset;
        public:
            constexpr auto getIoUnit() const noexcept { return _iounit; }
            constexpr auto getOpen() const noexcept { return _open; }
            constexpr auto getOffset() const noexcept { return _offset; }
            constexpr auto getMode() const noexcept { return _mode; }
            void setIoUnit(uint value) noexcept { _iounit = value; }
            void setOpen(uint value) noexcept { _open = value; }
            void setOffset(uint32_t value) noexcept { _offset = value; }
            void setMode(uint8_t value) noexcept { _mode = value; }
            bool close(DoFcallFunc);
            bool clunk(DoFcallFunc); 
            bool performClunk(DoFcallFunc);
            long read(void*, long, DoFcallFunc);
            long pread(void*, long, int64_t, DoFcallFunc);
            long pwrite(const void*, long, int64_t, DoFcallFunc);
            long write(const void*, long, DoFcallFunc);
            std::shared_ptr<Stat> fstat(DoFcallFunc);
            Mutex& getIoLock() noexcept { return _iolock; }
        private:
            Mutex _iolock;
    };
} // end namespace jyq
#endif // end LIBJYQ_CFID_H__
