//=====================================================================================================================
// Trustwave ltd. @{SRCH}
//														wmi_registry_client.cpp
//
//---------------------------------------------------------------------------------------------------------------------
// DESCRIPTION:
//
//
//---------------------------------------------------------------------------------------------------------------------
// By      : Assaf Cohen
// Date    : 7/29/20
// Comments:
//=====================================================================================================================
//                          						Include files
//=====================================================================================================================

#include "wmi_registry_client.hpp"
#include <Python.h>
#include <boost/filesystem.hpp>
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/tuple.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <iostream>
#include <chrono>
#include "session.hpp"
#include "base64_encode.hpp"
using trustwave::wmi_registry_client;
namespace bp = boost::python;
namespace {
    template<typename T> inline std::vector<T> to_std_vector(const boost::python::object& iterable)
    {
        return std::vector<T>(bp::stl_input_iterator<T>(iterable), bp::stl_input_iterator<T>());
    }
    struct scoped_timer
    {
        explicit scoped_timer(const std::string_view name):start_(std::chrono::high_resolution_clock::now()),name_(name)
        {}
        ~scoped_timer()
        {
            auto stop = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start_);

            std::cerr << "Time taken by "<<name_<<": "
                      << duration.count() << " microseconds" << std::endl;
        }
        decltype(std::chrono::high_resolution_clock::now()) start_;
        const std::string_view name_;
    };

    wmi_registry_client::result handle_pyerror()
    {
        std::string msg;
        if (PyErr_Occurred()) {
            using namespace boost::python;
            using namespace boost;

            PyObject *exc,*val,*tb;
            object formatted_list, formatted;
            PyErr_Fetch(&exc,&val,&tb);
            handle<> hexc(exc),hval(allow_null(val)),htb(allow_null(tb));
            object traceback(import("traceback"));
            object format_exception_only(traceback.attr("format_exception_only"));
            formatted_list = format_exception_only(hexc,hval);
            formatted = str("\n").join(formatted_list);
            msg = extract<std::string>(formatted);
        }
        bp::handle_exception();
        PyErr_Clear();
        return std::make_tuple(false,msg);
    }
} // namespace
wmi_registry_client::result wmi_registry_client::connect(const session& sess)
{
    scoped_timer t("connect");
    try {
        Py_Initialize();
        boost::filesystem::path workingDir = boost::filesystem::absolute("./").normalize();
        PyObject* sysPath = PySys_GetObject("path");
        PyList_Insert(sysPath, 0, PyUnicode_FromString(workingDir.string().c_str()));
        PySys_SetObject("path", sysPath);
        main_ = boost::python::import("__main__");
        global_ = main_.attr("__dict__");
        helper_ = boost::python::import("helper_module");
        exec_ = helper_.attr("WMI_REG_EXEC_METHOD")(sess.remote(), sess.creds().username(), sess.creds().password());
        {
            scoped_timer t("real connect");
            exec_.attr("connect")();
        }
    }
    catch(const boost::python::error_already_set&) {
        PyErr_Print();
        return std::make_tuple(false,"");
    }
    catch(...) {
        return std::make_tuple(false,"");
    }
    return std::make_tuple(true,"");
}
wmi_registry_client::result wmi_registry_client::key_exists(const std::string& key, bool& exists)
{
    scoped_timer t("key_exists");
    static constexpr std::string_view slashes("\\");
    auto pos = key.find_last_of(slashes);
    std::string parent_key;
    std::string child_key;
    if(pos == std::string::npos) {
        parent_key = std::string("");
        child_key = key;
    }
    else {
        parent_key = std::string(key.c_str(), pos);
        child_key = std::string_view(key.c_str() + pos + slashes.length());
    }
    exists = false;
    try {
        auto rr = exec_.attr("EnumKey")(parent_key);
        try {
            if(rr.contains("Error")) {
                std::string as = bp::extract<std::string>(rr["Error"]);
                return std::make_tuple(false, as);
            }
        }
        catch(...) {
            //log?
        }
        try {
            std::cerr<<"kex 1 "<<key<<std::endl;
            auto vec = to_std_vector<std::string>(rr);
            std::cerr<<"kex 2"<<key<<std::endl;

            exists = vec.end() != std::find_if(vec.begin(), vec.end(), [&child_key](const std::string& e) {
                return boost::iequals(child_key, e);
            });
            std::cerr<<"kex 3 "<<key<<std::endl;

        }
        catch(const boost::python::error_already_set&) {
            return handle_pyerror();
        }
        catch(...) {
            return std::make_tuple(false,"");
        }
    }
    catch(const boost::python::error_already_set&) {
        return handle_pyerror();
    }
    catch(...) {
        return std::make_tuple(false,"");
    }
    return std::make_tuple(true,"");
}
wmi_registry_client::result wmi_registry_client::value_exists(const std::string& key, const std::string& value, bool& exists)
{
    scoped_timer t("value_exists");
    try {
        exists = false;
        auto rr = exec_.attr("EnumValues")(key);
        try {
            if(rr.contains("Error")) {
                std::string as = bp::extract<std::string>(rr["Error"]);
                return std::make_tuple(false, as);
            }
        }
        catch(const boost::python::error_already_set&) {
            PyErr_Clear();
        }
        try {
            auto vec = to_std_vector<std::string>(rr["sNames"]);
            exists = vec.end() != std::find_if(vec.begin(), vec.end(), [&value](const std::string& e) {
                         return boost::iequals(value, e);
                     });
        }
        catch(const boost::python::error_already_set&) {
            return handle_pyerror();
        }
        catch(...) {
            return std::make_tuple(false,"");
        }
    }
    catch(const boost::python::error_already_set&) {
        return handle_pyerror();
    }
    catch(...) {
        return std::make_tuple(false,"");
    }
    return std::make_tuple(true,"");
}

