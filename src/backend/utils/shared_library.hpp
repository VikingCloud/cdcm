//=====================================================================================================================
// VikingCloud, Inc. @{SRCH}
//														shared_library.hpp
//
//---------------------------------------------------------------------------------------------------------------------
// DESCRIPTION:
//
//
//---------------------------------------------------------------------------------------------------------------------
// By      : Assaf Cohen
// Date    : 11/26/19
// Comments:
//=====================================================================================================================
//                          						Include files
//=====================================================================================================================
#ifndef _SHARED_LIBRARY_HPP
#define _SHARED_LIBRARY_HPP

#include <functional>
#include <regex>
#include <vector>
#include <boost/filesystem.hpp>
#include <dlfcn.h>

namespace trustwave {
    class shared_library final {
    public:
        shared_library(): so_(nullptr) {}

        shared_library(const shared_library& rhs) = delete;
        shared_library& operator=(shared_library) = delete;

        explicit shared_library(const boost::filesystem::path& p): shared_library() { open(p); }

        ~shared_library() { close(); }

        const std::string& filename() const noexcept { return filename_; }

        bool opened() const noexcept { return (so_ != nullptr); }

        void close() noexcept
        {
            if(opened()) {
                dlclose(so_);
                so_ = nullptr;
                filename_.clear();
            }
        }

        bool open(const boost::filesystem::path& p) noexcept
        {
            close();
            return ((so_ = open_intenal(p)) != nullptr);
        }

        bool has(const std::string& s) const noexcept { return (nullptr != so_) && dlsym(so_, s.c_str()); }

        template<typename T> std::function<T> get(const std::string& s) noexcept
        {
            std::function<T> r = nullptr;
            if(so_ != nullptr) {
                r = reinterpret_cast<T*>(dlsym(so_, s.c_str()));
            }
            return r;
        }

        void* get_raw(const std::string& s) noexcept { return so_ ? dlsym(so_, s.c_str()) : nullptr; }

        shared_library& operator=(const shared_library& rhs) = delete;

    private:
        void* open_intenal(const boost::filesystem::path& p) noexcept
        {
            const std::regex name_lib_filter("lib\\S+\\.so");
            boost::system::error_code ec;
            if(boost::filesystem::is_directory(p, ec)) {
                return nullptr;
            }
            if(!std::regex_match(p.filename().generic_string(), name_lib_filter)) {
                return nullptr;
            }
            void* ret = nullptr;
            if((ret = dlopen(p.c_str(), RTLD_GLOBAL | RTLD_LAZY))) {
                filename_ = p.generic_string();
                return ret;
            }
            std::cerr << "failed to open " << p.c_str() << ". dlerror: " << dlerror() << std::endl;
            return nullptr;
        }

    private:
        void* so_;
        std::string filename_;
    };

} // namespace trustwave

#endif //_SHARED_LIBRARY_HPP