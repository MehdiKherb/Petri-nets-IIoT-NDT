#!/bin/bash

export TENANT_NAME=org.eclipse.ditto
export DEVICE_ID=demo-device-001

curl -i -X POST http://${REGISTRY_IP}:${REGISTRY_PORT_HTTP}/v1/tenants/${TENANT_NAME} 

curl -u ditto:ditto -i -X POST http://${REGISTRY_IP}:${REGISTRY_PORT_HTTP}/v1/devices/${TENANT_NAME}/${TENANT_NAME}:${DEVICE_ID}

curl -i -X PUT -H "Content-Type: application/json" --data '[{
"type": "hashed-password",
"auth-id": "demo-device-001-auth",
"secrets": [{"pwd-plain": "my-password"}]}]' http://${REGISTRY_IP}:${REGISTRY_PORT_HTTP}/v1/credentials/${TENANT_NAME}/${TENANT_NAME}:${DEVICE_ID}

#create a policy 

curl -i -X PUT -u ditto:ditto -H 'Content-Type: application/json' --data '{
  "entries": {
    "DEFAULT": {
      "subjects": {
        "{{ request:subjectId }}": {
           "type": "Ditto user authenticated via nginx"
        }
      },
      "resources": {
        "thing:/": {
          "grant": ["READ", "WRITE"],
          "revoke": []
        },
        "policy:/": {
          "grant": ["READ", "WRITE"],
          "revoke": []
        },
        "message:/": {
          "grant": ["READ", "WRITE"],
          "revoke": []
        }
      }
    },
    "HONO": {
      "subjects": {
        "pre-authenticated:hono-connection": {
          "type": "Connection to Eclipse Hono"
        }
      },
      "resources": {
        "thing:/": {
          "grant": ["READ", "WRITE"],
          "revoke": []
        },
        "message:/": {
          "grant": ["READ", "WRITE"],
          "revoke": []
        }
      }
    }
  }
}' http://${DITTO_API_IP}:${DITTO_API_PORT_HTTP}/api/2/policies/org.acme:my-policy


#create the thing :  

curl -X PUT http://${DITTO_API_IP}:${DITTO_API_PORT_HTTP}/api/2/things/${TENANT_NAME}:${DEVICE_ID} -u ditto:ditto -H 'Content-Type: application/json' -d @devicePrototype.json

#configure the connection between ditto and hono via amqp

curl -X POST -i -u devops:devopsPw1! -H 'Content-Type: application/json' -d '{
"targetActorSelection": "/system/sharding/connection",
"headers": {
"aggregate": false
},
"piggybackCommand": {
"type": "connectivity.commands:createConnection",
"connection": {
"id": "hono-sandbox-connection",
"connectionType": "amqp-10",
"connectionStatus": "open",
"uri": "amqp://consumer%40HONO:verysecret@'${AMQP_NETWORK_IP}:${AMQP_NETWORK_PORT_AMQP}'",
"failoverEnabled": true,
"sources": [{
"addresses": [
"telemetry/org.eclipse.ditto",
"event/org.eclipse.ditto"
],
"authorizationContext": ["nginx:ditto"]
}]
}}}' http://${DITTO_API_IP}:${DITTO_API_PORT_HTTP}/devops/piggyback/connectivity?timeout=8s


