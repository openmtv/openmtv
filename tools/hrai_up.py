#!/usr/bin/env python3
"""The model's part of tools/hrai.py, for a machine with torch and spandrel (a
GPU if there is one): every IN/*.png upscaled by MODEL (.pth, loaded as
weights only) into OUT/ at 4x; a model of scale 8 is brought to 4x by
averaging 2x2 sub-pixels.

  hrai_up.py MODEL IN OUT
"""
import os
import sys
import time

import numpy as np
import torch
from PIL import Image
from spandrel import ModelLoader


def main(model, src, dst):
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    net = ModelLoader().load_from_state_dict(torch.load(model, map_location='cpu', weights_only=True)).eval().to(dev)
    k = net.scale
    print('%s: %s, scale %d, %s' % (os.path.basename(model), net.architecture.name, k, dev), flush=True)
    os.makedirs(dst, exist_ok=True)
    names = sorted(f for f in os.listdir(src) if f.endswith('.png'))
    t = time.time()
    for f in names:
        img = np.asarray(Image.open(os.path.join(src, f)).convert('RGB'))
        x = torch.from_numpy(img.copy()).permute(2, 0, 1).float().div(255).unsqueeze(0).to(dev)
        with torch.inference_mode():
            # (the network itself, without spandrel's padding to a size it requires: the
            # ESRGANs here take any size -- a model with such needs would want net(x))
            y = net.model(x).clamp(0, 1)[0].permute(1, 2, 0).float().cpu().numpy()
        if k == 8:
            h, w = y.shape[0] // 2, y.shape[1] // 2
            y = y.reshape(h, 2, w, 2, 3).mean((1, 3))
        elif k != 4:
            sys.exit('%s: scale %d' % (model, k))
        Image.fromarray((y * 255 + 0.5).astype(np.uint8)).save(os.path.join(dst, f))
    print('%d pictures in %.0f s' % (len(names), time.time() - t), flush=True)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
