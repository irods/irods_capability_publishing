
#include "irods_server_properties.hpp"
#include "irods_re_plugin.hpp"
#include "utilities.hpp"
#include "publishing_utilities.hpp"
#include "irods_query.hpp"
#include "irods_virtual_path.hpp"

#include "rsExecMyRule.hpp"
#include "rsOpenCollection.hpp"
#include "rsReadCollection.hpp"
#include "rsCloseCollection.hpp"
#include "rsModAVUMetadata.hpp"

#define IRODS_FILESYSTEM_ENABLE_SERVER_SIDE_API
#include "filesystem.hpp"

#include <boost/any.hpp>
#include <boost/regex.hpp>
#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <random>

#include "json.hpp"


int _delayExec(
    const char *inActionCall,
    const char *recoveryActionCall,
    const char *delayCondition,
    ruleExecInfo_t *rei );

namespace irods {
    namespace publishing {
        publisher::publisher(
            ruleExecInfo_t*    _rei,
            const std::string& _instance_name) :
              rei_(_rei)
            , comm_(_rei->rsComm)
            , config_(_instance_name) {
        } // publisher

        void publisher::schedule_publishing_policy(
            const std::string& _json,
            const std::string& _params) {
            const int delay_err = _delayExec(
                                      _json.c_str(),
                                      "",
                                      _params.c_str(),
                                      rei_);
            if(delay_err < 0) {
                THROW(
                delay_err,
                "delayExec failed");
            }
        } // schedule_publishing_policy

        bool publisher::metadata_exists_on_collection(
            const std::string& _collection_name,
            const std::string& _attribute,
            const std::string& _value,
            const std::string& _units ) {
            try {
                std::string query_str {
                    boost::str(
                            boost::format("SELECT META_COLL_ATTR_VALUE, META_COLL_ATTR_UNITS WHERE META_COLL_ATTR_NAME = '%s' and COLL_NAME = '%s'") %
                            _attribute %
                            _collection_name) };
                query<rsComm_t> qobj{rei_->rsComm, query_str, 1};
                if(qobj.size() == 0) {
                    return false;
                }

                for(auto results : qobj) {
                    if(results[0] == _value &&
                       results[1] == _units) {
                        return true;
                    }
                }

                return false;

            }
            catch( irods::exception& _e) {
                return false;
            }
        } // metadata_exists_on_collection

        bool publisher::metadata_exists_on_object(
            const std::string& _object_path,
            const std::string& _attribute,
            const std::string& _value,
            const std::string& _units ) {
            namespace fs = irods::experimental::filesystem;
            try {
                fs::path p{_object_path};
                std::string coll_name = p.parent_path().string();
                std::string data_name = p.object_name().string();

                std::string query_str {
                    boost::str(
                            boost::format("SELECT META_DATA_ATTR_VALUE, META_DATA_ATTR_UNITS WHERE META_DATA_ATTR_NAME = '%s' and COLL_NAME = '%s' and DATA_NAME = '%s'")
                            % _attribute
                            % coll_name
                            % data_name) };
                query<rsComm_t> qobj{rei_->rsComm, query_str, 1};
                if(qobj.size() == 0) {
                    return false;
                }

                for(auto results : qobj) {
                    if(results[0] == _value &&
                       results[1] == _units) {
                        return true;
                    }
                }

                return false;

            }
            catch( irods::exception& _e) {
                return false;
            }
        } // metadata_exists_on_object

