#!/usr/bin/env python2
# -*- coding: utf-8
import sys

K = [0x5A827999L, 0x6ED9EBA1L, 0x8F1BBCDCL, 0xCA62C1D6L]

def S(x,k):
    return ((x<<k)|(x>>(32-k)))&0xffffffffL;

def int2str(num, l):
    return ''.join([chr((num>>((l-i-1)*8))&0xff) for i in range(l)])

def sha1(string):
    H = [0x67452301L, 0xEFCDAB89L, 0x98BADCFEL, 0x10325476L, 0xC3D2E1F0L]
    l = len(string)*8 # Longueur du texte
    z = (448-(l+1))%512 # nombre de zero a ajouter pour obtenir une langueur de 512n-64
    info = (1<<(z+64))|l # 1 suivi de z fois 0 et de l sur 64bits
    string += int2str(info, (1+z+64)/8)
    
    for n in range(len(string)/64):
        M = string[n*64:(n+1)*64]
        W = [ (ord(M[t*4+0])<<24) |  (ord(M[t*4+1])<<16) | (ord(M[t*4+2])<<8) | ord(M[t*4+3]) for t in range(16) ]
        for i in range(16, 80):
            W.append(S(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1))
        [a, b, c, d, e] = H
        
        for t in range(0, 20):
            tmp = S(a, 5) + ((b & c) | ((~ b) & d)) + e + W[t] + K[0]
            e = d
            d = c
            c = S(b, 30)
            b = a
            a = tmp & 0xffffffffL
        
        for t in range(20, 40):
            tmp = S(a, 5) + (b ^ c ^ d) + e + W[t] + K[1]
            e = d
            d = c
            c = S(b, 30)
            b = a
            a = tmp & 0xffffffffL
        
        for t in range(40, 60):
            tmp = S(a, 5) + ((b & c) | (b & d) | (c & d)) + e + W[t] + K[2]
            e = d
            d = c
            c = S(b, 30)
            b = a
            a = tmp & 0xffffffffL
        
        for t in range(60, 80):
            tmp = S(a, 5) + (b ^ c ^ d) + e + W[t] + K[3]
            e = d
            d = c
            c = S(b, 30)
            b = a
            a = tmp & 0xffffffffL
        
        H = [(H[0]+a) & 0xffffffffL,
             (H[1]+b) & 0xffffffffL,
             (H[2]+c) & 0xffffffffL,
             (H[3]+d) & 0xffffffffL,
             (H[4]+e) & 0xffffffffL]
    
    return hex((H[0]<<128 | H[1]<<96 | H[2]<<64 | H[3]<<32 | H[4]))[2:-1].zfill(40)

if len(sys.argv) > 1:
    print(sha1(sys.argv[1]))
else:
    print("Usage: %s \"string to hash\""%sys.argv[0])
