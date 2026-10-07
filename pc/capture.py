#!/usr/bin/env python3
"""Capture labeled 32x32 grayscale PNGs via USB CDC."""
import argparse
from pathlib import Path
import serial
from PIL import Image

def read_exact(ser, count):
    data=ser.read(count)
    if len(data)!=count: raise TimeoutError(f"Expected {count} bytes, got {len(data)}")
    return data

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--port', required=True, help='e.g. /dev/ttyACM0')
    ap.add_argument('--label', required=True, choices=['circle','square','triangle'])
    ap.add_argument('--out', default='dataset/train')
    ap.add_argument('--count', type=int, default=20)
    args=ap.parse_args()
    folder=Path(args.out)/args.label;folder.mkdir(parents=True,exist_ok=True)
    with serial.Serial(args.port,115200,timeout=8) as ser:
        ser.reset_input_buffer()
        ser.write(b'I')
        status=read_exact(ser,1)
        if status!=b'1': raise RuntimeError('Camera adapter not ready: integrate Waveshare driver first')
        for i in range(args.count):
            input(f'Place {args.label} in view; Enter for image {i+1}/{args.count}...')
            ser.reset_input_buffer();ser.write(b'C')
            start=read_exact(ser,1)
            if start==b'E': raise RuntimeError('Camera capture failed')
            if start!=b'\xaa' or read_exact(ser,1)!=b'\x55': raise RuntimeError('Invalid frame header')
            size=int.from_bytes(read_exact(ser,2),'little')
            if size!=1024: raise RuntimeError(f'Unexpected frame size: {size}')
            frame=read_exact(ser,size);checksum=read_exact(ser,1)[0]
            actual=0
            for pixel in frame: actual^=pixel
            if actual!=checksum: raise RuntimeError('Frame checksum mismatch')
            name=folder/f'{args.label}_{len(list(folder.glob("*.png"))):05d}.png'
            Image.frombytes('L',(32,32),frame).save(name)
            print(name)
if __name__=='__main__':main()