wmi_registry_client::result wmi_registry_client::enumerate_key(const std::string& key, enum_key& ek)
{
    scoped_timer t("enumerate_key");
    try {
        std::cerr<<"ek 1 "<<key<<std::endl;
        auto rr = exec_.attr("EnumKey")(key);
        std::cerr<<"ek 2 "<<key<<std::endl;
        std::vector<std::string> vec;
        try {

        if(rr.contains("Error"))
        {
            std::cerr<<"ek 3 "<<key<<std::endl;

            std::string as=bp::extract<std::string>(rr["Error"]);
            return std::make_tuple(false,as);
        }
        std::cerr<<"ek 4 "<<key<<std::endl;


        std::cerr<<"ek 5 "<<key<<std::endl;

            std::cerr<<"ek 6 "<<key<<std::endl;

            if(!rr.is_none())
            vec = to_std_vector<std::string>(rr);
            std::cerr<<"ek 7 "<<key<<std::endl;

        }
        catch(const boost::python::error_already_set&) {
            std::cerr<<"ek catch"<<std::endl;
            PyErr_Clear();

            // no subkeys maybe values therefore will not return
        }
        catch(...) {
            return std::make_tuple(false,"");
        }

        for(auto e: vec) { ek.sub_keys_.push_back(sub_key(e)); }
        std::cerr<<"ek -> ekv "<<key<<std::endl;
        auto ekv_res = enumerate_key_values(key, ek.registry_values_);
        if(!std::get<0>(ekv_res))
        {
            std::cerr<<"ekv -> ekv failed: "<<key<<std::endl;
            return ekv_res;
        }
        std::cerr<<"ek -> ekv done: "<<key<<std::endl;

        return std::make_tuple(true,"");
    }
    catch(const boost::python::error_already_set&) {
        return handle_pyerror();
    }
    catch(...) {
        std::cerr<<"ek catch 2"<<std::endl;
        return std::make_tuple(false,"");
    }
    return std::make_tuple(false,"");
}
wmi_registry_client::result wmi_registry_client::enumerate_key_values(const std::string& key, enum_key_values& ev)
{
    scoped_timer t("enumerate_key_values");

    try {
        std::cerr<<"ekv 1 "<<key<<std::endl;

        auto rr = exec_.attr("EnumValues")(key);
        std::cerr<<"ekv 2 "<<key<<std::endl;
try {
            if(rr.contains("Error")) {
                std::string as = bp::extract<std::string>(rr["Error"]);
                return std::make_tuple(false, as);
            }
        }
        catch(const boost::python::error_already_set&) {
            std::cerr<<"ekv catch"<<std::endl;
            PyErr_Clear();

        }
        std::cerr<<"ekv 3 "<<key<<std::endl;

        std::vector<std::string> vs;
        std::vector<uint32_t> vt;
        try {
            std::cerr<<"ekv 3.5 "<<key<<std::endl;

            vs = to_std_vector<std::string>(rr["sNames"]);
            std::cerr<<"ekv 4 "<<key<<std::endl;

            vt = to_std_vector<uint32_t>(rr["Types"]);
            std::cerr<<"ekv 5 "<<key<<std::endl;

        }
        catch(const boost::python::error_already_set&) {
            return handle_pyerror();
        }
        for(size_t i = 0; i < vs.size(); ++i) {
            registry_value rv(vt[i], "", vs[i]);
            std::cerr <<"before ik "<<vs[i]<<":"<<vt[i]<<std::endl;
            internal_key_get_value_by_name(key, vs[i], rv);
            ev.push_back(rv);
        }
    }
    catch(const boost::python::error_already_set&) {
        return handle_pyerror();
    }
    catch(...) {
        return std::make_tuple(false,"");
    }
    std::cerr <<"realy done "<<std::endl;

    return std::make_tuple(true,"");
}

