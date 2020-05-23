#!/usr/bin/env python3

from testUtils import Utils
from Cluster import Cluster

import argparse

parser = argparse.ArgumentParser(add_help=False)
parser.add_argument('-?', action='help', default=argparse.SUPPRESS,
                    help=argparse._('show this help message and exit'))
parser.add_argument("--defproducera_prvt_key", type=str, help="defproducera private key.")
parser.add_argument("--defproducerb_prvt_key", type=str, help="defproducerb private key.")

args = parser.parse_args()
defproduceraPrvtKey=args.defproducera_prvt_key
defproducerbPrvtKey=args.defproducerb_prvt_key

cluster=Cluster(walletd=False, enableMongo=False, defproduceraPrvtKey=defproduceraPrvtKey, defproducerbPrvtKey=defproducerbPrvtKey)

init_str='{"nodes":['
init_str=init_str+'{"host":"localhost", "port":8001}'
init_str=init_str+','
init_str=init_str+'{"host":"localhost", "port":8002}'
init_str=init_str+','
init_str=init_str+'{"host":"localhost", "port":8003}'
init_str=init_str+']}'

cluster.initializeNodesFromJson(init_str);

nodes=[]
for i in range(totalNodes):
    nodes.append(cluster.getNode(i))

if nodes[2].getConnections() >= 2:
    errorExit("address exchange not working - node not connecting to nodes which aren't know to it")
