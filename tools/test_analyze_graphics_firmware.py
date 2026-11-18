#!/usr/bin/env python3
"""Synthetic scanout checks: wrong channel order, swapped panes, blank frames."""
import tempfile
import unittest
from pathlib import Path
from PIL import Image
from analyze_graphics_firmware import pixels
COLORS=[(255,0,0),(0,255,0),(0,0,255),(255,255,255)]
class GraphicsAcceptance(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory();self.addCleanup(self.directory.cleanup)
        self.path=Path(self.directory.name)/'screen.ppm';im=Image.new('RGB',(1280,800))
        for y in range(2):
            for x in range(2):im.paste(COLORS[y*2+x],(x*400,y*300,(x+1)*400,(y+1)*300))
        im.save(self.path)
    def test_expected(self):self.assertEqual(len(pixels(self.path,COLORS)['samples']),4)
    def test_wrong_channels(self):
        with self.assertRaises(AssertionError):pixels(self.path,[(0,0,255),(0,255,0),(255,0,0),(255,255,255)])
    def test_wrong_panes(self):
        with self.assertRaises(AssertionError):pixels(self.path,COLORS[::-1])
    def test_blank(self):
        Image.new('RGB',(1280,800)).save(self.path)
        with self.assertRaises(AssertionError):pixels(self.path,COLORS)
    def test_small(self):
        Image.new('RGB',(640,480)).save(self.path)
        with self.assertRaises(AssertionError):pixels(self.path,COLORS)
if __name__=='__main__':unittest.main()