        bool publisher::publishing_metadata_exists_in_path(
            const std::string& _path) {
            namespace fs   = irods::experimental::filesystem;
            namespace fsvr = irods::experimental::filesystem::server;
            rsComm_t& comm = *rei_->rsComm;
            try {
                fs::path full_path{_path};

                if(fsvr::is_data_object(comm, full_path) &&
                   object_is_published(_path)) {
                    return true;
                }
                else if(collection_is_published(_path)) {
                    return true;
                }

                auto coll = full_path.parent_path();
                while(!coll.empty()) {
                    try {
                        if(collection_is_published(coll.string())) {
                            return true;
                        }

                        coll = coll.parent_path();
                    }
                    catch(const std::exception& _e) {
                        rodsLog(
                            LOG_ERROR,
                            "publishing_metadata_exists_in_path failed [%s]",
                            _e.what());
                    }
                    catch(const irods::exception& _e) {
                        rodsLog(
                            LOG_ERROR,
                            "publishing_metadata_exists_in_path failed [%s]",
                            _e.what());
                    }

                } // while

                return false;
            }
            catch(const std::exception& _e) {
                rodsLog(
                    LOG_ERROR,
                    "publishing_metadata_exists_in_path failed [%s]",
                    _e.what());
            }
            catch(const irods::exception& _e) {
                rodsLog(
                    LOG_ERROR,
                    "publishing_metadata_exists_in_path failed [%s]",
                    _e.what());
            }

            return false;

        } // publishing_metadata_exists_in_path

        void publisher::schedule_collection_publishing_event(
            const std::string& _collection_name,
            const std::string& _publisher,
            const std::string& _user_name) {

            rodsLog(
                config_.log_level,
                "irods::publishing::collection publishing collection [%s] with publisher [%s]",
                _collection_name.c_str(),
                _publisher.c_str());

            using json = nlohmann::json;
            json rule_obj;
            rule_obj["rule-engine-operation"]     = policy::collection::publish;
            rule_obj["rule-engine-instance-name"] = config_.instance_name_;
            rule_obj["collection-name"]           = _collection_name;
            rule_obj["user-name"]                 = _user_name;
            rule_obj["publisher"]                 = _publisher;
            rule_obj["publish-type"]              = publish_type::collection;

            const auto delay_err = _delayExec(
                                       rule_obj.dump().c_str(),
                                       "",
                                       generate_delay_execution_parameters().c_str(),
                                       rei_);
            if(delay_err < 0) {
                THROW(
                    delay_err,
                    boost::format("queue collection publishing failed for [%s] publisher [%s]") %
                    _collection_name %
                    _publisher);
            }

        rodsLog(
            config_.log_level,
            "irods::publishing::collection publishing collection [%s] with [%s]",
            _collection_name.c_str(),
            _publisher.c_str());
        } // schedule_collection_publishing_event

        void publisher::schedule_object_publishing_event(
            const std::string& _object_path,
            const std::string& _user_name,
            const std::string& _publisher) {
            try {
                schedule_policy_event_for_object(
                    policy::object::publish,
                    _object_path,
                    _user_name,
                    _publisher,
                    publish_type::object,
                    generate_delay_execution_parameters());
            }
            catch(const irods::exception& _e) {
                rodsLog(
                    LOG_ERROR,
                    "failed [%s]",
                    _e.what());
            }
        } // schedule_object_publishing_event

        std::string publisher::generate_delay_execution_parameters() {
            std::string params{config_.delay_parameters + "<INST_NAME>" + config_.instance_name_ + "</INST_NAME>"};

            int min_time{1};
            try {
                min_time = boost::lexical_cast<int>(config_.minimum_delay_time);
            }
            catch(const boost::bad_lexical_cast&) {}

            int max_time{30};
            try {
                max_time = boost::lexical_cast<int>(config_.maximum_delay_time);
            }
            catch(const boost::bad_lexical_cast&) {}

            std::string sleep_time{"1"};
            try {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(min_time, max_time);
                sleep_time = boost::lexical_cast<std::string>(dis(gen));
            }
            catch(const boost::bad_lexical_cast&) {}

            params += "<PLUSET>"+sleep_time+"s</PLUSET>";

            rodsLog(
                config_.log_level,
                "irods::storage_tiering :: delay params min [%d] max [%d] computed [%s]",
                min_time,
                max_time,
                params.c_str());

            return params;

        } // generate_delay_execution_parameters

