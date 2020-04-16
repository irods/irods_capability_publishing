
#define IRODS_IO_TRANSPORT_ENABLE_SERVER_SIDE_API

#include "irods_query.hpp"
#include "irods_re_plugin.hpp"
#include "irods_re_ruleexistshelper.hpp"
#include "utilities.hpp"
#include "plugin_specific_configuration.hpp"
#include "configuration.hpp"
#include "filesystem.hpp"
#include "dstream.hpp"




#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <sstream>

namespace {
    struct configuration {
        std::string              instance_name_;
        std::vector<std::string> hosts_;
        int                      bulk_count_{100};
        int                      read_size_{4194304};
        explicit configuration(const std::string& _instance_name) :
            instance_name_{_instance_name} {
            try {
                auto cfg = irods::publishing::get_plugin_specific_configuration(_instance_name);
                if(cfg.find("hosts") != cfg.end()) {
                    std::vector<boost::any> host_list = boost::any_cast<std::vector<boost::any>>(cfg.at("hosts"));
                    for( auto& i : host_list) {
                        hosts_.push_back(boost::any_cast<std::string>(i));
                    }
                }

                if(cfg.find("bulk_count") != cfg.end()) {
                    bulk_count_ = boost::any_cast<int>(cfg.at("bulk_count"));
                }

                if(cfg.find("read_size") != cfg.end()) {
                    bulk_count_ = boost::any_cast<int>(cfg.at("read_size"));
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
    std::string persistent_identifier_policy;

    void invoke_persistent_identifier_event(
        ruleExecInfo_t*    _rei,
        const std::string& _path,
        const std::string& _service_name,
        std::string*       _persistent_identifier) {
        using namespace boost::archive::iterators;

        // make smart decisions given _service_name

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

        (*_persistent_identifier) = os.str();

    } // invoke_persistent_identifier_event

} // namespace

irods::error start(
    irods::default_re_ctx&,
    const std::string& _instance_name ) {
    RuleExistsHelper::Instance()->registerRuleRegex("irods_policy_.*");
    config = std::make_unique<configuration>(_instance_name);
    persistent_identifier_policy = irods::publishing::policy::compose_policy_name(
                                     irods::publishing::policy::prefix,
                                     "persistent_identifier");
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
    _ret = persistent_identifier_policy == _rn;
    return SUCCESS();
}

irods::error list_rules(
    irods::default_re_ctx&,
    std::vector<std::string>& _rules) {
    _rules.push_back(persistent_identifier_policy);
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
        // Extract parameters from args
        auto it = _args.begin();
        const std::string  path{ irods::publishing::any_to_string(*it) }; ++it;
        const std::string  service_name{ irods::publishing::any_to_string(*it) }; ++it;
        std::string* pid{ boost::any_cast<std::string*>(*it) }; ++it;

        invoke_persistent_identifier_event(
            rei,
            path,
            service_name,
            pid);
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




