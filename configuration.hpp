#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <string>
#include "rodsLog.h"

namespace irods {
    namespace publishing {
        const std::string metadata_separator{"::"};

        namespace policy {
            // Policy Naming Examples
            // irods_policy_<namespace>_<subject>_<operation>_<technology>
            // irods_policy_publishing_object_publish
            // irods_policy_publishing_collection_publish

            static constexpr auto prefix = "irods_policy_publishing";
            std::string compose_policy_name(
                    const std::string& _prefix,
                    const std::string& _technology);

            namespace object {
                static const std::string publish{"irods_policy_publishing_object_publish"};
                static const std::string purge{"irods_policy_publishing_object_purge"};
            } // object


            namespace collection {
                static const std::string publish{"irods_policy_publishing_collection_publish"};
                static const std::string purge{"irods_policy_publishing_collection_purge"};
            } // collection

        } // policy

        std::string operation_and_publish_types_to_policy_name(
                const std::string& _operation_type,
                const std::string& _publish_type);

        namespace schedule {
            static const std::string object{"irods_policy_schedule_object_publish"};
            static const std::string collection{"irods_policy_schedule_collection_publish"};
        }

        namespace publish_type {
            static const std::string collection{"collection"};
            static const std::string object{"object"};
        }

        namespace operation_type {
            static const std::string publish{"publish"};
            static const std::string purge{"purge"};
        }

        struct configuration {
            // metadata attributes
            std::string publish{"irods::publishing::publish"};
            std::string api_token{"irods::publishing::api_token"};

            // basic configuration
            std::string minimum_delay_time{"1"};
            std::string maximum_delay_time{"30"};
            std::string delay_parameters{"<EF>60s DOUBLE UNTIL SUCCESS OR 5 TIMES</EF>"};
            int log_level{LOG_DEBUG};

            const std::string instance_name_{};
            explicit configuration(const std::string& _instance_name);
        }; // struct configuration
    } // namespace publishing
} // namespace irods

#endif // STORAGE_TIERING_CONFIGURATION_HPP