wmi_registry_client::result wmi_registry_client::key_get_value_by_name(const std::string& key, const std::string& value, registry_value& rv)
{
    scoped_timer t("key_get_value_by_name");
    std::cerr<<"kv 1 "<<key<<std::endl;
    enum_key_values ekv;
    result ekv_res;
    try {
        std::cerr<<"kv 2 "<<key<<std::endl;

        ekv_res = enumerate_key_values(key, ekv);
        std::cerr<<"kv 3 "<<key<<std::endl;

    }
    catch(const boost::python::error_already_set&) {
        std::cerr<<"kv 4 "<<key<<std::endl;

        return handle_pyerror();
    }
    if(std::get<0>(ekv_res)) {
        std::cerr<<"kv 5 "<<key<<std::endl;

        auto it = std::find_if(ekv.begin(), ekv.end(),
                               [&value](const registry_value& el) { return boost::iequals(el.name_, value); });
        if(it != ekv.end()) {
            rv.type(it->type());
            rv.name(it->name());
            rv.value(it->value());
            return std::make_tuple(true,"");
        }
        // value doesnt exist
        return std::make_tuple(false,"");
    }

    return ekv_res;
}
wmi_registry_client::result wmi_registry_client::internal_key_get_value_by_name(const std::string& key, const std::string& value,
                                                         registry_value& rv)
{
    scoped_timer t("internal_key_get_value_by_name");
    static const std::unordered_map<uint32_t, std::string_view> type_to_method
        = {{REG_SZ, "GetStringValue"},   {REG_EXPAND_SZ, "GetExpandedStringValue"}, {REG_BINARY, "GetBinaryValue"},
           {REG_DWORD, "GetDWORDValue"}, {REG_MULTI_SZ, "GetMultiStringValue"},     {REG_QWORD, "GetQWORDValue"}};
    try {
        std::cerr <<" ik "<<key<<" "<<value<<" "<<rv.type()<<" "<<rv.name()<<std::endl;

        auto rr = exec_.attr(type_to_method.at(rv.type()).data())(key, value);

        switch(rv.type()) {
            case REG_BINARY: {
                auto bin_data = to_std_vector<uint8_t>(rr);
                auto c64_str = base64_encode(reinterpret_cast<char*>(bin_data.data()), bin_data.size());
                rv.value(c64_str);
            } break;
            case REG_DWORD: {
                std::stringstream stream;
                stream << "0x" << std::hex << bp::extract<uint32_t>(rr);
                rv.value(stream.str());
            } break;
            case REG_QWORD: {
                std::stringstream stream;
                stream << "0x" << std::hex << bp::extract<uint64_t>(rr);
                rv.value(stream.str());
            } break;
            case REG_SZ:
            case REG_EXPAND_SZ:
            case REG_MULTI_SZ: {
                rv.value(bp::extract<std::string>(rr));
            } break;
            default: {
                std::cerr << "in default";
                return std::make_tuple(false,"");
            }
        }
        return std::make_tuple(true,"");
    }
    catch(const boost::python::error_already_set&) {
        std::cerr<<"ik 1 "<<key<<std::endl;
        return handle_pyerror();
    }
    catch(...) {
        return std::make_tuple(false,"");
    }
}
