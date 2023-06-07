//=====================================================================================================================
// VikingCloud, Inc. @{SRCH}
//														authenticated_scan_server.hpp
//
//---------------------------------------------------------------------------------------------------------------------
// DESCRIPTION: @{HDES}
// -----------
//---------------------------------------------------------------------------------------------------------------------
// CHANGES LOG: @{HREV}
// -----------
// Revision: 01.00
// By      : Assaf Cohen
// Date    : 19 May 2019
// Comments:

#ifndef TRUSTWAVE_COMMON_SINGLETON_RUNNER_AUTHENTICATED_SCAN_SERVER_HPP_
#define TRUSTWAVE_COMMON_SINGLETON_RUNNER_AUTHENTICATED_SCAN_SERVER_HPP_
//=====================================================================================================================
//                          						Include files
//=====================================================================================================================
#include <boost/asio.hpp>
#include "settings.hpp"
#include "action.hpp"
#include "configurable.hpp"
#include "configuration.hpp"
#include "typedefs.hpp"
#include "dispatcher.hpp"
#include "Logger/include/Logger.h"
#include "sessions_cache/shared_mem_sessions_cache.hpp"
#include "client.hpp"
#include "process_specific_object.hpp"
//=====================================================================================================================
//                          						namespaces
//=====================================================================================================================

namespace trustwave {
    class authenticated_scan_server final: public configurable<cdcm_settings>{
    public:
        [[nodiscard]] Dispatcher<Action_Base>& public_dispatcher();
        [[nodiscard]] Dispatcher<trustwave::cdcm_client>& get_clients_dispatcher();
        [[nodiscard]] Dispatcher<trustwave::process_specific_object>& process_specific_repository();
        boost::asio::io_service& io_context();
        Dispatcher<configuration> service_conf_repository;
        boost::shared_ptr<shared_mem_sessions_cache> sessions;
        virtual ~authenticated_scan_server() = default;
        authenticated_scan_server(const authenticated_scan_server&) = delete;
        authenticated_scan_server& operator=(const authenticated_scan_server&) = delete;
        authenticated_scan_server(authenticated_scan_server&&) = delete;
        authenticated_scan_server& operator=(authenticated_scan_server&&) = delete;
        std::shared_ptr<cdcm_settings> settings() { return conf_; }
        static auto& instance()
        {
            static authenticated_scan_server app;
            return app;
        }
        ILogger* logger() { return logger_ptr_; }
        template<typename T> int run_as(size_t instance_id = 0);
        boost::shared_ptr<session> get_session(const std::string& session_id);

    private:
#undef uint_t
        authenticated_scan_server();
        ILogger* logger_ptr_;
        Dispatcher<Action_Base> public_dispatcher_;
        Dispatcher<process_specific_object> process_specific_repository_;
//        std::map<std::string,std::unique_ptr<clients_map>> session_to_clients_;
        boost::asio::io_service ios_;
    };

} // namespace trustwave
#include "log_macros.hpp"

#endif /* TRUSTWAVE_COMMON_SINGLETON_RUNNER_AUTHENTICATED_SCAN_SERVER_HPP_ */
