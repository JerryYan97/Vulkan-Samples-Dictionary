import requests
import os

if __name__ == "__main__":
    url = "https://www.dropbox.com/s/u13zuwdxy0fpgzc/little_paris_eiffel_tower_4k.hdr?dl=1"
    r = requests.get(url) # create HTTP response object

    scriptPathName = os.path.realpath(__file__)
    idx = scriptPathName.rfind('\\')
    scriptPath = scriptPathName[:idx]

    if(not os.path.exists(scriptPath + "\\data")):
        os.mkdir(scriptPath + "\\data")

    with open(scriptPath + "\\data\\" + "little_paris_eiffel_tower_4k.hdr",'wb') as f:
        f.write(r.content)