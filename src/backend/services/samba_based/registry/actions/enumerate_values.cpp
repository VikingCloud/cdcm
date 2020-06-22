//
// Created by rfrenkel on 6/9/2020.
//

//=====================================================================================================================
//                                                  Include files
//=====================================================================================================================
#include "enumerate_values.hpp"

#include "protocol/msg_types.hpp"
#include "session.hpp"
#include "singleton_runner/authenticated_scan_server.hpp"
#include "../registry_client.hpp"
#include "../registry_value.hpp"

//=====================================================================================================================
//                                                  namespaces
//=====================================================================================================================

namespace tao ::json {

    template<>
    struct traits<trustwave::registry_value>:
        binding::object<TAO_JSON_BIND_REQUIRED("name", &trustwave::registry_value::name_),
                        TAO_JSON_BIND_REQUIRED("type", &trustwave::registry_value::type_),
                        TAO_JSON_BIND_REQUIRED("value", &trustwave::registry_value::value_)> {
    };

    template<>
    struct traits<trustwave::enum_key_values_ver1>:
        binding::object<TAO_JSON_BIND_REQUIRED("registry_values", &trustwave::enum_key_values_ver1::registry_values_)> {
    };

    template<>
    struct traits<trustwave::enum_key_values_ver2>:
        binding::object<TAO_JSON_BIND_REQUIRED("registry_values", &trustwave::enum_key_values_ver2::registry_values_)> {
    };

} // namespace tao::json

using trustwave::Enumerate_Registry_Values_Action;
using action_status = trustwave::Action_Base::action_status;
action_status Enumerate_Registry_Values_Action::act(boost::shared_ptr<session> sess, std::shared_ptr<action_msg> action,
                                                    std::shared_ptr<result_msg> res)
{
    if(!sess || (sess && sess->id().is_nil())) {
        res->res("Error: Session not found");
        return action_status::FAILED;
    }

    auto c = trustwave::registry_client();

    auto ekact = std::dynamic_pointer_cast<reg_action_enumerate_registry_values_msg>(action);
    if(!ekact) {
        AU_LOG_ERROR("Failed dynamic cast");
        res->res("Error: Internal error");
        return action_status::FAILED;
    }
    if(ekact->key_.empty()) {
        res->res("Error: key is mandatory");
        return action_status::FAILED;
    }
    result r = c.connect(*sess);
    if(!std::get<0>(r)) {
        AU_LOG_DEBUG("Failed connecting to %s err: ", sess->remote().c_str(), win_errstr(std::get<1>(r)));
        if(werr_pipe_busy == std::get<1>(r).w) {
            res->res(std::string("Error: ") + std::string(win_errstr(std::get<1>(r))));
            return action_status::POSTPONED;
        }
        res->res(std::string("Error: ") + std::string(win_errstr(std::get<1>(r))));
        return action_status::FAILED;
    }

    // version 1  - as we currently behave, return the value, type and data
    // trustwave::enum_key_values_ver1 ek{};
    // auto ret = c.enumerate_key_values_ver1(ekact->key_, ek);

    // version 2  - as pm defined, only values as string
    trustwave::enum_key_values_ver2 ek{};
    auto ret = c.enumerate_key_values_ver2(ekact->key_, ek);

    if(std::get<0>(ret)) { res->res(ek); }
    else {
        auto status = werror_to_ntstatus(std::get<1>(ret));
        AU_LOG_DEBUG("%s", nt_errstr(status));
        res->res(std::string("Error: ") + nt_errstr(status));
    }
    return action_status::SUCCEEDED;
}

// instance of the our plugin
static std::shared_ptr<Enumerate_Registry_Values_Action> instance = nullptr;

// extern function, that declared in "action.hpp", for export the plugin from dll
std::shared_ptr<trustwave::Action_Base> import_action()
{
    return instance ? instance : (instance = std::make_shared<Enumerate_Registry_Values_Action>());
}