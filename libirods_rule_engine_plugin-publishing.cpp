
// =-=-=-=-=-=-=-
// irods includes
#include "irods_re_plugin.hpp"
#include "irods_re_ruleexistshelper.hpp"
#include "irods_hierarchy_parser.hpp"
#include "irods_resource_backport.hpp"
#include "irods_query.hpp"
#include "rsModAVUMetadata.hpp"

#include "utilities.hpp"
#include "publishing_utilities.hpp"

#undef LIST

// =-=-=-=-=-=-=-
// stl includes
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

// =-=-=-=-=-=-=-
// boost includes
#include <boost/any.hpp>
#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>

#include "json.hpp"

#include "objDesc.hpp"
extern l1desc_t L1desc[NUM_L1_DESC];

int _delayExec(
    const char *inActionCall,
    const char *recoveryActionCall,
    const char *delayCondition,
    ruleExecInfo_t *rei );

namespace {
    bool metadata_is_new = false;
    std::unique_ptr<irods::publishing::configuration>     config;
    std::map<int, std::tuple<std::string, std::string>> opened_objects;

    std::tuple<int, std::string>
    get_index_and_resource(const dataObjInp_t* _inp) {
        int l1_idx{};
        dataObjInfo_t* obj_info{};
        for(const auto& l1 : L1desc) {
            if(FD_INUSE != l1.inuseFlag) {
                continue;
            }
            if(!strcmp(l1.dataObjInp->objPath, _inp->objPath)) {
                obj_info = l1.dataObjInfo;
                l1_idx = &l1 - L1desc;
            }
        }

        if(nullptr == obj_info) {
            THROW(
                SYS_INVALID_INPUT_PARAM,
                "no object found");
        }

        std::string resource_name;
        irods::error err = irods::get_resource_property<std::string>(
                               obj_info->rescId,
                               irods::RESOURCE_NAME,
                               resource_name);
        if(!err.ok()) {
            THROW(err.code(), err.result());
        }

        return std::make_tuple(l1_idx, resource_name);
    } // get_object_path_and_resource

    void apply_publishing_policy(
        const std::string &    _rn,
        ruleExecInfo_t*        _rei,
        std::list<boost::any>& _args) {
        try {
            std::string object_path;
            std::string source_resource;
            // NOTE:: 3rd parameter is the target
            if("pep_api_data_obj_open_pre"   == _rn ||
               "pep_api_data_obj_create_pre" == _rn ||
               "pep_api_data_obj_put_pre"    == _rn ||
               "pep_api_data_obj_unlink_pre" == _rn) {
                auto it = _args.begin();
                std::advance(it, 2);
                if(_args.end() == it) {
                    THROW(
                        SYS_INVALID_INPUT_PARAM,
                        "invalid number of arguments");
                }

                auto obj_inp = boost::any_cast<dataObjInp_t*>(*it);
                if(obj_inp->openFlags & O_WRONLY || obj_inp->openFlags & O_RDWR) {
                    irods::publishing::publisher idx{_rei, config->instance_name_};
                    if(idx.publishing_metadata_exists_in_path(obj_inp->objPath)) {
                        THROW(
                            SYS_INVALID_OPR_TYPE,
                            boost::format("object is published and now imutable [%s]")
                            % obj_inp->objPath);
                    }
                }
            }
            else if("pep_api_rm_coll_pre" == _rn) {
                auto it = _args.begin();
                std::advance(it, 2);
                if(_args.end() == it) {
                    THROW(
                        SYS_INVALID_INPUT_PARAM,
                        "invalid number of arguments");
                }

                auto coll_inp = boost::any_cast<collInp_t*>(*it);
                irods::publishing::publisher idx{_rei, config->instance_name_};
                if(idx.publishing_metadata_exists_in_path(coll_inp->collName)) {
                    THROW(
                        SYS_INVALID_OPR_TYPE,
                        boost::format("collection is published and now imutable [%s]")
                        % coll_inp->collName);
                }

            }
            else if("pep_api_mod_avu_metadata_pre" == _rn) {
                auto it = _args.begin();
                std::advance(it, 2);
                if(_args.end() == it) {
                    THROW(
                        SYS_INVALID_INPUT_PARAM,
                        "invalid number of arguments");
                }

                const auto avu_inp = boost::any_cast<modAVUMetadataInp_t*>(*it);
                const std::string attribute{avu_inp->arg3};
                if(config->publish != attribute) {
                    return;
                }

                const std::string operation{avu_inp->arg0};
                const std::string type{avu_inp->arg1};
                const std::string object_path{avu_inp->arg2};
                const std::string add{"add"};
                const std::string set{"set"};
                const std::string collection{"-C"};
                const std::string object{"-d"};

                irods::publishing::publisher idx{_rei, config->instance_name_};
                if(operation == set || operation == add) {
                    if(config->publish == attribute) {
                        // was the added tag an publishing indicator
                        // verify that this is not new metadata with a query and set a flag
                        if(type == collection) {
                            metadata_is_new = !idx.metadata_exists_on_collection(
                                                             object_path,
                                                             avu_inp->arg3,
                                                             avu_inp->arg4,
                                                             avu_inp->arg5);
                        }
                        if(type == object) {
                            metadata_is_new = !idx.metadata_exists_on_object(
                                                             object_path,
                                                             avu_inp->arg3,
                                                             avu_inp->arg4,
                                                             avu_inp->arg5);
                        }
                    }
                }
            }
            else if("pep_api_mod_avu_metadata_post" == _rn) {
                auto it = _args.begin();
                std::advance(it, 2);
                if(_args.end() == it) {
                    THROW(
                        SYS_INVALID_INPUT_PARAM,
                        "invalid number of arguments");
                }

                const auto avu_inp = boost::any_cast<modAVUMetadataInp_t*>(*it);
                const std::string operation{avu_inp->arg0};
                const std::string type{avu_inp->arg1};
                const std::string logical_path{avu_inp->arg2};
                const std::string attribute{avu_inp->arg3};
                const std::string value{avu_inp->arg4};
                const std::string units{avu_inp->arg5};
                const std::string add{"add"};
                const std::string set{"set"};
                const std::string rm{"rm"};
                const std::string collection{"-C"};
                const std::string data_object{"-d"};

                // no work to do if this is not a publication attribute
                if(config->publish != attribute) {
                    return;
                }

                irods::publishing::publisher idx{_rei, config->instance_name_};
                if(operation == rm) {
                    // removed publish metadata from collection
                    if(type == collection) {
                        // schedule a purge of all published data in collection
                    }
                    // removed a single published AVU on an object
                    if(type == data_object) {
                        // schedule a purge of published data
                    }
                }
                else if(operation == set || operation == add) {
                    if(type == collection) {
                        if(metadata_is_new) {
                            idx.schedule_collection_publishing_event(
                                logical_path,
                                value,
                                _rei->rsComm->clientUser.userName);
                        }
                    }
                    if(type == data_object) {
                        if(metadata_is_new) {
                            idx.schedule_object_publishing_event(
                                    logical_path,
                                    value,
                                    _rei->rsComm->clientUser.userName);
                        }
                    }
                }
            }
        }
        catch(const boost::bad_any_cast& _e) {
            THROW(
                INVALID_ANY_CAST,
                boost::str(boost::format(
                    "function [%s] rule name [%s]")
                    % __FUNCTION__ % _rn));
        }
    } // apply_publishing_policy

