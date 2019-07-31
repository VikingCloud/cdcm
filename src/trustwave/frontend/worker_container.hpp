//===========================================================================
// Trustwave ltd. @{SRCH}
//								worker_container.hpp
//
//---------------------------------------------------------------------------
// DESCRIPTION: @{HDES}
// -----------
//---------------------------------------------------------------------------
// CHANGES LOG: @{HREV}
// -----------
// Revision: 01.00
// By      : Assaf Cohen
// Date    : 18 Jul 2019
// Comments: 

#ifndef TRUSTWAVE_FRONTEND_WORKER_CONTAINER_HPP_
#define TRUSTWAVE_FRONTEND_WORKER_CONTAINER_HPP_
#include <boost/multi_index_container.hpp>           // for interpro...
#include <boost/multi_index/ordered_index.hpp>           //
#include <boost/multi_index/ranked_index.hpp>           //
#include <boost/multi_index/mem_fun.hpp>                      // for multi_index
#include <boost/multi_index/member.hpp>                      // for multi_index
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/shared_ptr.hpp>
#include <string>                                            // for string
#include <chrono>
#include <tuple>
namespace bmi = boost::multi_index;
namespace chr = std::chrono;

namespace trustwave {
//===================================================================
//                      Forward declarations
//===================================================================
//  This defines one worker, idle or active
struct worker {
    std::string identity_;   //  Address of worker
    int64_t expiry_;         //  Expires at unless heartbeat
    std::string last_worked_session_;   //
    bool idle_;

    worker(std::string identity, int64_t expiry = 0) :
                    identity_(identity), expiry_(expiry), last_worked_session_("N/A"), idle_(true)
    {
    }
};
typedef std::shared_ptr <worker> sp_worker_t;
class worker_container {
public:
    //Tags
    struct id; // worker id
    struct session; // session_id (last worked)
    struct idle_id; // session_id  and also idle
    struct idle; //is idle
    struct expiration; // by expiration
    worker_container(uint32_t heartbeat_expiry) :
                    heartbeat_expiry_(heartbeat_expiry)
    {
    }
    bool exists(std::string& identity) const
    {
        return cont_.get <id>().find(identity) != cont_.get <id>().end();
    }
    sp_worker_t get(std::string& identity) const
    {
        auto rv = cont_.get <id>().find(identity);
        if (rv != cont_.get <id>().end()){
            return *rv;
        }
        return sp_worker_t();
    }
    sp_worker_t get_next_worker() const
    {
        auto rv = cont_.get <expiration>().begin();
        if (rv != cont_.get <expiration>().end()){
            return *rv;
        }
        return sp_worker_t();
    }
    sp_worker_t get_by_last_worked_session(std::string& sess_id) const
    {

        auto rv = cont_.get <session>().find(sess_id);
        if (rv != cont_.get <session>().end()){
            return *rv;
        }
        return sp_worker_t();
    }
    void insert(sp_worker_t w)
    {
        w->expiry_ = chr::system_clock::to_time_t(chr::system_clock::now()) + heartbeat_expiry_;
        cont_.insert(w);
    }

