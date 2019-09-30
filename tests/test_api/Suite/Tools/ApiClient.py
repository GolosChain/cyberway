import requests
import json

class ApiClient:
    def __init__(self, host, port, apiClientDialog):
        self.url = "http://" + host + ":" + str(port) + "/v1/chain/"
        self.apiClientDialog = apiClientDialog

    def sendRequest(self, function, params):
        requestUrl = self.url + function
        jsonParams = json.dumps(params)
        if self.apiClientDialog == True:
            print(requestUrl + " " + jsonParams)

        response = requests.post(url=requestUrl, data=jsonParams)
        if response.status_code == 200 or response.status_code == 202:
            object = json.loads(response.content)
            if self.apiClientDialog == True:
                print(object)

            return object

        raise NameError("The request executed with error: " + str(json.loads(response.content)))