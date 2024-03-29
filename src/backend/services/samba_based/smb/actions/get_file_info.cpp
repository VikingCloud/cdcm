//=====================================================================================================================
// VikingCloud, Inc. @{SRCH}
//														get_file_info.cpp
//
//---------------------------------------------------------------------------------------------------------------------
// DESCRIPTION:
//
//
//---------------------------------------------------------------------------------------------------------------------
// By      : Assaf Cohen
// Date    : 11/27/19
// Comments:
//=====================================================================================================================
//                          						Include files
//=====================================================================================================================
#include "get_file_info.hpp"
#include <codecvt>
#include <regex>
#include <boost/algorithm/string/replace.hpp>
#include "taocpp-json/include/tao/json/contrib/traits.hpp"
#include "../smb_client.hpp"
#include "session.hpp"
#include "singleton_runner/authenticated_scan_server.hpp"
#include "pe_context.hpp"

using trustwave::SMB_Get_File_Info;
using action_status = trustwave::Action_Base::action_status;

static void pe_hdr_populate_json(const std::map<std::u16string, std::u16string>& pe_values,
                                 tao::json::events::to_value& json_blob)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;

    for (const auto& e : pe_values) {
        auto key = convert.to_bytes(std::u16string(e.first));
        auto val = convert.to_bytes(std::u16string(e.second));

        if (key == "FileVersion") { // fix for CDCM-66
            static const std::regex re("([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)\\s+.*");
            std::smatch what;

            if (std::regex_match(val, what, re)) {
                val = what.str(1);
            }
        }

        json_blob.key(key);
        json_blob.string(val.empty() ? std::string("NULL") : val);
        json_blob.member();
    }
}

static void file_info_populate_json(const trustwave::smb_client& smb_cli,
                                    tao::json::events::to_value& json_blob)
{
    json_blob.key("size");
    json_blob.string(std::to_string(smb_cli.file_size()));
    json_blob.member();

    json_blob.key("path");
    json_blob.string("NULL");
    json_blob.member();

    json_blob.key("last_modified");
    json_blob.string(std::to_string(smb_cli.last_modified()));
    json_blob.member();
}

action_status SMB_Get_File_Info::act(boost::shared_ptr<session> sess, std::shared_ptr<action_msg> action,
                                     std::shared_ptr<result_msg> res)
{
    if (!sess || sess->id().is_nil()) {
        res->set_response_for_error(CDCM_ERROR::SESSION_NOT_FOUND);
        return action_status::FAILED;
    }

    auto smb_action = std::dynamic_pointer_cast<smb_get_file_info_msg>(action);
    if (!smb_action) {
        AU_LOG_ERROR("Failed dynamic cast");
        res->set_response_for_error(CDCM_ERROR::INTERNAL_ERROR);
        return action_status::FAILED;
    }
    if (smb_action->param.empty()) {
        res->set_response_for_error(CDCM_ERROR::PATH_IS_MANDATORY);
        return action_status::FAILED;
    }
    std::string path = boost::replace_all_copy(smb_action->param, "\\", "/");
    std::string base("smb://");
    base.append(sess->remote()).append("/").append(path);

    trustwave::smb_client rc;
    auto connect_res = rc.open_file(base.c_str());
    if (!connect_res.first) {
        res->set_response_for_error_with_unique_code_or_msg(CDCM_ERROR::GENERAL_ERROR_WITH_ASSET,
            connect_res.second, std::string((std::strerror(connect_res.second))));
        return action_status::FAILED;
    }

    tao::json::events::to_value c;
    c.begin_object();

    file_info_populate_json(rc, c);

    pe_context pc(rc);

    if (pc.parse() == 0) {
        std::map<std::u16string, std::u16string> ret;
        static const std::unordered_set<std::u16string> s = {u"CompanyName", u"FileDescription",
            u"FileVersion", u"ProductName", u"ProductVersion"};

        pc.extract_info(ret, s);
        pe_hdr_populate_json(ret, c);
    }

    c.end_object();
    res->set_response_for_success(c.value);

    return action_status::SUCCEEDED;
}

static std::shared_ptr<SMB_Get_File_Info> instance = nullptr;

// extern function, that declared in "action.hpp", for export the plugin from dll
std::shared_ptr<trustwave::Action_Base> import_action()
{
    return instance ? instance : (instance = std::make_shared<SMB_Get_File_Info>());
}
