#!/usr/bin/env python3
"""Accept only actual guest GOP and Linux framebuffer pixels on host scanout."""
from pathlib import Path
import argparse,hashlib,json,re
from PIL import Image
from analyze_managed_firmware import analyze as lifecycle_analyze

def pixels(path,expected):
    with Image.open(path) as im:
        im=im.convert('RGB');assert im.width>=1024 and im.height>=768
        samples=[]
        for (x,y),color in zip([(200,150),(600,150),(200,450),(600,450)],expected):
            actual=im.getpixel((x,y));assert actual==color,(path,(x,y),actual,color)
            samples.append({'x':x,'y':y,'rgb':actual})
        image_sha=hashlib.sha256(im.tobytes()).hexdigest()
        im.save(path.with_suffix('.png'))
    return {'file':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'rgb_sha256':image_sha,'samples':samples}

def analyze(root):
    life=lifecycle_analyze(root)
    text=(root/'native/positive-once/serial.log').read_text().replace('\r','')
    assert text.count('SYLE_GOP_PATTERN_READY 800x600 BGRX')==2
    assert text.count('SYLE_GOP_HANDOFF_READY')==2
    assert text.count('SYLE_FB_PATTERN_READY 800x600 BGRX')==1
    assert text.count('SYLE_FB_HANDOFF_VERIFIED')==1
    assert 'SYLE_GOP_FAILED' not in text and 'SYLE_FB_ERROR' not in text
    assert 'efifb: framebuffer at' in text and 'fb0: EFI VGA frame buffer device' in text
    assert 'Linux version 6.12.0-syle-gfx1' in text
    config=(root/'inputs/linux.config').read_text()
    for item in ['CONFIG_INITRAMFS_SOURCE=""','CONFIG_FB_EFI=y','CONFIG_FB_DEVICE=y','CONFIG_FRAMEBUFFER_CONSOLE=y']:assert item in config,item
    from analyze_managed_firmware import records
    graphics=records(text,'GFX.RELEASE ',['generation','configs','dma','frames','pixels','last_hash','scanout_revoked'])
    assert len(graphics)==2
    for gen,g in enumerate(graphics,1):
        assert g['generation']==gen and g['configs']>=2 and g['dma']>0 and g['frames']>0 and g['pixels']>0 and g['last_hash'] and g['scanout_revoked']==1
    capture=json.loads((root/'native/positive-once/captures.json').read_text());assert set(capture)=={'gop-1','gop-2','linux-1'}
    pictures=[]
    for label,colors in [('gop-1',[(255,0,0),(0,255,0),(0,0,255),(255,255,255)]),('gop-2',[(255,0,0),(0,255,0),(0,0,255),(255,255,255)]),('linux-1',[(0,255,255),(255,0,255),(255,255,0),(32,32,32)])]:
        path=root/'native/positive-once'/(label+'.ppm');assert capture[label]==str(path.resolve());pictures.append(pixels(path,colors))
    result={'validation':'PASS','acceptance':'actual guest GOP, Linux EFI framebuffer handoff and host scanout; preserved Ring3 disk lifecycle and complete resource reclaim','lifecycle':life,'graphics':graphics,'pictures':pictures}
    (root/'graphics-analysis.json').write_text(json.dumps(result,indent=2));return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);a=p.parse_args();print(json.dumps(analyze(a.root),indent=2))
