

#ifndef INDEXING_UTILITIES_HPP
#define INDEXING_UTILITIES_HPP

#include <list>
#include <boost/any.hpp>
#include <string>

#include <irods/rcMisc.h>
#include "configuration.hpp"

namespace irods {
    namespace publishing {
        class publisher {

            public:
            publisher(
                ruleExecInfo_t*    _rei,
                const std::string& _instance_name);

            void schedule_publishing_policy(
                const std::string& _json,
                const std::string& _params);

            bool metadata_exists_on_collection(
                const std::string& _collection_name,
                const std::string& _attribute,
                const std::string& _value,
                const std::string& _units );

            bool metadata_exists_on_object(
                const std::string& _object_path,
                const std::string& _attribute,
                const std::string& _value,
                const std::string& _units );

            bool publishing_metadata_exists_in_path(
                const std::string& _path);

            void schedule_collection_publishing_event(
                const std::string& _object_path,
                const std::string& _publisher,
                const std::string& _user_name);

            void schedule_object_publishing_event(
                const std::string& _object_path,
                const std::string& _user_name,
                const std::string& _publisher);

            private:
            using metadata_results = std::vector<std::pair<std::string, std::string>>;

            std::string generate_delay_execution_parameters();

            bool object_is_published(
                const std::string& _object_path);

            bool collection_is_published(
                const std::string& _collection_name);

            metadata_results
            get_metadata_for_data_object(
                const std::string& _object_path,
                const std::string& _meta_attr_name);

            metadata_results
            get_metadata_for_collection(
                const std::string& _collection_name,
                const std::string& _meta_attr_name);

            void schedule_policy_event_for_object(
                const std::string& _event,
                const std::string& _object_path,
                const std::string& _user_name,
                const std::string& _publisher,
                const std::string& _publish_type,
                const std::string& _data_movement_params);

            // Attributes
            ruleExecInfo_t* rei_;
            rsComm_t*       comm_;
            configuration   config_;

            const std::string EMPTY_RESOURCE_NAME{"EMPTY_RESOURCE_NAME"};
        }; // class publisher
    } // namespace publishing
} // namespace irods

#endif // INDEXING_UTILITIES_HPP

