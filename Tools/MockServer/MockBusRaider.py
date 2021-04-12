from MockImager import MockImager

class MockBusRaider:

    def __init__(self):
        self.mockImager = MockImager()

    def imager(self):
        return self.mockImager
        