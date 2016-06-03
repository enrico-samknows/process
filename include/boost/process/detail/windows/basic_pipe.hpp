// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PROCESS_DETAIL_WINDOWS_PIPE_HPP
#define BOOST_PROCESS_DETAIL_WINDOWS_PIPE_HPP

#include <boost/detail/winapi/basic_types.hpp>
#include <boost/detail/winapi/pipes.hpp>
#include <boost/detail/winapi/handles.hpp>
#include <boost/detail/winapi/file_management.hpp>
#include <boost/detail/winapi/get_last_error.hpp>
#include <boost/detail/winapi/access_rights.hpp>
#include <boost/detail/winapi/process.hpp>
#include <boost/process/detail/windows/compare_handles.hpp>
#include <system_error>
#include <string>


namespace boost { namespace process { namespace detail { namespace windows {

template<class CharT, class Traits = std::char_traits<CharT>>
class basic_pipe
{
    ::boost::detail::winapi::HANDLE_ _source = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    ::boost::detail::winapi::HANDLE_ _sink   = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
public:
    typedef CharT                      char_type  ;
    typedef          Traits            traits_type;
    typedef typename Traits::int_type  int_type   ;
    typedef typename Traits::pos_type  pos_type   ;
    typedef typename Traits::off_type  off_type   ;
    typedef ::boost::detail::winapi::HANDLE_ native_handle;

