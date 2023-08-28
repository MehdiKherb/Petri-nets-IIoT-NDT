# Petri-nets-IIoT-NDT
This repository represents the artifact of our paper entitled : "Constructing a Network Digital Twin through Formal Modeling: Tackling the Virtual-Real Mapping Challenge in IIoT Networks". It contains all the scripts for deploying an IoT network and its Digital Twin using Eclipse Hono + Ditto. A Timed Petri-net is used to model the network. 

**hono-mqtt-client** : this is the MQTT client firmware of the simulated IoT devices, it is developed in Contiki-NG. It is to be deployed on motes created in Cooja simulator and its function is to publish to a mosquitto broker running locally (the required mosquitto version is 1.6.9). The published values include : the average packets end-to-end delay, the node ID, and the current data send interval of the node.

**rpl-border-router** : this is the firmware to be deployed in the sink node (Node 1 in the simulated network), it serves as a border router forwarding published messages from the source nodes to the mosquitto broker.

**deployment utils** : these are the necessary script to deploy and configure [Cloud2Edge](https://www.eclipse.org/packages/packages/cloud2edge/)  package containing Eclipse Hono + Ditto on Google Cloud Platform (GCP). You can refer to the following medium article for more insights : [Running Eclipse HONO and Ditto on Google Cloud](https://medium.com/google-cloud/running-eclipse-hono-and-ditto-on-google-cloud-1-d47504fecc7).

- _deploymentScript.sh_ : this is the first script to run which deploys the package on your GCP.
- _setCloud2EdgeEnv.sh_ : is the second script to run which outputs environment variables containing the addresses of the different deployed services. A copy/paste of the output commands will set up the different environment variables necessary for the package configuration.
- _setupDitto_Hono.sh_ : this is the third script to run which configures the package, it :
  1. Creates a Tenant _org.eclipse.ditto_ in Hono.
  2. Creates a device _demo-device-001_ in this tenant.
  3. Creates a policy _org.acme:my-policy_ for Ditto.
  4. Creates a thing using the _devicePrototype.json_ file.
  5. Sets up the connection between Ditto and Hono via AMQP.

**Observability** : contains the logs and plots for the study on system observability conducted in our paper.

**anomaly_detection** : contains the logs and plots for the real-time fault detection part of our paper.

**2sources** : contains the logs and plots of the three considered Data Send Intervals for PDR prediction of the simulated network.

**ProducerConsumerPN.ipynb** : this is the Timed Petri-net producer-consumer model that we developed. It requires the installation of SNAKES API with ''pip install SNAKES''.

**MQTT-python-clients.ipynb** : this is the python MQTT client that subscribes to the local MQTT broker and publishes to Hono MQTT adapter.

