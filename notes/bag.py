import cbag

class Bag:
        def __init__(self):
                self.my_bag = cbag.init()

        def set(self, index, new_val):
                cbag.set(self.my_bag, index, new_val)

        def get(self, index):
                return cbag.get(self.my_bag, index)
