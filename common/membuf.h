#ifndef COMMON_MENBUF_H
#define COMMON_MENBUF_H
#include <boost/interprocess/streams/bufferstream.hpp>
#include <streambuf>

namespace common {

// using MemBuf = boost::interprocess::bufferstream;

struct MemBuf : std::streambuf {
    MemBuf() {}
    MemBuf(const char *base, size_t size) { init(base, size); }

    void init(const char *base, size_t size) {
        char *p(const_cast<char *>(base));
        std::streambuf::setg(p, p, p + size);
        std::streambuf::setp(p, p + size);

        base_ = p;
        size_ = size;
    }
    pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                     std::ios_base::openmode which = std::ios_base::in) {
        if (which == std::ios_base::in) {
            if (dir == std::ios_base::cur)
                gbump(off);
            // else if (dir == std::ios_base::end)
            //     std::streambuf::setg(eback(), egptr() + off, egptr());
            else if (dir == std::ios_base::beg)
                std::streambuf::setg(base_, base_ + off, base_ + size_);
            return gptr() - eback();
        }
        if (which == std::ios_base::out) {
            if (dir == std::ios_base::cur)
                pbump(off);
            // else if (dir == std::ios_base::end)
            //     // FIXME: std::streambuf::setp(epptr(), epptr() + off,
            //     pptr()); std::streambuf::setp(epptr(), epptr() + off);
            else if (dir == std::ios_base::beg) {
                std::streambuf::setp(base_, base_ + size_);
                pbump(off);
            }
            return pptr() - pbase();
        }
        return gptr() - eback();
    }

    //  set relative input position
    pos_type seekg(int offset) {
        this->seekoff(offset, std::ios_base::cur, std::ios_base::in);
        return gptr() - eback();
    }

    // set relative output position
    pos_type seekp(int offset) {
        this->seekoff(offset, std::ios_base::cur, std::ios_base::out);
        return pptr() - pbase();
    }

    // set absolute input position
    pos_type setg(size_t offset) {
        this->seekoff(offset, std::ios_base::beg, std::ios_base::in);
        return offset;
    }

    // set absolute input position
    pos_type setp(size_t offset) {
        this->seekoff(offset, std::ios_base::beg, std::ios_base::out);
        return offset;
    }

    // tell the position of the input pointer
    pos_type tellg() const { return gptr() - eback(); }
    // tell the position of the output pointer
    pos_type tellp() const { return pptr() - pbase(); }

private:
    char *base_;
    size_t size_;
};

} // namespace common
#endif // !COMMON_MENBUF_H