        bool publisher::object_is_published(
            const std::string& _object_path) {
            boost::filesystem::path p{_object_path};
            std::string coll_name = p.parent_path().string();
            std::string data_name = p.filename().string();

            std::string query_str {
                boost::str(boost::format(
                "SELECT META_DATA_ATTR_VALUE WHERE META_DATA_ATTR_NAME = '%s' and DATA_NAME = '%s' AND COLL_NAME = '%s'")
                        % config_.publish
                        % data_name
                        % coll_name)};
            try {
                query<rsComm_t> qobj{comm_, query_str, 1};
                if(qobj.size() == 0) {
                    return false;
                }
            }
            catch(const std::exception&) {
                return false;
            }
            catch(const irods::exception&) {
                return false;
            }

            return true;
        } // object_is_published

        bool publisher::collection_is_published(
            const std::string& _collection_name) {
            std::string query_str {
                boost::str(boost::format(
                "SELECT META_COLL_ATTR_VALUE WHERE META_COLL_ATTR_NAME = '%s' and COLL_NAME = '%s'")
                        % config_.publish
                        % _collection_name)};
            try {
                query<rsComm_t> qobj{comm_, query_str, 1};
                if(qobj.size() == 0) {
                    return false;
                }
            }
            catch(const std::exception&) {
                return false;
            }
            catch(const irods::exception&) {
                return false;
            }

            return true;
        } // collection_is_published

        publisher::metadata_results publisher::get_metadata_for_data_object(
            const std::string& _object_path,
            const std::string& _meta_attr_name) {
            boost::filesystem::path p{_object_path};
            std::string coll_name = p.parent_path().string();
            std::string data_name = p.filename().string();

            std::string query_str {
                boost::str(boost::format(
                "SELECT META_DATA_ATTR_VALUE, META_DATA_ATTR_UNITS WHERE META_DATA_ATTR_NAME = '%s' and DATA_NAME = '%s' AND COLL_NAME = '%s'")
                % _meta_attr_name
                % data_name
                % coll_name) };
            query<rsComm_t> qobj{comm_, query_str};

            metadata_results ret_val;
            for(const auto& row : qobj) {
                ret_val.push_back(std::make_pair(row[0], row[1]));
            }

            return ret_val;
        } // get_metadata_for_data_object

        publisher::metadata_results publisher::get_metadata_for_collection(
            const std::string& _meta_attr_name,
            const std::string& _collection) {
            std::string query_str {
                boost::str(boost::format(
                "SELECT META_COLL_ATTR_VALUE, META_COLL_ATTR_UNITS WHERE META_COLL_ATTR_NAME = '%s' and COLL_NAME = '%s'")
                % _meta_attr_name
                % _collection) };
            query<rsComm_t> qobj{comm_, query_str, 1};

            metadata_results ret_val;
            for(const auto& row : qobj) {
                ret_val.push_back(std::make_pair(row[0], row[1]));
            }

            return ret_val;
        } // get_metadata_for_collection

        void publisher::schedule_policy_event_for_object(
            const std::string& _event,
            const std::string& _object_path,
            const std::string& _user_name,
            const std::string& _publisher,
            const std::string& _publish_type,
            const std::string& _data_movement_params) {
            using json = nlohmann::json;
            json rule_obj;
            rule_obj["rule-engine-operation"]     = _event;
            rule_obj["rule-engine-instance-name"] = config_.instance_name_;
            rule_obj["object-path"]               = _object_path;
            rule_obj["user-name"]                 = _user_name;
            rule_obj["publisher"]                 = _publisher;
            rule_obj["publish-type"]              = _publish_type;

            const auto delay_err = _delayExec(
                                       rule_obj.dump().c_str(),
                                       "",
                                       _data_movement_params.c_str(),
                                       rei_);
            if(delay_err < 0) {
                THROW(
                    delay_err,
                    boost::format("queue publishing event failed for object [%s] publisher [%s] type [%s]") %
                    _object_path %
                    _publisher %
                    _publish_type);
            }

            rodsLog(
                config_.log_level,
                "irods::publishing::publisher publishing object [%s] with [%s] type [%s]",
                _object_path.c_str(),
                _publisher.c_str(),
                _publish_type.c_str());

        } // schedule_policy_event_for_object
    } // namespace publishing
}; // namespace irods

