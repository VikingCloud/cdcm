#ifdef __cplusplus
extern "C" {
#endif
#include "source3/include/includes.h"
#include "rpc_common.h"
#include "../libcli/auth/netlogon_creds_cli.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "rpc_client/cli_netlogon.h"
#include "../libcli/smbreadline/smbreadline.h"
#include "../libcli/security/security.h"
#include "libsmb/libsmb.h"
#include "../libcli/smb/smbXcli_base.h"
#include "rpc_client/cli_pipe.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "client.h"

#ifdef __cplusplus
}
#endif
#include "rpc_client.hpp"
#include "session.hpp"
#undef Required
#include "singleton_runner/authenticated_scan_server.hpp"
#define Required (3)
using trustwave::rpc_client;
using trustwave::result;
//=====================================================================================================================
// VikingCloud, Inc. @{SRCH}
//														rpc_client.cpp
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
using trustwave::session;

rpc_client::~rpc_client()
{
    if(cli_ != nullptr) { cli_shutdown(cli_); }
}
result rpc_client::connect(const session& sess, const std::string& share, const std::string& device)
{
    creds_ = ::cli_credentials_init(talloc_tos());
    auto smb_creds = sess.creds("smb");
    cli_credentials_set_domain(creds_, smb_creds.domain().c_str(), CRED_SPECIFIED);
    cli_credentials_set_username(creds_, smb_creds.username().c_str(), CRED_SPECIFIED);
    cli_credentials_set_password(creds_, smb_creds.password().c_str(), CRED_SPECIFIED);
    cli_credentials_set_workstation(creds_, smb_creds.workstation().c_str(), CRED_SPECIFIED);

    NTSTATUS nt_status = dcerpc_parse_binding(talloc_tos(), sess.remote().c_str(), &binding_);

    if(!NT_STATUS_IS_OK(nt_status)) { return {false, ntstatus_to_werror(nt_status)}; }

    nt_status = dcerpc_binding_set_transport(binding_, NCACN_NP);
    if(!NT_STATUS_IS_OK(nt_status)) { return {false, ntstatus_to_werror(nt_status)}; }
    nt_status = cli_full_connection_creds(&cli_, nullptr, dcerpc_binding_get_string_option(binding_, "host"), nullptr,
                                          0, share.c_str(), device.c_str(), creds_, 0, SMB_SIGNING_IPC_DEFAULT);

    if(!NT_STATUS_IS_OK(nt_status)) {
        AU_LOG_DEBUG("Cannot connect to server. Error was %s\n", nt_errstr(nt_status));
        return {false, ntstatus_to_werror(nt_status)};
    }
    return {true, ntstatus_to_werror(nt_status)};
}
result rpc_client::open_pipe(const ndr_interface_table* table, const bool noauth)
{
    cli_set_timeout(cli_, 10000);
    NTSTATUS nt_status;
    if(noauth) {
        nt_status
            = cli_rpc_pipe_open_noauth_transport(cli_, dcerpc_binding_get_transport(binding_), table, &pipe_handle_);
    }
    else {
        nt_status = cli_rpc_pipe_open_with_creds(cli_, table, dcerpc_binding_get_transport(binding_),
                                                 DCERPC_AUTH_TYPE_NTLMSSP, DCERPC_AUTH_LEVEL_PRIVACY,
                                                 smbXcli_conn_remote_name(cli_->conn), creds_, &pipe_handle_);
    }

    if(!NT_STATUS_IS_OK(nt_status)) {
        auto err_str = nt_errstr(nt_status);
        AU_LOG_DEBUG("Could not initialise %s. Error was %s\n", table->name,
                     (err_str != nullptr) ? err_str : "Unknown");
        return {false, ntstatus_to_werror(nt_status)};
    }
    return {true, ntstatus_to_werror(nt_status)};
}
result rpc_client::connect_and_open_pipe(const session& sess, const std::string& share, const std::string& device,
                                         const ndr_interface_table* table, const bool noauth)
{
    auto rv = connect(sess, share, device);
    if(!std::get<0>(rv)) {
        AU_LOG_DEBUG("Connect failed. ");
        return rv;
    }
    return open_pipe(table, noauth);
}
cli_state* rpc_client::cli() { return cli_; }
rpc_pipe_client* rpc_client::pipe_handle() { return pipe_handle_; }