import json

from PyQt5.QtWidgets import QListWidget
import psutil

from PyQt5.QtCore import QThread, pyqtSignal

class SDUpdateThread(QThread):
    sig = pyqtSignal(str)
    def __init__(self, parent=None) -> None:
        super().__init__()
        self.parent = parent
    def getSDCards(self):
        partitions = psutil.disk_partitions()
        sdCards = []
        for partition in partitions:
            if "removable" in partition.opts and partition.fstype != "":
                sdCards.append(partition)
        return sdCards
    def run(self):
        sdCards = self.getSDCards()
        if len(sdCards) > 0:
            sdCardJson = json.dumps(sdCards)
            self.sig.emit(sdCardJson)

class SDCardListWidget(QListWidget):
    def __init__(self) -> None:
        super().__init__()
        self.thread = SDUpdateThread(self)
        self.thread.sig.connect(self.update)
        self.thread.run()

    def update(self, newListJSON):
        self.clear()
        newList = json.loads(newListJSON)
        for item in newList:
            self.addItem(item[0])
