'''
Parameters used for communication
'''
class RICCommsParams:

    def __init__(self, connParams: dict = {}, fileTransferParams: dict = {}) -> None:
        self.connParams = connParams
        self.fileTransferParams = fileTransferParams

    @property
    def conn(self) -> dict:
        return self.connParams

    @conn.setter
    def conn(self, connParams: dict) -> None:
        self.connParams = connParams

    @property
    def fileTransfer(self) -> dict:
        return self.fileTransferParams

    @fileTransfer.setter
    def fileTransfer(self, fileTransferParams: dict) -> None:
        self.fileTransferParams = fileTransferParams
