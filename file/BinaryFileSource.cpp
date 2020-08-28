// Copyright (c) 2014-2016 Josh Blum
//                    2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "FileDescriptor.hpp"

#include <Pothos/Framework.hpp>

#include <Poco/Logger.h>

/***********************************************************************
 * |PothosDoc Binary File Source
 *
 * Read data from a file and write it to an output stream on port 0.
 *
 * |category /Sources
 * |category /File IO
 * |keywords source binary file
 *
 * |param dtype[Data Type] The output data type.
 * |widget DTypeChooser(float=1,cfloat=1,int=1,cint=1,uint=1,cuint=1,dim=1)
 * |default "complex_float64"
 * |preview disable
 *
 * |param path[File Path] The path to the input file.
 * |default ""
 * |widget FileEntry(mode=open)
 *
 * |param rewind[Auto Rewind] Enable automatic file rewind.
 * When rewind is enabled, the binary file source will stream from the beginning
 * of the file after the end of file is reached.
 * |default false
 * |option [Disabled] false
 * |option [Enabled] true
 * |preview valid
 *
 * |factory /blocks/binary_file_source(dtype)
 * |setter setFilePath(path)
 * |setter setAutoRewind(rewind)
 **********************************************************************/
class BinaryFileSource : public Pothos::Block
{
public:
    static Block *make(const Pothos::DType &dtype)
    {
        return new BinaryFileSource(dtype);
    }

    BinaryFileSource(const Pothos::DType &dtype):
        _fd(-1),
        _rewind(false)
    {
        this->setupOutput(0, dtype);
        this->registerCall(this, POTHOS_FCN_TUPLE(BinaryFileSource, setFilePath));
        this->registerCall(this, POTHOS_FCN_TUPLE(BinaryFileSource, setAutoRewind));
    }

    void setFilePath(const std::string &path)
    {
        _path = path;
        //file was open -> close old fd, and open this new path
        if (_fd != -1)
        {
            this->deactivate();
            this->activate();
        }
    }

    void setAutoRewind(const bool rewind)
    {
        _rewind = rewind;
    }

    void activate(void)
    {
        if (_path.empty()) throw Pothos::FileException("BinaryFileSource", "empty file path");
        _fd = openSourceFD(_path.c_str()); 
        if (_fd < 0)
        {
            poco_error_f4(Poco::Logger::get("BinaryFileSource"), "open(%s) returned %d -- %s(%d)", _path, _fd, std::string(strerror(errno)), errno);
        }
    }

    void deactivate(void)
    {
        close(_fd);
        _fd = -1;
    }

    void work(void)
    {
        #ifdef _MSC_VER
        //TODO use windows API to have timeout
        #else
        //setup timeval for timeout
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = this->workInfo().maxTimeoutNs/1000; //ns->us

        //setup rset for timeout
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(_fd, &rset);

        //call select with timeout
        if (::select(_fd+1, &rset, NULL, NULL, &tv) <= 0) return this->yield();
        #endif

        auto out0 = this->output(0);
        void *ptr = out0->buffer();
        auto r = read(_fd, ptr, out0->buffer().length);
        if (r == 0 and _rewind) lseek(_fd, 0, SEEK_SET);
        if (r >= 0) out0->produce(size_t(r)/out0->dtype().size());
        else
        {
            poco_error_f3(Poco::Logger::get("BinaryFileSource"), "read() returned %d -- %s(%d)", int(r), std::string(strerror(errno)), errno);
        }
    }

private:
    int _fd;
    std::string _path;
    bool _rewind;
};

static Pothos::BlockRegistry registerBinaryFileSource(
    "/blocks/binary_file_source", &BinaryFileSource::make);
