#!/usr/bin/env python3

import unittest

from python_structure_manager_test import TestStructureManagerCell

class SimpleCheck(unittest.TestCase):
    def setUp(self):
        self.truth = True


    def test_simple_example(self):
        self.assertTrue(self.truth)


if __name__ == '__main__':
    unittest.main()