    basic_pipe(::boost::detail::winapi::HANDLE_ source, ::boost::detail::winapi::HANDLE_ sink)
            : _source(source), _sink(sink) {}
    inline explicit basic_pipe(const std::string & name);
    inline basic_pipe(const basic_pipe& p);
    basic_pipe(basic_pipe&& lhs)  : _source(lhs._source), _sink(lhs._sink)
    {
        lhs._source = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
        lhs._sink = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    }
    inline basic_pipe& operator=(const basic_pipe& p);
    basic_pipe& operator=(basic_pipe&& lhs);
    ~basic_pipe()
    {
        if (_sink   != ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
            ::boost::detail::winapi::CloseHandle(_sink);
        if (_source != ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
            ::boost::detail::winapi::CloseHandle(_source);
    }
   native_handle native_source() const {return _source;}
   native_handle native_sink  () const {return _sink;}

    basic_pipe()
    {
        if (!::boost::detail::winapi::CreatePipe(&_source, &_sink, nullptr, 0))
            throw std::system_error(
                    std::error_code(
                    ::boost::detail::winapi::GetLastError(),
                    std::system_category()),
                    "CreatePipe() failed");

    }

    int_type write(const char_type * data, int_type count)
    {
        constexpr static ::boost::detail::winapi::DWORD_ ERROR_BROKEN_PIPE_ = 109;

        ::boost::detail::winapi::DWORD_ write_len;
        if (!::boost::detail::winapi::WriteFile(
                _sink, data, count * sizeof(char_type), &write_len, nullptr
                ))
        {
            auto ec = ::boost::process::detail::get_last_error();
            if (ec.value() == /*::boost::detail::winapi::*/ERROR_BROKEN_PIPE_)
                return 0;
            else
                throw std::system_error(ec, "WriteFile failed");
        }
        return write_len;
    }
    int_type read(char_type * data, int_type count)
    {
        constexpr static ::boost::detail::winapi::DWORD_ ERROR_BROKEN_PIPE_ = 109;

        ::boost::detail::winapi::DWORD_ read_len;
        if (!::boost::detail::winapi::ReadFile(
                _source, data, count * sizeof(char_type), &read_len, nullptr
                ))
        {
            auto ec = ::boost::process::detail::get_last_error();
            if (ec.value() == /*::boost::detail::winapi::*/ERROR_BROKEN_PIPE_)
                return 0;
            else
                throw std::system_error(ec, "ReadFile failed");
        }
        return read_len;
    }

    bool is_open()
    {
        return (_source != ::boost::detail::winapi::INVALID_HANDLE_VALUE_) ||
               (_sink   != ::boost::detail::winapi::INVALID_HANDLE_VALUE_);
    }

    void close()
    {
        ::boost::detail::winapi::CloseHandle(_source);
        ::boost::detail::winapi::CloseHandle(_sink);
        _source = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
        _sink   = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    }
};

template<class Char, class Traits>
basic_pipe<Char, Traits>::basic_pipe(const basic_pipe & p)
{
    auto proc = ::boost::detail::winapi::GetCurrentProcess();

    if (p._source == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        _source = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    else if (!::boost::detail::winapi::DuplicateHandle(
            proc, p._source, proc, &_source, 0,
            static_cast<::boost::detail::winapi::BOOL_>(true),
             ::boost::detail::winapi::DUPLICATE_SAME_ACCESS_))
        throw_last_error("Duplicate Pipe Failed");

    if (p._sink == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        _sink = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    else if (!::boost::detail::winapi::DuplicateHandle(
            proc, p._sink, proc, &_sink, 0,
            static_cast<::boost::detail::winapi::BOOL_>(true),
             ::boost::detail::winapi::DUPLICATE_SAME_ACCESS_))
        throw_last_error("Duplicate Pipe Failed");

}

template<class Char, class Traits>
basic_pipe<Char, Traits>::basic_pipe(const std::string & name)
{
    static constexpr int OPEN_EXISTING_         = 3; //temporary.
    static constexpr int FILE_FLAG_OVERLAPPED_  = 0x40000000; //temporary
    //static constexpr int FILE_ATTRIBUTE_NORMAL_ = 0x00000080; //temporary

    ::boost::detail::winapi::HANDLE_ source = ::boost::detail::winapi::create_named_pipe(
            name.c_str(),
            ::boost::detail::winapi::PIPE_ACCESS_INBOUND_
            | FILE_FLAG_OVERLAPPED_, //write flag
            0, 1, 8192, 8192, 0, nullptr);

    if (source == boost::detail::winapi::INVALID_HANDLE_VALUE_)
        ::boost::process::detail::throw_last_error("create_named_pipe() failed");

    ::boost::detail::winapi::HANDLE_ sink = boost::detail::winapi::create_file(
            name.c_str(),
            ::boost::detail::winapi::GENERIC_WRITE_, 0, nullptr,
            OPEN_EXISTING_,
            FILE_FLAG_OVERLAPPED_, //to allow read
            nullptr);

    if (sink == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        ::boost::process::detail::throw_last_error("create_file() failed");

    _source = source;
    _sink   = sink;
}

template<class Char, class Traits>
basic_pipe<Char, Traits>& basic_pipe<Char, Traits>::operator=(const basic_pipe & p)
{
    auto proc = ::boost::detail::winapi::GetCurrentProcess();

    if (p._source == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        _source = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    else if (!::boost::detail::winapi::DuplicateHandle(
            proc, p._source, proc, &_source, 0,
            static_cast<::boost::detail::winapi::BOOL_>(true),
             ::boost::detail::winapi::DUPLICATE_SAME_ACCESS_))
        throw_last_error("Duplicate Pipe Failed");

    if (p._sink == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        _sink = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    else if (!::boost::detail::winapi::DuplicateHandle(
            proc, p._sink, proc, &_sink, 0,
            static_cast<::boost::detail::winapi::BOOL_>(true),
             ::boost::detail::winapi::DUPLICATE_SAME_ACCESS_))
        throw_last_error("Duplicate Pipe Failed");

    return *this;
}

template<class Char, class Traits>
basic_pipe<Char, Traits>& basic_pipe<Char, Traits>::operator=(basic_pipe && lhs)
{
    if (_source == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        ::boost::detail::winapi::CloseHandle(_source);

    if (_sink == ::boost::detail::winapi::INVALID_HANDLE_VALUE_)
        ::boost::detail::winapi::CloseHandle(_sink);

    _source = lhs._source;
    _sink   = lhs._sink;
    lhs._source = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    lhs._sink   = ::boost::detail::winapi::INVALID_HANDLE_VALUE_;
    return *this;
}

template<class Char, class Traits>
inline bool operator==(const basic_pipe<Char, Traits> & lhs, const basic_pipe<Char, Traits> & rhs)
{
    return compare_handles(lhs.source(), rhs.source()) &&
           compare_handles(lhs.sink(),   rhs.sink());
}

template<class Char, class Traits>
inline bool operator!=(const basic_pipe<Char, Traits> & lhs, const basic_pipe<Char, Traits> & rhs)
{
    return !compare_handles(lhs.source(), rhs.source()) ||
           !compare_handles(lhs.sink(),   rhs.sink());
}

}}}}

#endif