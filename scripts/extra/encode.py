import requests
import ujson as json
import sys
import binascii

to_address = "TRTLv3xYqUdAy4K8viYjNnMj21NLohHbf9ut2Cczxyh96d74TzxNgdB3aZbb9U2ZJ1DVmVpbDwzH77821o9ciNYQVaSt3V6bu7R"
content = """
  _____     ____
 /      \  |  o |
|        |/ ___\|
|_________/
|_|_| |_|_|
"""

def rpc(method, params={}):
    base_url = "http://localhost:8070/json_rpc"
    payload = {
        "password": "80085",
        "jsonrpc" : "2.0",
        "method" : method,
        "params" : params,
        "id" : "blah"
    }

    try:
        response = requests.post(base_url, data=json.dumps(payload)).json()
    except Exception as e:
        print("Doesn't seem like turtle-service is running. {}".format(response))
        sys.exit(1)

    if 'error' in response:
        print("Failed to talk to server. {}".format(response))
        sys.exit(1)
    return response

r = rpc("sendTransaction", {
    "transfers": [{
        "amount": 1,
        "address": to_address,
    }],
    "fee": 10,
    "anonymity": 3,
    "extra": binascii.hexlify(content.encode()).decode()
})
print(r)