    void apply_object_policy(
        ruleExecInfo_t*    _rei,
        const std::string& _policy_root,
        const std::string& _object_path,
        const std::string& _user_name,
        const std::string& _publisher,
        const std::string& _publish_type) {
        const std::string policy_name{irods::publishing::policy::compose_policy_name(
                              _policy_root,
                              _publisher)};

        std::list<boost::any> args;
        args.push_back(boost::any(_object_path));
        args.push_back(boost::any(_user_name));
        args.push_back(boost::any(_publish_type));
        irods::publishing::invoke_policy(_rei, policy_name, args);

    } // apply_object_policy

    void apply_collection_policy(
        ruleExecInfo_t*    _rei,
        const std::string& _policy_root,
        const std::string& _collection_name,
        const std::string& _user_name,
        const std::string& _publisher,
        const std::string& _publish_type) {
        const std::string policy_name{irods::publishing::policy::compose_policy_name(
                              _policy_root,
                              _publisher)};

        std::list<boost::any> args;
        args.push_back(boost::any(_collection_name));
        args.push_back(boost::any(_user_name));
        args.push_back(boost::any(_publish_type));
        irods::publishing::invoke_policy(_rei, policy_name, args);

    } // apply_collection_policy

} // namespace


irods::error start(
    irods::default_re_ctx&,
    const std::string& _instance_name ) {
    RuleExistsHelper::Instance()->registerRuleRegex("pep_api_.*");
    config = std::make_unique<irods::publishing::configuration>(_instance_name);
    return SUCCESS();
} // start

irods::error stop(
    irods::default_re_ctx&,
    const std::string& ) {
    return SUCCESS();
} // stop

irods::error rule_exists(
    irods::default_re_ctx&,
    const std::string& _rn,
    bool&              _ret) {
    const std::set<std::string> rules{
                                    "pep_api_rm_coll_pre",
                                    "pep_api_data_obj_open_pre",
                                    "pep_api_data_obj_create_pre",
                                    "pep_api_data_obj_put_pre",
                                    "pep_api_data_obj_unlink_pre",
                                    "pep_api_mod_avu_metadata_pre",
                                    "pep_api_mod_avu_metadata_post"};
    _ret = rules.find(_rn) != rules.end();

    return SUCCESS();
} // rule_exists

