#!/usr/bin/env python3

'''
Usage: python makechange.py
This is python3, so you might need to launch it with python3 makechange.py

Make two wallets and fill one or both with some funds, or start mining to it.
Open the wallets with walletd like so:

./walletd -w walletA.wallet -p yourpass --rpc-password test --bind-port 8070
./walletd -w walletB.wallet -p yourpass --rpc-password test --bind-port 8071

Feel free to change these parameters if needed of course.

This script rapidly sends random amount of funds from two wallets to each
other, hopefully generating change on a new network.
'''

import requests
import json
import random
import time
import sys
from threading import Thread


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
        print(response['error'])
        return False
    else:
        print(response['result'])
        return True


def sendTXs(host, port, rpcPassword, sender, receiver):
    def loop():
        n = 1000
        while(n < 100000000000):
            yield n
            n *= 10

    sleepAmount = 0.001

    while True:
        for i in loop():
            # give it a bit more randomness, maybe this helps
            amount = random.randint(i, i+10000)

            params = {'transfers': [{'address': receiver, 'amount': amount}],
                      'fee': 10,
                      'anonymity': 5,
                      'changeAddress': sender}

            if not sendTransaction(host, port, rpcPassword, **params):
                time.sleep(sleepAmount)
                print("Sleeping for " + str(sleepAmount) + " seconds...")
                sleepAmount *= 2
                break
            else:
                sleepAmount = 0.001


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
