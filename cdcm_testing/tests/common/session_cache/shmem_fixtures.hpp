//=====================================================================================================================
// VikingCloud, Inc. @{SRCH}
//														shmem_fixtures.hpp
//
//---------------------------------------------------------------------------------------------------------------------
// DESCRIPTION:
//
//
//---------------------------------------------------------------------------------------------------------------------
// By      : Assaf Cohen
// Date    : 8/27/19
// Comments:
//=====================================================================================================================
//                          						Include
//                          files
//=====================================================================================================================
#ifndef CDCM_SHMEM_FIXTURES_HPP
#define CDCM_SHMEM_FIXTURES_HPP
#include "session.hpp"
#include "sessions_cache/shared_mem_sessions_cache.hpp"
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
struct EmptyCache {
    EmptyCache()
    {
        sessions = trustwave::shared_mem_sessions_cache::get_or_create("sessions_test", 1024 * 1024 * 10, 5);
        sessions->flush_all_entries();
    }
    ~EmptyCache() {
        boost::filesystem::remove("/dev/shm/sessions_test_lock");
    }
    boost::shared_ptr<trustwave::shared_mem_sessions_cache> sessions;
};
struct OneInCache: EmptyCache {
    OneInCache()
    {
        auto s = boost::make_shared<trustwave::session>("192.168.0.1");
        s->creds("smb","WORKGROUP", "admin1", "pass1", "ws1");
        sessions->add(s);
    }
    ~OneInCache() {}
};
struct TwoInCache: OneInCache {
    TwoInCache()
    {

        auto s = boost::make_shared<trustwave::session>("192.168.0.2");
        s->creds("smb","WORKGROUP", "admin2", "pass2", "ws2");
        sessions->add(s);
    }
    ~TwoInCache() {}
};
#endif // CDCM_SHMEM_FIXTURES_HPP
