// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_PROCESS_DETAIL_POSIX_BASIC_CMD_HPP_
#define BOOST_PROCESS_DETAIL_POSIX_BASIC_CMD_HPP_

#include <boost/process/detail/posix/handler.hpp>
#include <boost/process/detail/posix/cmd.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <string>
#include <vector>

namespace boost
{
namespace process
{
namespace detail
{
namespace posix
{

inline std::vector<std::string>  build_args(std::vector<std::string> && data)
{
    std::vector<std::string>  st;
    
    st.reserve(data.size());
    for (auto & arg : data)
    {
        //don't need the argument afterwards so,
        boost::trim(arg);

        if ((arg.front() != '"') && (arg.back() != '"'))
        {
            auto it = std::find(arg.begin(), arg.end(), ' ');//contains space?
            if (it != arg.end())//ok, contains spaces.
            {
                //the first one is put directly onto the output,
                //because then I don't have to copy the whole string
                arg.insert(arg.begin(), '"');
                arg += '"'; //thats the post one.
            }
        }
        st.push_back(std::move(arg));
    }

    return st;
}



struct exe_cmd_init : boost::process::detail::api::handler_base_ext
{
    exe_cmd_init(const exe_cmd_init & ) = delete;
    exe_cmd_init(exe_cmd_init && ) = default;
    exe_cmd_init(std::string && exe, std::vector<std::string> && args)
            : exe(std::move(exe)), args(api::build_args(std::move(args))) {};
    template <class Executor>
    void on_setup(Executor& exec) 
    {
        if (exe.empty())
            exec.exe = args.front().c_str();
        else
            exec.exe = &exe.front();


        if (!args.empty())
        {
            cmd_impl = make_cmd();
            exec.cmd_line = cmd_impl.data();
        }



    }
    static exe_cmd_init exe_args(std::string && exe, std::vector<std::string> && args) {return exe_cmd_init(std::move(exe), std::move(args));}
    static exe_cmd_init cmd     (std::string && cmd) {return exe_cmd_init("", {std::move(cmd)});}
private:
	std::vector<char*> make_cmd();
    std::string exe;
    std::vector<std::string> args;
    std::vector<char*> cmd_impl;
};

std::vector<char*> exe_cmd_init::make_cmd()
{
	std::vector<char*> vec;
	if (!exe.empty())
		vec.push_back(&exe.front());

	for (auto & v : args)
		vec.push_back(&v.front());

	vec.push_back(nullptr);

	return vec;
}


}}}}

#endif