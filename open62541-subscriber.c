#include "open62541.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#define PROFILE "http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp"
#define ADDRESS "opc.udp://224.0.0.1:4801/"

UA_Boolean running = true;

static void stopHandler(int signal) {
	running = false;
}

UA_NodeId connectionIdentifier;
UA_NodeId readerGroupIdentifier;

UA_NodeId readerIdentifier_1;
UA_DataSetReaderConfig readerConfig_1;
UA_NodeId readerIdentifier_2;
UA_DataSetReaderConfig readerConfig_2;

static UA_StatusCode addPubSubConnection(UA_Server *server,UA_String *profile,
		UA_NetworkAddressUrlDataType *networkAddressUrl);
static UA_StatusCode addReaderGroup(UA_Server *server);
static void fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData,char*);
static UA_StatusCode addDataSetReader(UA_Server *server,UA_NodeId *reader_id,	
		char *reader_name,uint16_t publisher_id,uint16_t writer_group_id,
		uint16_t dataset_writer_id,UA_DataSetReaderConfig *reader_config);
static UA_StatusCode addSubscribedVariables (UA_Server *server, 
		UA_NodeId dataSetReaderId,UA_DataSetReaderConfig readerCfg,uint16_t);

// Basic function defined in open62541 example
// ---------------------------------------------------------------------------
static UA_StatusCode addPubSubConnection(UA_Server *server, 
		UA_String *transportProfile,
		UA_NetworkAddressUrlDataType *networkAddressUrl) {

    if((server == NULL) || (transportProfile == NULL) ||
        (networkAddressUrl == NULL)) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING("UDPMC Connection 1");
    connectionConfig.transportProfileUri = *transportProfile;
    connectionConfig.enabled = UA_TRUE;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.numeric = UA_UInt32_random ();
    retval |= UA_Server_addPubSubConnection (server, &connectionConfig, 
				&connectionIdentifier);
    if (retval != UA_STATUSCODE_GOOD) {
        return retval;
    }

    return retval;
}

static UA_StatusCode addReaderGroup(UA_Server *server) {
    if(server == NULL) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_ReaderGroupConfig readerGroupConfig;
    memset (&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    retval |= UA_Server_addReaderGroup(server, connectionIdentifier, 
				&readerGroupConfig,&readerGroupIdentifier);
    UA_Server_setReaderGroupOperational(server, readerGroupIdentifier);
    return retval;
}

// ---------------------------------------------------------------------------
static UA_StatusCode addSubscribedVariables (UA_Server *server, 
		UA_NodeId dataSetReaderId,UA_DataSetReaderConfig readerConfigId,
		uint16_t number_id) {

    if(server == NULL)
        return UA_STATUSCODE_BADINTERNALERROR;

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_NodeId folderId;
    UA_String folderName = readerConfigId.dataSetMetaData.name;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    UA_QualifiedName folderBrowseName;
    if(folderName.length > 0) {
        oAttr.displayName.locale = UA_STRING ("en-US");
        oAttr.displayName.text = folderName;
        folderBrowseName.namespaceIndex = 1;
        folderBrowseName.name = folderName;
    } else {
        oAttr.displayName = UA_LOCALIZEDTEXT ("en-US", "Subscribed Variables");
        folderBrowseName = UA_QUALIFIEDNAME (1, "Subscribed Variables");
    }

    UA_Server_addObjectNode (server, UA_NODEID_NULL,
                             UA_NODEID_NUMERIC (0, UA_NS0ID_OBJECTSFOLDER),
                             UA_NODEID_NUMERIC (0, UA_NS0ID_ORGANIZES),
                             folderBrowseName, UA_NODEID_NUMERIC (0,
                             UA_NS0ID_BASEOBJECTTYPE), oAttr, NULL, &folderId);

    UA_FieldTargetVariable *targetVars = (UA_FieldTargetVariable *)
   			UA_calloc(readerConfigId.dataSetMetaData.fieldsSize, 
				sizeof(UA_FieldTargetVariable));

    for(size_t i = 0; i < readerConfigId.dataSetMetaData.fieldsSize; i++) {
        /* Variable to subscribe data */
        UA_VariableAttributes vAttr = UA_VariableAttributes_default;
        UA_LocalizedText_copy(
						&readerConfigId.dataSetMetaData.fields[i].description,
            &vAttr.description);
        vAttr.displayName.locale = UA_STRING("en-US");
        vAttr.displayName.text = readerConfigId.dataSetMetaData.fields[i].name;
        vAttr.dataType = readerConfigId.dataSetMetaData.fields[i].dataType;

        UA_NodeId newNode;
        retval |= UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, 
							(UA_UInt32)number_id),
              folderId,
              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
              UA_QUALIFIEDNAME(1, 
								(char *)readerConfigId.dataSetMetaData.fields[i].name.data),
              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
              vAttr, NULL, &newNode);

        UA_FieldTargetDataType_init(&targetVars[i].targetVariable);
        targetVars[i].targetVariable.attributeId  = UA_ATTRIBUTEID_VALUE;
        targetVars[i].targetVariable.targetNodeId = newNode;
    }

    retval = UA_Server_DataSetReader_createTargetVariables(server, 
				dataSetReaderId,readerConfigId.dataSetMetaData.fieldsSize,targetVars);

    for(size_t i = 0; i < readerConfigId.dataSetMetaData.fieldsSize; i++)
        UA_FieldTargetDataType_clear(&targetVars[i].targetVariable);

    UA_free(targetVars);
    UA_free(readerConfigId.dataSetMetaData.fields);

    return retval;
}

