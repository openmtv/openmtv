#!/usr/bin/env python3
"""The models as ONNX, for mtv-dec --make-ki (docs/ki-bilder.md, stage 2): each
SRC .pth (OpenModelDB's file, loaded as weights only) exported as DST .onnx with
PyTorch's former exporter (opset 17), the input named "input" and the output
"output", any height and width.  The files on https://openmtv.net/ki/1/models/
came from this with spandrel 0.4.2 and torch 2.11 (CPU); a new model or a new
set of files goes to ki/2/ (launcher/ki.go: their sizes and SHA-256).

  hrai_onnx.py SRC.pth DST.onnx [SRC.pth DST.onnx ...]
"""
import hashlib
import os
import sys

import torch
from spandrel import ModelLoader

for src, dst in zip(sys.argv[1::2], sys.argv[2::2]):
    net = ModelLoader().load_from_state_dict(torch.load(src, map_location='cpu', weights_only=True)).eval()
    x = torch.rand(1, 3, 40, 56)
    torch.onnx.export(net.model, x, dst, dynamo=False, opset_version=17, input_names=['input'], output_names=['output'],
                      dynamic_axes={'input': {2: 'h', 3: 'w'}, 'output': {2: 'H', 3: 'W'}})
    print(dst, os.path.getsize(dst), hashlib.sha256(open(dst, 'rb').read()).hexdigest()[:16], 'scale', net.scale)
