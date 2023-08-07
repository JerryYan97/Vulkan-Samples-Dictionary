import requests
import os

if __name__ == "__main__":
    url = "https://www.dropbox.com/scl/fi/7ezrw19c2kr2srvyn6qxj/uvNormalSphere.obj?rlkey=vual09zy4zsjvlhabx4nhbuy1&dl=1"
    r = requests.get(url) # create HTTP response object

    scriptPathName = os.path.realpath(__file__)
    idx = scriptPathName.rfind('\\')
    scriptPath = scriptPathName[:idx]

    if(not os.path.exists(scriptPath + "\\data")):
        os.mkdir(scriptPath + "\\data")

    with open(scriptPath + "\\data\\" + "uvNormalSphere.obj",'wb') as f:
        f.write(r.content)