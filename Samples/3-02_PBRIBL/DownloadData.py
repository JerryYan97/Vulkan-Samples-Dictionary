import requests
import os

if __name__ == "__main__":
    url = "https://www.dropbox.com/scl/fi/lnm3vgr3plmily1qkvtwp/output_skybox.hdr?rlkey=k3a5gerogb9pyje6cfnj0tqtn&dl=1"
    r = requests.get(url) # create HTTP response object

    scriptPathName = os.path.realpath(__file__)
    idx = scriptPathName.rfind('\\')
    scriptPath = scriptPathName[:idx]

    if(not os.path.exists(scriptPath + "\\data")):
        os.mkdir(scriptPath + "\\data")

    with open(scriptPath + "\\data\\" + "output_skybox.hdr",'wb') as f:
        f.write(r.content)