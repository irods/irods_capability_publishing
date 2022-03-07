
#define IRODS_IO_TRANSPORT_ENABLE_SERVER_SIDE_API

#include <irods/irods_query.hpp>
#include <irods/irods_re_plugin.hpp>
#include <irods/irods_re_ruleexistshelper.hpp>
#include "utilities.hpp"
#include "plugin_specific_configuration.hpp"
#include "configuration.hpp"
#include <irods/dstream.hpp>
#include <irods/rsModAVUMetadata.hpp>
#include <irods/irods_hasher_factory.hpp>
#include <irods/MD5Strategy.hpp>

#define IRODS_FILESYSTEM_ENABLE_SERVER_SIDE_API
#include <irods/transport/default_transport.hpp>
#include <irods/filesystem.hpp>

#include "cpr/response.h"
#include "cpr/session.h"
#include "cpr/cpr.h"

#include <boost/any.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <sstream>
#include <algorithm>

namespace {
    struct configuration : irods::publishing::configuration {
        std::vector<std::string> hosts_;
        configuration(const std::string& _instance_name) :
            irods::publishing::configuration(_instance_name) {
            try {
                auto cfg = irods::publishing::get_plugin_specific_configuration(_instance_name);
                if(cfg.find("hosts") != cfg.end()) {
                    std::vector<boost::any> host_list = boost::any_cast<std::vector<boost::any>>(cfg.at("hosts"));
                    for( auto& i : host_list) {
                        hosts_.push_back(boost::any_cast<std::string>(i));
                    }
                }
            }
            catch(const boost::bad_any_cast& _e) {
                THROW(
                    INVALID_ANY_CAST,
                    _e.what());
            }
        }// ctor
    }; // configuration

    std::unique_ptr<configuration> config;
    std::string object_publish_policy;
    std::string object_purge_policy;
    std::string collection_publish_policy;
    std::string collection_purge_policy;

    std::string get_api_token_for_user(
        rsComm_t*         _comm,
        const std::string _user_name) {

        std::string query_str{
            boost::str(boost::format(
            "SELECT META_USER_ATTR_VALUE where USER_NAME = '%s' and META_USER_ATTR_NAME = '%s'")
            % _user_name
            % config->api_token)};
        irods::query qobj{_comm, query_str, 1};
        return qobj.front()[0];

    } // get_api_token_for_user

    void apply_persistent_identifier_policy(
        ruleExecInfo_t*    _rei,
        const std::string& _object_path,
        std::string*       _pid) {

        std::list<boost::any> args;
        args.push_back(boost::any(_object_path));
        args.push_back(boost::any(_pid));
        std::string policy_name = irods::publishing::policy::compose_policy_name(
                                  irods::publishing::policy::prefix,
                                  "persistent_identifier_dataworld");
        irods::publishing::invoke_policy(_rei, policy_name, args);

    } // apply_persistent_identifier_policy
#if 0
    std::string generate_id() {
        using namespace boost::archive::iterators;
        std::stringstream os;
        typedef
            base64_from_binary< // convert binary values to base64 characters
                transform_width<// retrieve 6 bit integers from a sequence of 8 bit bytes
                    const char *,
                    6,
                    8
                >
            >
            base64_text; // compose all the above operations in to a new iterator

        boost::uuids::uuid uuid{boost::uuids::random_generator()()};
        std::string uuid_str = boost::uuids::to_string(uuid);
        std::copy(
            base64_text(uuid_str.c_str()),
            base64_text(uuid_str.c_str() + uuid_str.size()),
            ostream_iterator<char>(os));

        return os.str();
    } // generate_id
#endif