irods::error list_rules(irods::default_re_ctx&, std::vector<std::string>&) {
    return SUCCESS();
} // list_rules

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
        apply_publishing_policy(_rn, rei, _args);
    }
    catch(const  std::invalid_argument& _e) {
        irods::publishing::exception_to_rerror(
            SYS_NOT_SUPPORTED,
            _e.what(),
            rei->rsComm->rError);
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   _e.what());
    }
    catch(const std::domain_error& _e) {
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

    return CODE(RULE_ENGINE_CONTINUE);

} // exec_rule

irods::error exec_rule_text(
    irods::default_re_ctx&,
    const std::string&  _rule_text,
    msParamArray_t*     _ms_params,
    const std::string&  _out_desc,
    irods::callback     _eff_hdlr) {
    using json = nlohmann::json;

    try {
        // skip the first line: @external
        std::string rule_text{_rule_text};
        if(_rule_text.find("@external") != std::string::npos) {
            rule_text = _rule_text.substr(10);
        }
        const auto rule_obj = json::parse(rule_text);
        const std::string& rule_engine_instance_name = rule_obj["rule-engine-instance-name"];
        // if the rule text does not have our instance name, fail
        if(config->instance_name_ != rule_engine_instance_name) {
            return ERROR(
                    SYS_NOT_SUPPORTED,
                    "instance name not found");
        }
        return ERROR(
                SYS_NOT_SUPPORTED,
                "supported rule name not found");
    }
    catch(const  std::invalid_argument& _e) {
        std::string msg{"Rule text is not valid JSON -- "};
        msg += _e.what();
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   msg);
    }
    catch(const std::domain_error& _e) {
        std::string msg{"Rule text is not valid JSON -- "};
        msg += _e.what();
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   msg);
    }
    catch(const irods::exception& _e) {
        return ERROR(
                _e.code(),
                _e.what());
    }

    return SUCCESS();
} // exec_rule_text

irods::error exec_rule_expression(
    irods::default_re_ctx&,
    const std::string&     _rule_text,
    msParamArray_t*        _ms_params,
    irods::callback        _eff_hdlr) {
    using json = nlohmann::json;
    ruleExecInfo_t* rei{};
    const auto err = _eff_hdlr("unsafe_ms_ctx", &rei);
    if(!err.ok()) {
        return err;
    }

    try {
        const auto rule_obj = json::parse(_rule_text);
        if(irods::publishing::policy::object::publish ==
           rule_obj["rule-engine-operation"]) {
            try {
                // proxy for provided user name
                const std::string& user_name = rule_obj["user-name"];
                rstrcpy(
                    rei->rsComm->clientUser.userName,
                    user_name.c_str(),
                    NAME_LEN);

                apply_object_policy(
                    rei,
                    irods::publishing::policy::object::publish,
                    rule_obj["object-path"],
                    rule_obj["user-name"],
                    rule_obj["publisher"],
                    rule_obj["publish-type"]);
            }
            catch(const irods::exception& _e) {
                printErrorStack(&rei->rsComm->rError);
                return ERROR(
                        _e.code(),
                        _e.what());
            }
        }
        else if(irods::publishing::policy::object::purge ==
                rule_obj["rule-engine-operation"]) {
            try {
                // proxy for provided user name
                const std::string& user_name = rule_obj["user-name"];
                rstrcpy(
                    rei->rsComm->clientUser.userName,
                    user_name.c_str(),
                    NAME_LEN);

                apply_object_policy(
                    rei,
                    irods::publishing::policy::object::purge,
                    rule_obj["object-path"],
                    rule_obj["user-name"],
                    rule_obj["publisher"],
                    rule_obj["publish-type"]);
            }
            catch(const irods::exception& _e) {
                printErrorStack(&rei->rsComm->rError);
                return ERROR(
                        _e.code(),
                        _e.what());
            }
        }
        else if(irods::publishing::policy::collection::publish ==
                rule_obj["rule-engine-operation"]) {

            apply_collection_policy(
                rei,
                irods::publishing::policy::collection::publish,
                rule_obj["collection-name"],
                rule_obj["user-name"],
                rule_obj["publisher"],
                rule_obj["publish-type"]);
        }
        else if(irods::publishing::policy::collection::purge ==
                rule_obj["rule-engine-operation"]) {

            apply_collection_policy(
                rei,
                irods::publishing::policy::collection::purge,
                rule_obj["collection-name"],
                rule_obj["user-name"],
                rule_obj["publisher"],
                rule_obj["publish-type"]);
        }
        else {
            printErrorStack(&rei->rsComm->rError);
            return ERROR(
                    SYS_NOT_SUPPORTED,
                    "supported rule name not found");
        }
    }
    catch(const  std::invalid_argument& _e) {
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   _e.what());
    }
    catch(const std::domain_error& _e) {
        return ERROR(
                   SYS_NOT_SUPPORTED,
                   _e.what());
    }
    catch(const irods::exception& _e) {
        return ERROR(
                _e.code(),
                _e.what());
    }

    return SUCCESS();

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

