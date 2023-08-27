function mapToDittoProtocolMsg(
    headers,
    textPayload,
    bytePayload,
    contentType
) {

    if (contentType !== "application/json") {
        return null; // only handle messages with content-type application/json
    }

    var jsonData = JSON.parse(textPayload);
    var node_id_value = jsonData.node_id;
    var dataRateValue = jsonData.dataRate;
    var AverageDelayValue = jsonData.AverageDelay;
    
    var path;
    var value;
    
    if (node_id_value != null) {
        path = "/attributes";
        value = {
            node_id: {
              properties: {
                value: node_id_value
              }
            }
        }
    }

    if (dataRateValue != null && AverageDelayValue != null) {
        path = "/features";
        value = {
            dataRate: {
                properties: {
                    value: dataRateValue
                }
            },
            AverageDelay: {
                properties: {
                    value: AverageDelayValue
                }
            }
        };
    } else if (dataRateValue != null) {
        path = "/features/dataRate/properties/value";
        value = dataRateValue;
    } else if (bufferStateValue != null) {
        path = "/features/bufferState/properties/remainingPackets";
        value = bufferStateValue;
    }
    
    if (!path || !value) {
        return null;
    }

    return Ditto.buildDittoProtocolMsg(
        "org.eclipse.ditto",     // the namespace we use
        headers["device_id"],    // Hono sets the authenticated device-id in this header
        "things",                // it is a Thing entity we want to update
        "twin",                  // we want to update the twin
        "commands",
        "modify",                // command = modify
        path,
        headers,                 // copy all headers as Ditto headers
        value
    );
}