    std::string create_dataset(
        const std::string& _object_path,
        const std::string& _user_name,
        const std::string& _api_token) {
        namespace fs   = irods::experimental::filesystem;
        namespace fsvr = irods::experimental::filesystem::server;
        using json = nlohmann::json;

        fs::path object_path{_object_path};
        auto data_name{object_path.object_name()};

        const std::string auth_string{"Bearer " + _api_token};

        const std::string data_set_title{data_name.string()};
        const std::string data_set_visibility{"OPEN"}; // config param :: OPEN vs PRIVATE

        std::string data_set_id{};

        const std::string url{
            boost::str(boost::format("https://api.data.world/v0/datasets/%s")
            % _user_name)};

        nlohmann::json payload;
        payload["title"] = data_set_title;
        payload["visibility"] = data_set_visibility;

        auto r = cpr::Post(
                     cpr::Url{url},
                     cpr::Body{payload.dump()},
                     cpr::Header{
                         {"Content-Type", "application/json"},
                         {"Authorization", auth_string}});
        auto response = json::parse(r.text);
        if(200 != r.status_code) {
            THROW(
                SYS_INTERNAL_ERR,
                r.text);
        }

        rodsLog(
            config->log_level,
            "return code [%d] status [%s] url [%s]",
            r.status_code,
            r.text.c_str(),
            r.url.c_str());

        // we need to parse the response json, and get the data set id from the uri
        std::string uri{response["uri"]};
        boost::filesystem::path p{uri};
        data_set_id = p.filename().string();

        return data_set_id;

    } // create_dataset

    void upload_file(
        const std::string& _user_name,
        const std::string& _data_set_id,
        const std::string& _api_token,
        const std::string& _object_path,
        const char*        _data,
        const uintmax_t    _size) {
        const std::string auth_string{"Bearer " + _api_token};
        namespace fs = irods::experimental::filesystem;
        fs::path object_path{_object_path};
        auto data_name{object_path.object_name()};
        const std::string url{
            boost::str(boost::format("https://api.data.world/v0/uploads/%s/%s/files/%s")
            % _user_name
            % _data_set_id
            % data_name.string())};
        auto r = cpr::Put(
                     cpr::Url{url},
                     cpr::Body{_data, _size},
                     cpr::Header{
                         {"Authorization", auth_string},
                         {"Content-Type", "application/octet-stream"}});
        if(200 != r.status_code) {
            THROW(
                SYS_INTERNAL_ERR,
                r.text);
        }

        rodsLog(
            config->log_level,
            "return code [%d] status [%s] url [%s]",
            r.status_code,
            r.text.c_str(),
            r.url.c_str());

    } // upload_file

    void invoke_publish_object_policy(
        ruleExecInfo_t*    _rei,
        const std::string& _object_path,
        const std::string& _user_name,
        const std::string& _publish_type) {
        namespace fsvr = irods::experimental::filesystem::server;

        try {
            std::string pid{"persistent_identifier"};
            /*apply_persistent_identifier_policy(
                _rei,
                _object_path,
                _source_resource,
                &pid);*/

            const auto api_token{get_api_token_for_user(_rei->rsComm, _user_name)};
            auto data_set_id = create_dataset(
                                   _object_path,
                                   _user_name,
                                   api_token);

            // read the data out of irods into a buffer
            auto object_size = fsvr::data_object_size(*_rei->rsComm, _object_path);
            irods::experimental::io::server::basic_transport<char> xport(*_rei->rsComm);
            irods::experimental::io::idstream ds{xport, _object_path};
            const uintmax_t read_size{object_size};
            char read_buff[read_size];
            ds.read(read_buff, read_size);

            upload_file(
                _user_name,
                data_set_id,
                api_token,
                _object_path,
                read_buff,
                read_size);
        }
        catch(const std::runtime_error& _e) {
            rodsLog(
                LOG_ERROR,
                "Exception [%s]",
                _e.what());
            THROW(
                SYS_INTERNAL_ERR,
                _e.what());
        }
        catch(const std::exception& _e) {
            rodsLog(
                LOG_ERROR,
                "Exception [%s]",
                _e.what());
            THROW(
                SYS_INTERNAL_ERR,
                _e.what());
        }
    } // invoke_publish_object_policy

