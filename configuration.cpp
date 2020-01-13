
#include "configuration.hpp"
#include "plugin_specific_configuration.hpp"

namespace irods {
    namespace publishing {
        configuration::configuration(
            const std::string& _instance_name ) :
            instance_name_{_instance_name} {
            try {
                auto cfg = get_plugin_specific_configuration(_instance_name);
                auto capture_parameter = [&](const std::string& _param, std::string& _attr) {
                    if(cfg.find(_param) != cfg.end()) {
                        _attr = boost::any_cast<std::string>(cfg.at(_param));
                    }
                }; // capture_parameter

                capture_parameter("publish", publish);
                capture_parameter("api_token", api_token);
                capture_parameter("persistent_identifier", persistent_identifier);
                capture_parameter("minimum_delay_time", minimum_delay_time);
                capture_parameter("maximum_delay_time", maximum_delay_time);
                capture_parameter("delay_parameters",   delay_parameters);
            } catch ( const boost::bad_any_cast& _e ) {
                THROW( INVALID_ANY_CAST, _e.what() );
            } catch ( const exception _e ) {
                THROW( KEY_NOT_FOUND, _e.what() );
            }


        } // ctor configuration

        namespace policy {
            std::string compose_policy_name(
                    const std::string& _prefix,
                    const std::string& _technology) {
                return _prefix+"_"+_technology;
            }
        }

        std::string operation_and_publish_types_to_policy_name(
                const std::string& _operation_type,
                const std::string& _publish_type) {

            if(operation_type::publish == _operation_type) {
                if(publish_type::object == _publish_type) {
                    return policy::object::publish;
                }
                else if(publish_type::collection == _publish_type) {
                    return policy::collection::publish;
                } // else
            }
            else if(operation_type::purge == _operation_type) {
                if(publish_type::object == _publish_type) {
                    return policy::object::purge;
                }
                else if(publish_type::collection == _publish_type) {
                    return policy::collection::purge;
                } // else
            }

            THROW(
                SYS_INVALID_INPUT_PARAM,
                boost::format("operation [%s], publish [%s]")
                % _operation_type
                % _publish_type);
        } // operation_and_publish_types_to_policy_name
    } // namespace publishing
} // namepsace irods

