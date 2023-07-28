
#include "plugin_specific_configuration.hpp"
#include <irods/irods_server_properties.hpp>
#include <irods/irods_exception.hpp>

namespace irods {
    namespace publishing {
         plugin_specific_configuration get_plugin_specific_configuration(
            const std::string& _instance_name ) {
            try {
                const auto& rule_engines = get_server_property<
                    const nlohmann::json&>(
                            std::vector<std::string>{
                            KW_CFG_PLUGIN_SPECIFIC_CONFIGURATION,
                            KW_CFG_PLUGIN_TYPE_RULE_ENGINE});
                for ( const auto& rule_engine : rule_engines ) {
                    const auto& inst_name = rule_engine.at( KW_CFG_INSTANCE_NAME ).get_ref<const std::string&>();
                    if ( inst_name == _instance_name ) {
                        if(rule_engine.count(KW_CFG_PLUGIN_SPECIFIC_CONFIGURATION) > 0) {
                            return rule_engine.at(KW_CFG_PLUGIN_SPECIFIC_CONFIGURATION);
                        } // if has PSC
                    } // if inst_name
                } // for rule_engines
            } catch ( const boost::bad_any_cast& e ) {
                THROW( INVALID_ANY_CAST, e.what() );
            } catch ( const std::out_of_range& e ) {
                THROW( KEY_NOT_FOUND, e.what() );
            }

            THROW(
                SYS_INVALID_INPUT_PARAM,
                boost::format("failed to find configuration for publishing plugin [%s]") %
                _instance_name);
        } // get_plugin_specific_configuration


    } // namespace publishing
} // namespace irods
