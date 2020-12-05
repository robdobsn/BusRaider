from typing import Dict, List
import requests
from tempfile import NamedTemporaryFile
import os
import shutil

class ReleaseMgr():

    fileSpecDefault = {
        "firmware.bin": {"dest":"ESP32"},
        "partitions.bin": {"dest":"SD"},
        "start.elf": {"dest":"SD"},
        "kernel.img": {"dest":"SD"},
        "config.txt": {"dest":"SD"},
        "bootcode.bin": {"dest":"SD"},
    }

    def __init__(self, gitRepoOwner, gitRepoName) -> None:
        self.gitRepoOwner = gitRepoOwner
        self.gitRepoName = gitRepoName
        self.repoReleases = []
        self.infoOnFiles = []
        self.releaseStr = ""
        self.releaseInfo = None
        self.fileSpecs = self.fileSpecDefault

    def __del__(self):
        self.cleanUp()

    def cleanUp(self):
        for f in self.infoOnFiles:
            if "tempFileName" in f:
                os.remove(f.get("tempFileName",""))
        self.infoOnFiles = []

    def refresh(self) -> None:
        resp = requests.get(f"https://api.github.com/repos/{self.gitRepoOwner}/{self.gitRepoName}/releases")
        self.repoReleases = resp.json()

    def getReleaseList(self) -> List[str]:
        return [rel.get("tag_name","") for rel in self.repoReleases]

    def setRelease(self, releaseStr) -> Dict:
        self.releaseStr = ""
        self.releaseInfo = None
        for rel in self.repoReleases:
            if rel.get("tag_name","") == releaseStr:
                self.releaseStr = releaseStr
                self.releaseInfo = rel
                return rel
        return None

    def downloadAsset(self, assetURL):
        if assetURL == "":
            return None
        with requests.get(assetURL, stream=True) as r:
            r.raw.decode_content = True
            with NamedTemporaryFile(mode="wb", delete=False) as f:
                shutil.copyfileobj(r.raw, f)
                tmpFileName = f.name
        with open(tmpFileName, "rb") as ff:
            bbb = ff.read()
        return tmpFileName

    def downloadReleaseToTempFiles(self) -> bool:
        self.cleanUp()
        if not self.releaseInfo:
            return False
        assets = self.releaseInfo.get("assets",[])
        for asset in assets:
            tmpFileInfo = {}
            tmpFileName = self.downloadAsset(asset.get("url", ""))
            if tmpFileName != "":
                tmpFileInfo = {"name":asset.get("name",""),"url":asset.get("url",""), "tmpFileName":tmpFileName}
                self.infoOnFiles.append(tmpFileInfo)
        return True
        
    def isSDUsed(self) -> bool:
        assets = self.releaseInfo.get("assets",[])
        for asset in assets:
            assetName = asset.get("name","")
            if assetName != "" and assetName in self.fileSpecs:
                fileSpec = self.fileSpecs.get(assetName,{})
                if fileSpec.get("dest","") == "SD":
                    return True
        return False



