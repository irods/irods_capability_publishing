# Motivation
The iRODS publishing capability provides a policy framework around the publication of data or collections of data to external services.   Data objects or logical collections are annotated with metadata which indicates that any data objects or nested collections of data objects should be published to a given catalog or service.

# Configuration
## Metadata
Objects or collections are annotated with metadata indicating they should be published. The metadata is formatted as follows:
```
irods::publishing::publish <service>
```
Where `<serivce>` references a catalog or publication service.  This string is used to dynamically build the policy invocations when the publishing policy is triggered in order to delegate the operations to the appropriate rule engine plugin.

Users may also need additional configuration given identity management with a given service.  For instance, `data.world` requires and API Token which must be associated with a given user:
```
irods::publishing::api_token <token>
```

In the future, users may be decorated with various metadata associated with identity management.  Given that only iRODS administrators may view or alter user based metadata, this remains secure.

## Plugin Settings
To configure the publishing capability, two rule engine plugins need to be added to the "rule_engines" section of `/etc/irods/server_config.json`:
```
      "rule_engines": [
          {
                "instance_name": "irods_rule_engine_plugin-publishing-instance",
                "plugin_name": "irods_rule_engine_plugin-publishing",
                "plugin_specific_configuration": {
                }  
          },
          {
                "instance_name": "irods_rule_engine_plugin-dataworld-instance",
                "plugin_name": "irods_rule_engine_plugin-dataworld",
                "plugin_specific_configuration": {
                }  
          }          
      ]
```

The first is the publishing framework rule engine plugin, the second is the plugin responsible for implementing the policy for the publication service. Currently the only supported service is [data.world](https://data.world/). Other publication services such as [Dataverse](https://dataverse.org/) will be supported as interest in the community is identified.

# Policy Implementation
Policy names are dynamically crafted by the publishing plugin in order to invoke a particular service. The four policies a publishing technology must implement are crafted from base strings with the name of the service as indicated by the object or collection metadata annotation.  Should a new service be supported, these are the policies that need be implemented which will be invoked by the framework.

```
irods_policy_publishing_object_publish_<service>(object_path, user_name, publication_type)
irods_policy_publishing_object_purge_<service>(object_path, user_name, publication_type)
irods_policy_publishing_collection_publish_<service>(collection_path, user_name, publication_type)
irods_policy_publishing_collection_purge_<service>(collection_path, user_name, publication_type)
```
