'''
Usage: python makechange.py
This is python3, so you might need to launch it with python3 makechange.py

Make a wallet and fill it with some funds, or start mining to it.
Open the wallet with walletd like so:

walletd -w yourwalletfilename -p yourwalletpassword --rpc-password test

Fill in the address variable, and change the walletd port and address if needed.

Make sure the address variable is correct, because if it isn't, you will
rapidly be sending your funds to this address.

This script rapidly sends random amount of funds from your wallet back to you,
hopefully generating change on a new network.
'''

import requests
import json
import random
import time

address = "Fill me in!"

if len(address) != 99:
    print("Please fill in your address and re-run the script.")
    quit()

walletdPort = "8070"
walletdAddress = "127.0.0.1"

rpcPassword = "test"

def make_request(method, **kwargs):
    payload = {
        'jsonrpc': '2.0',
        'method': method,
        'password': rpcPassword,
        'id': 'test',
        'params': kwargs
    }

    url = f'http://{walletdAddress}:{walletdPort}/json_rpc'

    response = requests.post(url, data=json.dumps(payload),
                             headers={'content-type': 'application/json'}).json()

    if 'error' in response:
        print(response['error'])
        return False
    else:
        print(response['result'])
        return True

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

        params = {'transfers': [{'address': address, 'amount': amount}],
                  'fee': 10,
                  'anonymity': 5}

        if not make_request("sendTransaction", **params):
            time.sleep(sleepAmount)
            print("Sleeping for " + str(sleepAmount) + " seconds...")
            sleepAmount *= 2
            break
        else:
            sleepAmount = 0.001