    bool erase(std::string& identity)
    {

        auto& id_idx = cont_.get <id>();
        auto s = id_idx.find(identity);
        if (s == id_idx.end()){
            return false;
        }
        else{
            id_idx.erase(s);
            return true;
        }
    }
    bool is_idle(const std::string& identity) const
    {
        auto& idle_idx = cont_.get <idle_id>();
        return idle_idx.count(std::make_tuple(identity, true)) > 0;
    }
    bool is_busy(const std::string& identity) const
    {

        return !is_idle(identity);
    }
    bool set_idle(const std::string& identity)
    { //fixme assaf find nicer way
        auto& idle_idx = cont_.get <idle_id>();
        auto it2 = idle_idx.find(std::make_tuple(identity, false));
        if (it2 != idle_idx.end())
            return idle_idx.modify(it2, [this](sp_worker_t x){x->idle_=true;});
        else{
            auto& id_idx = cont_.get <id>();
            auto s = id_idx.find(identity);
            if (s == id_idx.end()){
                return false;
            }
        }
        return true;
    }
    bool set_busy(const std::string& identity)
    {
        //fixme assaf find nicer way
        auto& idle_idx = cont_.get <idle_id>();
        auto it2 = idle_idx.find(std::make_tuple(identity, true));
        if (it2 != idle_idx.end())
            return idle_idx.modify(it2, [this](sp_worker_t x){x->idle_=false;});
        auto& id_idx = cont_.get <id>();
        auto s = id_idx.find(identity);
        if (s == id_idx.end()){
            return false;
        }
        return true;
    }
    bool update_last_worked(const std::string& identity, const std::string& sess_id)
    {
        auto it2 = cont_.project <session>(cont_.get <id>().find(identity));
        return cont_.get <session>().modify(it2, [&sess_id,this](sp_worker_t x){x->last_worked_session_ =sess_id;});
    }
    auto idle_workers()
    {
        auto& idle_idx = cont_.get <idle>();
        return idle_idx.equal_range(true);

    }

    bool update_expiration(const std::string& identity)
    {
        auto it2 = cont_.project <expiration>(cont_.get <id>().find(identity));
        return cont_.get <expiration>().modify(it2,
                        [this](sp_worker_t x){x->expiry_ =chr::system_clock::to_time_t(chr::system_clock::now())+heartbeat_expiry_;});

    }
private:
    struct idle_ids_compare {
        bool operator()(const worker& x, const worker& y) const
        {
            return compare(x.identity_, x.idle_, y.identity_, y.idle_);
        }
        bool operator()(const std::string& x, const worker& y) const
        {
            return compare(x, y.identity_);
        }
        bool operator()(const worker& x, const std::string& y) const
        {
            return compare(x.identity_, y);
        }

        template<typename T0>
        bool operator()(const std::tuple <T0>& x, const worker& y) const
        {
            return compare(std::get <0>(x), y.identity_);
        }
        template<typename T0>
        bool operator()(const worker& x, const std::tuple <T0>& y) const
        {
            return compare(x.identity_, std::get <0>(y));
        }
        template<typename T0, typename T1>
        bool operator()(const std::tuple <T0, T1>& x, const worker& y) const
        {
            return compare(std::get <0>(x), std::get <1>(x), y.identity_, y.idle_);
        }
        template<typename T0, typename T1>
        bool operator()(const worker& x, const std::tuple <T0, T1>& y) const
        {
            return compare(x.identity_, x.idle_, std::get <0>(y), std::get <1>(y));
        }
        bool compare(const std::string& l1, const std::string& l2) const
        {
            return l1 < l2;
        }
        bool compare(const std::string& l1, bool f1, const std::string& l2, bool f2) const
        {
            if (l1 != l2)
                return l1 < l2;
            return f1 < f2;
        }

    };
    typedef bmi::multi_index_container <sp_worker_t,
                    bmi::indexed_by <
                                    bmi::ordered_unique <bmi::tag <id>,
                                                    bmi::member <worker, std::string, &worker::identity_> >,
                                    bmi::ordered_non_unique <bmi::tag <expiration>,
                                                    bmi::member <worker, int64_t, &worker::expiry_>,std::greater<int64_t> >,
                                    bmi::ordered_non_unique <bmi::tag <idle>, bmi::member <worker, bool, &worker::idle_> >,
                                    bmi::ordered_unique <bmi::tag <idle_id>, bmi::identity <worker>, idle_ids_compare>,
                                    bmi::ordered_non_unique <bmi::tag <session>,
                                                    bmi::member <worker, std::string, &worker::last_worked_session_> > > > workers_t;

    workers_t cont_;
    uint32_t heartbeat_expiry_;
};
}

#endif /* TRUSTWAVE_FRONTEND_WORKER_CONTAINER_HPP_ */
