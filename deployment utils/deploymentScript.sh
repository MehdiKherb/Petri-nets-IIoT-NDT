#!/bin/bash

export PROJECT_ID=cloud2edge-370009
export VPC_NAME=eclipse-vpc


gcloud compute networks create $VPC_NAME --project=$PROJECT_ID --subnet-mode=auto --mtu=1460 --bgp-routing-mode=regional


gcloud compute firewall-rules create ${VPC_NAME}-allow-custom  --project=${PROJECT_ID}  --network=projects/${PROJECT_ID}/global/networks/${VPC_NAME} --direction=INGRESS --priority=65534 --source-ranges=10.128.0.0/9 --action=ALLOW --rules=all


gcloud compute firewall-rules create ${VPC_NAME}-allow-icmp --project=${PROJECT_ID} --network=projects/${PROJECT_ID}/global/networks/${VPC_NAME}  --direction=INGRESS --priority=65534 --source-ranges=0.0.0.0/0 --action=ALLOW --rules=icmp


gcloud compute firewall-rules create ${VPC_NAME}-allow-ssh --project=${PROJECT_ID} --network=projects/${PROJECT_ID}/global/networks/${VPC_NAME} --direction=INGRESS --priority=65534 --source-ranges=0.0.0.0/0 --action=ALLOW --rules=tcp:22

export ROUTER_NAME=eclipse-router
export NAT_NAME=eclipse-nat
export VPC_NAME=eclipse-vpc

gcloud compute routers create ${ROUTER_NAME} --project=${PROJECT_ID} --region=europe-west1 --network=${VPC_NAME}

gcloud compute routers nats create ${NAT_NAME} --router=${ROUTER_NAME} --auto-allocate-nat-external-ips --nat-all-subnet-ip-ranges --enable-logging --region europe-west1 --project ${PROJECT_ID}


gcloud beta container --project ${PROJECT_ID} clusters create eclipse-cluster --zone europe-west1-b --machine-type e2-standard-4 --image-type COS_CONTAINERD --disk-type 'pd-standard' --disk-size 300 --scopes "https://www.googleapis.com/auth/cloud-platform" --logging=SYSTEM,WORKLOAD --monitoring=SYSTEM --enable-private-nodes --master-ipv4-cidr 172.16.200.0/28 --enable-master-global-access --enable-ip-alias --network projects/${PROJECT_ID}/global/networks/${VPC_NAME} --subnetwork projects/${PROJECT_ID}/regions/europe-west1/subnetworks/${VPC_NAME} --no-enable-master-authorized-networks --addons HorizontalPodAutoscaling,HttpLoadBalancing,GcePersistentDiskCsiDriver --enable-autoprovisioning --min-cpu 1 --max-cpu 64 --min-memory 1 --max-memory 512 --autoprovisioning-scopes=https://www.googleapis.com/auth/cloud-platform --enable-autoprovisioning-autorepair --enable-autoprovisioning-autoupgrade --autoprovisioning-max-surge-upgrade 1 --autoprovisioning-max-unavailable-upgrade 0 --enable-vertical-pod-autoscaling --enable-shielded-nodes --tags http-server,https-server --node-locations europe-west1-b

#Set up Eclipse HONO and Ditto
gcloud container clusters get-credentials eclipse-cluster --zone europe-west1-b --project ${PROJECT_ID}

helm repo add eclipse-iot https://eclipse.org/packages/charts
NS=cloud2edge
RELEASE=c2e
kubectl create namespace $NS

helm install -n $NS --wait --timeout 15m --set hono.useLoadBalancer=true --set ditto.nginx.service.type=LoadBalancer --set hono.mqtt.authenticationRequired=false $RELEASE eclipse-iot/cloud2edge --version 0.2.4 --debug