    void invoke_publish_collection_policy(
        ruleExecInfo_t*    _rei,
        const std::string& _collection_name,
        const std::string& _user_name,
        const std::string& _publish_type) {
        namespace fsvr = irods::experimental::filesystem::server;

        try {
            const auto api_token{get_api_token_for_user(_rei->rsComm, _user_name)};
            auto data_set_id = create_dataset(
                                   _collection_name,
                                   _user_name,
                                   api_token);

            rsComm_t& comm = *_rei->rsComm;
            namespace fs   = irods::experimental::filesystem;
            namespace fsvr = irods::experimental::filesystem::server;
            for(auto p : fsvr::recursive_collection_iterator(comm, _collection_name)) {
                try {
                    if(fsvr::is_data_object(comm, p.path())) {
                        // read the data out of irods into a buffer
                        auto object_size = fsvr::data_object_size(*_rei->rsComm, p.path().string());
                        irods::experimental::io::server::basic_transport<char> xport(*_rei->rsComm);
                        irods::experimental::io::idstream ds{xport, p.path().string()};
                        const uintmax_t read_size{object_size};
                        char read_buff[read_size];
                        ds.read(read_buff, read_size);

                        upload_file(
                            _user_name,
                            data_set_id,
                            api_token,
                            p.path().string(),
                            read_buff,
                            read_size);
                    }
                }
                catch(const irods::exception& _e) {
                    rodsLog(
                        LOG_ERROR,
                        "failed to find indexing resource for object [%s]",
                        p.path().string().c_str());
                }

            } // for
        }
        catch(const std::runtime_error& _e) {
            rodsLog(
                LOG_ERROR,
                "Exception [%s]",
                _e.what());
            THROW(
                SYS_INTERNAL_ERR,
                _e.what());
        }
        catch(const std::exception& _e) {
            rodsLog(
                LOG_ERROR,
                "Exception [%s]",
                _e.what());
            THROW(
                SYS_INTERNAL_ERR,
                _e.what());
        }
    } // invoke_publish_object_policy

    void invoke_purge_object_policy(
        ruleExecInfo_t*    _rei,
        const std::string& _object_path,
        const std::string& _source_resource,
        const std::string& _publish_type) {

        try {
            // smart things here
        }
        catch(const std::runtime_error& _e) {
            rodsLog(
                LOG_ERROR,
                "Exception [%s]",
                _e.what());
            THROW(
                SYS_INTERNAL_ERR,
                _e.what());
        }
        catch(const std::exception& _e) {
            rodsLog(
                LOG_ERROR,
                "Exception [%s]",
                _e.what());
            THROW(
                SYS_INTERNAL_ERR,
                _e.what());
        }
    } // invoke_purge_object_policy

} // namespace

irods::error start(
    irods::default_re_ctx&,
    const std::string& _instance_name ) {
    RuleExistsHelper::Instance()->registerRuleRegex("irods_policy_.*");
    config = std::make_unique<configuration>(_instance_name);
    object_publish_policy = irods::publishing::policy::compose_policy_name(
                               irods::publishing::policy::object::publish,
                               "dataworld");
    object_purge_policy = irods::publishing::policy::compose_policy_name(
                               irods::publishing::policy::object::purge,
                               "dataworld");
    collection_publish_policy = irods::publishing::policy::compose_policy_name(
                               irods::publishing::policy::collection::publish,
                               "dataworld");
    collection_purge_policy = irods::publishing::policy::compose_policy_name(
                               irods::publishing::policy::collection::purge,
                               "dataworld");
    return SUCCESS();
}

irods::error stop(
    irods::default_re_ctx&,
    const std::string& ) {
    return SUCCESS();
}

irods::error rule_exists(
    irods::default_re_ctx&,
    const std::string& _rn,
    bool&              _ret) {
    _ret = object_publish_policy     == _rn ||
           object_purge_policy       == _rn ||
           collection_publish_policy == _rn ||
           collection_purge_policy   == _rn;
    return SUCCESS();
}

irods::error list_rules(
    irods::default_re_ctx&,
    std::vector<std::string>& _rules) {
    _rules.push_back(object_publish_policy);
    _rules.push_back(object_purge_policy);
    _rules.push_back(collection_publish_policy);
    _rules.push_back(collection_purge_policy);
    return SUCCESS();
}

