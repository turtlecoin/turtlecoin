#!/usr/bin/env python3

'''
Usage: python3 makechange.py
Need python 3.6 as that is when secrets got added, so you might need to launch it with python3 makechange.py

Have Ubuntu? n.04 add ppa, n.10 might have already check python3 -V

$ sudo add-apt-repository ppa:deadsnakes/ppa
$ sudo apt update
$ sudo apt install python3.6
$ python3.6 makechange.py

Make two wallets and fill one or both with some funds, or start mining to it.
Open the wallets with turtle-service like so:

./turtle-service -w walletA.wallet -p yourpass --rpc-password test --bind-port 8070
./turtle-service -w walletB.wallet -p yourpass --rpc-password test --bind-port 8071

Feel free to change these parameters if needed of course.

This script rapidly sends random amount of funds from two wallets to each
other, hopefully generating change on a new network.

For Forks, see comments for adjusting amounts especially if using more than 2 decimals.
'''

import requests
import json
import secrets
import time
import sys
from threading import Thread

# Forks adjust as needed
moveDecimal = 100  # TRTL has 2 decimals so 100 is the divide/multiply factor
minAmount = 100 * moveDecimal  # min number for amount to xfer
maxAmount = 5000 * moveDecimal  # max number for amount to xfer
anonymity = 3
fee = 10 # atomic units, TRTL would be 0.10 as the tx network fee

def getAddress(host, port, rpcPassword):
    payload = {
        'jsonrpc': '2.0',
        'method': "getAddresses",
        'password': rpcPassword,
        'id': 'test',
        'params': {}
    }

    url = 'http://' + host + ':' + port + '/json_rpc'

    response = requests.post(
        url, data=json.dumps(payload),
        headers={'content-type': 'application/json'}
    ).json()

    if 'error' in response:
        print(response['error'])
        print('Failed to get address, exiting')
        sys.exit()
    else:
        return response['result']['addresses'][0]


def sendTransaction(host, port, rpcPassword, **kwargs):
    payload = {
        'jsonrpc': '2.0',
        'method': "sendTransaction",
        'password': rpcPassword,
        'id': 'test',
        'params': kwargs
    }

    url = 'http://' + host + ':' + port + '/json_rpc'

    response = requests.post(
        url, data=json.dumps(payload),
        headers={'content-type': 'application/json'}
    ).json()
    if 'error' in response:
        response['error']['amount'] =  kwargs['transfers'][0]['amount']/moveDecimal
        print(response['error'])
        return False
    else:
        response['result']['amount'] =  kwargs['transfers'][0]['amount']/moveDecimal
        print(response['result'])
        return True


def sendTXs(host, port, rpcPassword, sender, receiver):
    sleepAmount = 0.01

    while True:
        amount = minAmount+ secrets.randbelow(maxAmount)

        params = {'transfers': [{'address': receiver, 'amount': amount}],
                  'fee': fee,
                  'anonymity': anonymity,
                  'changeAddress': sender}

        if not sendTransaction(host, port, rpcPassword, **params):
            time.sleep(sleepAmount)
            print("Sleeping for " + str(sleepAmount) + " seconds...")
            sleepAmount *= 2
        else:
            sleepAmount = 0.01


walletdHostA = "127.0.0.1"
walletdPortA = "8070"

walletdHostB = "127.0.0.1"
walletdPortB = "8071"

rpcPasswordA = "test"
rpcPasswordB = "test"

addressA = getAddress(walletdHostA, walletdPortA, rpcPasswordA)
addressB = getAddress(walletdHostB, walletdPortB, rpcPasswordB)

Thread(target=sendTXs, args=(walletdHostA, walletdPortA, rpcPasswordA,
       addressA, addressB)).start()

Thread(target=sendTXs, args=(walletdHostB, walletdPortB, rpcPasswordB,
       addressB, addressA)).start()
