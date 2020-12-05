import sys
from PyQt5.QtCore import QTimer
from PyQt5.QtGui import QFont
from PyQt5.QtWidgets import (QGridLayout, QListWidget, QListWidgetItem, QPushButton, QTextEdit, QWidget, QMessageBox, 
    QApplication, QLabel)
import serial.tools.list_ports
from ReleaseMgr import ReleaseMgr
from SDCardListWidget import SDCardListWidget
from ESP32Programmer import ESP32Programmer

class RDUpdater(QWidget):
    programTitle = "Updater"

    def __init__(self):
        super().__init__()
        self.programming = False
        self.newSDCardList = []
        self.SDCardList = []
        self.serialPortList = []
        self.esp32Programmer = ESP32Programmer()
        self.initReleases()
        self.initUI()

    def initReleases(self):
        self.releaseMgr = ReleaseMgr("robdobsn", "BusRaider")
        self.releaseMgr.refresh()

    def initUI(self):

        # Layout and style
        grid = QGridLayout(self)
        headingFont = QFont()
        headingFont.setBold(True)

        # Releases list
        heading = QLabel("Select Release to Program")
        heading.setFont(headingFont)
        grid.addWidget(heading, 0, 0)
        self.releaseListWidget = QListWidget()
        for releaseTag in self.releaseMgr.getReleaseList():
            self.releaseListWidget.addItem(releaseTag)
        grid.addWidget(self.releaseListWidget, 1, 0)

        # Release notes
        self.releaseNotesBox = QTextEdit("Release notes", self)
        self.releaseNotesBox.setReadOnly(True)
        grid.addWidget(self.releaseNotesBox, 1, 1)

        # Serial Port List
        self.serialPortList = QListWidget()
        self.comPorts = serial.tools.list_ports.comports()
        for port in self.comPorts:
            self.serialPortList.addItem(port.device + ' - ' + port.description)
        heading = QLabel("Select ESP32 Serial Port")
        heading.setFont(headingFont)
        grid.addWidget(heading,3,0)
        grid.addWidget(self.serialPortList,4,0)
        for i, comPort in enumerate(self.comPorts):
            if "CP210" in comPort.description:
                self.serialPortList.setCurrentRow(i)
                break

        # UI update timer
        self.uiUpdateTimer = QTimer(self)
        self.uiUpdateTimer.timeout.connect(self.onUIUpdate)
        self.uiUpdateTimer.start(1000)

        # SD Cards
        self.sdCardListWidget = SDCardListWidget()
        self.sdCards = []
        heading = QLabel("Select SD Card")
        heading.setFont(headingFont)
        grid.addWidget(heading,3,1)
        grid.addWidget(self.sdCardListWidget,4,1)
        self.sdCardListWidget.setDisabled(True)

        # Program button and status
        self.buttonProg = QPushButton("Program ESP32", self)
        self.buttonProg.clicked.connect(self.onClickProgram)
        grid.addWidget(self.buttonProg, 5, 0)
        self.resultProg = QLabel("")
        grid.addWidget(self.resultProg, 6, 0)

        # Initial release
        self.releaseListWidget.currentItemChanged.connect(self.releaseSelChange)
        if len(self.releaseMgr.getReleaseList()) > 0:
            self.releaseListWidget.setCurrentRow(0)
 
        # Show
        self.setLayout(grid)
        self.setGeometry(300, 300, 600, 600)
        self.setWindowTitle('BusRaider Updater')
        self.show()

    def releaseSelChange(self, listObj: QListWidgetItem):
        # Change of release - show release notes
        releaseTxt = listObj.text()
        relObj = self.releaseMgr.setRelease(releaseTxt)
        self.releaseNotesBox.setText(relObj.get("body",""))
        self.sdCardListWidget.setDisabled(not self.releaseMgr.isSDUsed())

    def onUIUpdate(self):
        if not self.programming:
            # Serial list
            newSerialList = serial.tools.list_ports.comports()
            if len(newSerialList) != len(self.comPorts):
                self.comPorts = newSerialList
                selPort = ""
                if len(self.serialPortList.selectedItems()) > 0:
                    selPort = self.serialPortList.selectedItems()[0].text()
                self.serialPortList.clear()
                for port in self.comPorts:
                    self.serialPortList.addItem(port.device + ' - ' + port.description)
                for i in range(self.serialPortList.count()-1):
                    if self.serialPortList.item(i).text() == selPort:
                        self.serialPortList.setCurrentRow(i)
                        break

    def onClickProgram(self):
        self.resultProg.setText("")

        # Check port
        if len(self.serialPortList.selectedItems()) == 0:
            QMessageBox.warning(self, self.programTitle, "Please plug in a micro-USB cable to the ESP32 module and select the serial port that appears")
            return
        portText = self.serialPortList.selectedItems()[0].text()
        portSplit = portText.split()
        if len(portSplit) == 0:
            QMessageBox.warning(self, self.programTitle, "Please plug in a micro-USB cable to the ESP32 module and select the serial port that appears")
            return
        port = portSplit[0]

        # Check release
        if self.releaseMgr.releaseStr == "":
            QMessageBox.warning(self, self.programTitle, "Please select a release")
            return

        # Download release
        if not self.releaseMgr.downloadReleaseToTempFiles():
            QMessageBox.warning(self, self.programTitle, "Failed to download release files")
            return

        # Program ESP32
        rslt, excp = self.esp32Programmer.program(port, self.releaseMgr.infoOnFiles)
        if rslt:
            self.resultProg.setText("Programmed OK")
        else:
            QMessageBox.warning(self, self.programTitle, str(excp), QMessageBox.Ok, QMessageBox.Ok)

def main():
    app = QApplication(sys.argv)
    ex = RDUpdater()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
