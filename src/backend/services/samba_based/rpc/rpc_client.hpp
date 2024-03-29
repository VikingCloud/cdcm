//=====================================================================================================================
// VikingCloud, Inc. @{SRCH}
//														rpc_client.hpp
//
//---------------------------------------------------------------------------------------------------------------------
// DESCRIPTION:
//
//
//---------------------------------------------------------------------------------------------------------------------
// By      : Assaf Cohen
// Date    : 6/3/20
// Comments:
//=====================================================================================================================
//                          						Include files
//=====================================================================================================================
#ifndef SRC_BACKEND_SERVICES_RPC_RPC_CLIENT_HPP
#define SRC_BACKEND_SERVICES_RPC_RPC_CLIENT_HPP
//=====================================================================================================================
//                          						Include files
//=====================================================================================================================
#ifdef __cplusplus
extern "C" {
#endif
#include "libcli/util/error.h"
#include "lib/talloc/talloc.h"
#ifdef __cplusplus
}
#endif
#undef uint_t
#include <string>
#include <tuple>
#include "client.hpp"
#include "configurable.hpp"
struct cli_state;
struct cli_credentials;
struct rpc_pipe_client;
struct ndr_interface_table;
struct dcerpc_binding;

//=====================================================================================================================
//                          						namespaces
//=====================================================================================================================
namespace trustwave {
    class session;
    using result = std::tuple<bool, WERROR>;
    class rpc_client final {
    public:
        // fixme assaf add copy ctor move ......
        rpc_client()=default;
        ~rpc_client();
        rpc_client(rpc_client const& other) = delete;
        rpc_client& operator=(rpc_client other) noexcept {
            swap(*this, other);
            return *this;
        }
        friend void swap(rpc_client& a,rpc_client& b) noexcept
        {
            using std::swap;
            swap(a.cli_,b.cli_);
            swap(a.creds_,b.creds_);
            swap(a.pipe_handle_,b.pipe_handle_);
            swap(a.binding_,b.binding_);
        }
        result connect(const session& sess, const std::string& share, const std::string& device);
        result connect_and_open_pipe(const session& sess, const std::string& share, const std::string& device,
                                     const ndr_interface_table* table, const bool noauth = false);
        cli_state* cli();
        rpc_pipe_client* pipe_handle();

    private:
        result open_pipe(const ndr_interface_table* table, const bool noauth);
        cli_state* cli_ = nullptr;
        cli_credentials* creds_ = nullptr;
        rpc_pipe_client* pipe_handle_ = nullptr;
        dcerpc_binding* binding_ = nullptr;
    };
} // namespace trustwave

#endif // SRC_BACKEND_SERVICES_RPC_RPC_CLIENT_HPP