irods::error exec_rule(
    irods::default_re_ctx&,
    const std::string&     _rn,
    std::list<boost::any>& _args,
    irods::callback        _eff_hdlr) {
    ruleExecInfo_t* rei{};
    const auto err = _eff_hdlr("unsafe_ms_ctx", &rei);
    if(!err.ok()) {
        return err;
    }

    try {
        if(_rn == object_publish_policy) {
            auto it = _args.begin();
            const std::string object_path{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string user_name{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string publish_type{ boost::any_cast<std::string>(*it) }; ++it;

            invoke_publish_object_policy(
                rei,
                object_path,
                user_name,
                publish_type);
        }
        else if(_rn == object_purge_policy) {
            auto it = _args.begin();
            const std::string object_path{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string user_name{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string publish_type{ boost::any_cast<std::string>(*it) }; ++it;

            invoke_purge_object_policy(
                rei,
                object_path,
                user_name,
                publish_type);
        }
        else if(_rn == collection_publish_policy) {
            auto it = _args.begin();
            const std::string collection_name{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string user_name{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string publish_type{ boost::any_cast<std::string>(*it) }; ++it;

            invoke_publish_collection_policy(
                rei,
                collection_name,
                user_name,
                publish_type);

        }
        else if(_rn == collection_purge_policy) {
            auto it = _args.begin();
            const std::string collection_name{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string user_name{ boost::any_cast<std::string>(*it) }; ++it;
            const std::string publish_type{ boost::any_cast<std::string>(*it) }; ++it;
        }
        else {
            return ERROR(
                    SYS_NOT_SUPPORTED,
                    _rn);
        }
    }
    catch(const std::invalid_argument& _e) {
        irods::publishing::exception_to_rerror(
            SYS_NOT_SUPPORTED,
            _e.what(),
            rei->rsComm->rError);
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   _e.what());
    }
    catch(const boost::bad_any_cast& _e) {
        irods::publishing::exception_to_rerror(
            INVALID_ANY_CAST,
            _e.what(),
            rei->rsComm->rError);
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   _e.what());
    }
    catch(const irods::exception& _e) {
        irods::publishing::exception_to_rerror(
            _e,
            rei->rsComm->rError);
        return irods::error(_e);
    }

    return err;

} // exec_rule

irods::error exec_rule_text(
    irods::default_re_ctx&,
    const std::string&,
    msParamArray_t*,
    const std::string&,
    irods::callback ) {
    return ERROR(
            RULE_ENGINE_CONTINUE,
            "exec_rule_text is not supported");
} // exec_rule_text

irods::error exec_rule_expression(
    irods::default_re_ctx&,
    const std::string&,
    msParamArray_t*,
    irods::callback) {
    return ERROR(
            RULE_ENGINE_CONTINUE,
            "exec_rule_expression is not supported");
} // exec_rule_expression

extern "C"
irods::pluggable_rule_engine<irods::default_re_ctx>* plugin_factory(
    const std::string& _inst_name,
    const std::string& _context ) {
    irods::pluggable_rule_engine<irods::default_re_ctx>* re =
        new irods::pluggable_rule_engine<irods::default_re_ctx>(
                _inst_name,
                _context);
    re->add_operation<
        irods::default_re_ctx&,
        const std::string&>(
            "start",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    const std::string&)>(start));
    re->add_operation<
        irods::default_re_ctx&,
        const std::string&>(
            "stop",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    const std::string&)>(stop));
    re->add_operation<
        irods::default_re_ctx&,
        const std::string&,
        bool&>(
            "rule_exists",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    const std::string&,
                    bool&)>(rule_exists));
    re->add_operation<
        irods::default_re_ctx&,
        std::vector<std::string>&>(
            "list_rules",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    std::vector<std::string>&)>(list_rules));
    re->add_operation<
        irods::default_re_ctx&,
        const std::string&,
        std::list<boost::any>&,
        irods::callback>(
            "exec_rule",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    const std::string&,
                    std::list<boost::any>&,
                    irods::callback)>(exec_rule));
    re->add_operation<
        irods::default_re_ctx&,
        const std::string&,
        msParamArray_t*,
        const std::string&,
        irods::callback>(
            "exec_rule_text",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    const std::string&,
                    msParamArray_t*,
                    const std::string&,
                    irods::callback)>(exec_rule_text));

    re->add_operation<
        irods::default_re_ctx&,
        const std::string&,
        msParamArray_t*,
        irods::callback>(
            "exec_rule_expression",
            std::function<
                irods::error(
                    irods::default_re_ctx&,
                    const std::string&,
                    msParamArray_t*,
                    irods::callback)>(exec_rule_expression));
    return re;

} // plugin_factory