static void fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData,
		char *folder) {

    if(pMetaData == NULL) {
        return;
    }

    UA_DataSetMetaDataType_init (pMetaData);
    pMetaData->name = UA_STRING(folder);

    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData*)UA_Array_new (pMetaData->fieldsSize,
                         		&UA_TYPES[UA_TYPES_FIELDMETADATA]);

    /* DateTime DataType */
    UA_FieldMetaData_init (&pMetaData->fields[0]);
    UA_NodeId_copy(&UA_TYPES[UA_TYPES_DOUBLE].typeId,
                    &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_NS0ID_DOUBLE;
    pMetaData->fields[0].name =  UA_STRING ("value");
    pMetaData->fields[0].valueRank = -1;
}

static UA_StatusCode addDataSetReader(UA_Server *server,UA_NodeId *reader_id,	
		char *reader_name,uint16_t publisher_id,uint16_t writer_group_id,
		uint16_t dataset_writer_id,UA_DataSetReaderConfig *readerConfig) {

    if(server == NULL) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    UA_StatusCode retval = UA_STATUSCODE_GOOD;

    readerConfig->name = UA_STRING(reader_name);
    UA_UInt16 publisherIdentifier = publisher_id;
    readerConfig->publisherId.type = &UA_TYPES[UA_TYPES_UINT16];
    readerConfig->publisherId.data = &publisherIdentifier;
    readerConfig->writerGroupId    = writer_group_id;
    readerConfig->dataSetWriterId  = dataset_writer_id;

    fillTestDataSetMetaData(&readerConfig->dataSetMetaData,reader_name);

    retval |= UA_Server_addDataSetReader(server,readerGroupIdentifier, 
				readerConfig,reader_id);

    return retval;
}

// ---------------------------------------------------------------------------

int run(UA_String *transportProfile,UA_NetworkAddressUrlDataType *netAddrUrl) {
	signal(SIGINT,stopHandler);
	signal(SIGTERM,stopHandler);
	
	UA_StatusCode ret = UA_STATUSCODE_GOOD;
	UA_Server *server = UA_Server_new();
	UA_ServerConfig *config = UA_Server_getConfig(server);
	UA_ServerConfig_setMinimal(config, 4840, NULL);
	
	UA_ServerConfig_addPubSubTransportLayer(config, 
			UA_PubSubTransportLayerUDPMP());
	
	// Add connection
	ret |= addPubSubConnection(server,transportProfile,netAddrUrl);
	if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;

	// Add ReaderGroup
  ret |= addReaderGroup(server);
  if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;

	// Add DataSetReader 1 & 2
  ret |= addDataSetReader(server,&readerIdentifier_1,"sensor_1",1,1,50001,
			&readerConfig_1);
  if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;
  ret |= addDataSetReader(server,&readerIdentifier_2,"sensor_2",1,1,50002,
			&readerConfig_2);
  if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;

  // Add SubscribedVariables to the created DataSetReader
  ret |= addSubscribedVariables(server,readerIdentifier_1,readerConfig_1,50001);
  if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;

  ret |= addSubscribedVariables(server,readerIdentifier_2,readerConfig_2,50002);
  if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;

  ret = UA_Server_run(server, &running);
  if (ret != UA_STATUSCODE_GOOD) return EXIT_FAILURE;

  UA_Server_delete(server);

  return ret == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;

}

int main(int argc, char **argv) {

  UA_String transportProfile = UA_STRING(PROFILE);
  UA_NetworkAddressUrlDataType netUrl = {UA_STRING_NULL,UA_STRING(ADDRESS)};
	
  return run(&transportProfile, &netUrl);
}
