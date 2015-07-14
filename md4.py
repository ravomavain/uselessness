#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import psutil
import os
from sympy import symbols, true, false, And, Or, Xor, Not, simplify, satisfiable
from optparse import OptionParser

def memory_usage():
    process = psutil.Process(os.getpid())
    mem = process.memory_info()[0] / float(2 ** 20)
    return mem

class byte:
    SIZE = 32
    def __init__(self, bits):
        if type(bits) == int:
            bits = [true if i == "1" else false for i in '{:032b}'.format(bits)]
        while len(bits) < self.SIZE:
            bits.insert(0,false)
        assert(len(bits) == self.SIZE)
        self.bits = bits
    def op(self,o,other):
        return byte([o(self.bits[i], other.bits[i]) for i in range(self.SIZE)])
    def iop(self,o,other):
        for i in range(self.SIZE):
            self.bits[i] = o(self.bits[i], other.bits[i])
        return self
    def __or__(self,other):
        return self.op(Or,other)
    def __ior__(self,other):
        return self.iop(Or,other)
    def __and__(self,other):
        return self.op(And,other)
    def __iand__(self,other):
        return self.iop(And,other)
    def __xor__(self,other):
        return self.op(Xor,other)
    def __ixor__(self,other):
        return self.iop(Xor,other)
    def __invert__(self):
        return byte([Not(self.bits[i]) for i in range(self.SIZE)])
    def __lshift__(self,other):
        return byte([self.bits[(i+other)%self.SIZE] for i in range(self.SIZE)])
    def __ilshift__(self,other):
        tmp = [self.bits[(i+other)%self.SIZE] for i in range(self.SIZE)]
        for i in range(self.SIZE):
            self.bits[i] = tmp[i]
        return self
    def __rshift__(self,other):
        return byte([self.bits[(i+(self.SIZE-other))%self.SIZE] for i in range(self.SIZE)])
    def __irshift__(self,other):
        tmp = [self.bits[(i+(self.SIZE-other))%self.SIZE] for i in range(self.SIZE)]
        for i in range(self.SIZE):
            self.bits[i] = tmp[i]
        return self
    def __add__(self,other):
        l = []
        s = self.bits[self.SIZE-1] ^ other.bits[self.SIZE-1]
        c = self.bits[self.SIZE-1] & other.bits[self.SIZE-1]
        l.insert(0, s)
        for i in range(self.SIZE-2,-1,-1):
            s = self.bits[i] ^ other.bits[i] ^ c
            c = ( self.bits[i] & other.bits[i] ) | ( c & (self.bits[i] ^ other.bits[i]) )
            l.insert(0,s)
        return byte(l)
    def __iadd__(self,other):
        s = self.bits[self.SIZE-1] ^ other.bits[self.SIZE-1]
        c = self.bits[self.SIZE-1] & other.bits[self.SIZE-1]
        self.bits[self.SIZE-1] = s
        for i in range(self.SIZE-2,-1,-1):
            s = self.bits[i] ^ other.bits[i] ^ c
            c = ( self.bits[i] & other.bits[i] ) | ( c & (self.bits[i] ^ other.bits[i]) )
            self.bits[i] = s
        return self
    def __sub__(self, other):
        return self + ~(other) + byte(0x1)
    def __isub__(self, other):
        self += ~(other) + byte(0x1)
        return self
    def __getitem__(self,index):
        return self.bits[index]
    def __setitem__(self,index,value):
        self.bits[index] = value
    def __str__(self):
        return "\n".join([str(b) for b in self.bits])
    def __repr__(self):
        return self.bits.__repr__()
    def copy(self):
        return byte([self.bits[i] for i in range(self.SIZE)])
    def subs(self, *args, **kwargs):
        for i in range(self.SIZE):
            self.bits[i] = self.bits[i].subs(*args, **kwargs)
    def simplify(self):
        for i in range(self.SIZE):
            self.bits[i] = simplify(self.bits[i])
    def subs(self, variables):
        for i in range(self.SIZE):
            self.bits[i] = self.bits[i].subs(variables)
    def toHex(self):
        return "{:08X}".format(int("".join(["1" if self.bits[(i-8)+j] == True else "0" for i in range(self.SIZE,0,-8) for j in range(8)]),2))
    def toInt(self):
        return int("".join(["1" if self.bits[i] == True else "0" for i in range(self.SIZE)]),2)
        

class md4:
    SQRT2 = byte(0x5a827999)
    SQRT3 = byte(0x6ed9eba1)
    INITA = byte(0x67452301)
    INITB = byte(0xefcdab89)
    INITC = byte(0x98badcfe)
    INITD = byte(0x10325476)
    def __init__(self, message, verbose=False):
        self.m = message
        self.verbose = verbose
    
    def step(self, f, a, b, c, d, x, s):
        print("Step {} ({}MB)".format(self.cnt, memory_usage()))
        self.cnt += 1
        a += f(b, c, d) + x
        a <<= s
        if self.verbose:
            print(self)
        return a
        #a.simplify()
    def rstep(self, f, a, b, c, d, x, s):
        print("Step {} ({}MB)".format(self.cnt, memory_usage()))
        self.cnt -= 1
        a >>= s
        a -= f(b, c, d) + x
        if self.verbose:
            print(self)
        return a
        #a.simplify()
    def F(self, x,y,z):
        return (z ^ (x & (y ^ z)))
    def G(self, x,y,z):
        return ((x & (y | z)) | (y & z))
    def H(self, x,y,z):
        return (x ^ y ^ z)
    def compute(self):
        self.cnt = 1
        self.a = self.INITA.copy()
        self.b = self.INITB.copy()
        self.c = self.INITC.copy()
        self.d = self.INITD.copy()
        if self.verbose:
            print(self)
        
        # Round 1
        self.step(self.F, self.a, self.b, self.c, self.d, self.m[0], 3)
        self.step(self.F, self.d, self.a, self.b, self.c, self.m[1], 7)
        self.step(self.F, self.c, self.d, self.a, self.b, self.m[2], 11)
        self.step(self.F, self.b, self.c, self.d, self.a, self.m[3], 19)
        self.step(self.F, self.a, self.b, self.c, self.d, self.m[4], 3)
        self.step(self.F, self.d, self.a, self.b, self.c, self.m[5], 7)
        self.step(self.F, self.c, self.d, self.a, self.b, self.m[6], 11)
        self.step(self.F, self.b, self.c, self.d, self.a, self.m[7], 19)
        self.step(self.F, self.a, self.b, self.c, self.d, self.m[8], 3)
        self.step(self.F, self.d, self.a, self.b, self.c, self.m[9], 7)
        self.step(self.F, self.c, self.d, self.a, self.b, self.m[10], 11)
        self.step(self.F, self.b, self.c, self.d, self.a, self.m[11], 19)
        self.step(self.F, self.a, self.b, self.c, self.d, self.m[12], 3)
        self.step(self.F, self.d, self.a, self.b, self.c, self.m[13], 7)
        self.step(self.F, self.c, self.d, self.a, self.b, self.m[14], 11)
        self.step(self.F, self.b, self.c, self.d, self.a, self.m[15], 19)
        
        # Round 2
        self.step(self.G, self.a, self.b, self.c, self.d, self.m[0] + self.SQRT2, 3)
        self.step(self.G, self.d, self.a, self.b, self.c, self.m[4] + self.SQRT2, 5)
        self.step(self.G, self.c, self.d, self.a, self.b, self.m[8] + self.SQRT2, 9)
        self.step(self.G, self.b, self.c, self.d, self.a, self.m[12] + self.SQRT2, 13)
        self.step(self.G, self.a, self.b, self.c, self.d, self.m[1] + self.SQRT2, 3)
        self.step(self.G, self.d, self.a, self.b, self.c, self.m[5] + self.SQRT2, 5)
        self.step(self.G, self.c, self.d, self.a, self.b, self.m[9] + self.SQRT2, 9)
        self.step(self.G, self.b, self.c, self.d, self.a, self.m[13] + self.SQRT2, 13)
        self.step(self.G, self.a, self.b, self.c, self.d, self.m[2] + self.SQRT2, 3)
        self.step(self.G, self.d, self.a, self.b, self.c, self.m[6] + self.SQRT2, 5)
        self.step(self.G, self.c, self.d, self.a, self.b, self.m[10] + self.SQRT2, 9)
        self.step(self.G, self.b, self.c, self.d, self.a, self.m[14] + self.SQRT2, 13)
        self.step(self.G, self.a, self.b, self.c, self.d, self.m[3] + self.SQRT2, 3)
        self.step(self.G, self.d, self.a, self.b, self.c, self.m[7] + self.SQRT2, 5)
        self.step(self.G, self.c, self.d, self.a, self.b, self.m[11] + self.SQRT2, 9)
        self.step(self.G, self.b, self.c, self.d, self.a, self.m[15] + self.SQRT2, 13)
        
        # Round 3
        self.step(self.H, self.a, self.b, self.c, self.d, self.m[0] + self.SQRT3, 3)
        self.step(self.H, self.d, self.a, self.b, self.c, self.m[8] + self.SQRT3, 9)
        self.step(self.H, self.c, self.d, self.a, self.b, self.m[4] + self.SQRT3, 11)
        self.step(self.H, self.b, self.c, self.d, self.a, self.m[12] + self.SQRT3, 15)
        self.step(self.H, self.a, self.b, self.c, self.d, self.m[2] + self.SQRT3, 3)
        self.step(self.H, self.d, self.a, self.b, self.c, self.m[10] + self.SQRT3, 9)
        self.step(self.H, self.c, self.d, self.a, self.b, self.m[6] + self.SQRT3, 11)
        self.step(self.H, self.b, self.c, self.d, self.a, self.m[14] + self.SQRT3, 15)
        self.step(self.H, self.a, self.b, self.c, self.d, self.m[1] + self.SQRT3, 3)
        self.step(self.H, self.d, self.a, self.b, self.c, self.m[9] + self.SQRT3, 9)
        self.step(self.H, self.c, self.d, self.a, self.b, self.m[5] + self.SQRT3, 11)
        self.step(self.H, self.b, self.c, self.d, self.a, self.m[13] + self.SQRT3, 15)
        self.step(self.H, self.a, self.b, self.c, self.d, self.m[3] + self.SQRT3, 3)
        self.step(self.H, self.d, self.a, self.b, self.c, self.m[11] + self.SQRT3, 9)
        self.step(self.H, self.c, self.d, self.a, self.b, self.m[7] + self.SQRT3, 11)
        self.step(self.H, self.b, self.c, self.d, self.a, self.m[15] + self.SQRT3, 15)
        
        self.a += self.INITA
        self.b += self.INITB
        self.c += self.INITC
        self.d += self.INITD
        if self.verbose:
            print(self)
    
    def rcompute(self, H):
        self.cnt = 48
        self.a, self.b, self.c, self.d = self.HtoABCD(H)
        
        if self.verbose:
            print(self)
        
        self.d -= self.INITD
        self.c -= self.INITC
        self.b -= self.INITB
        self.a -= self.INITA
        
        if self.verbose:
            print(self)
        
        # Reverse Round 3
        self.rstep(self.H, self.b, self.c, self.d, self.a, self.m[15] + self.SQRT3, 15)
        self.rstep(self.H, self.c, self.d, self.a, self.b, self.m[7] + self.SQRT3, 11)
        self.rstep(self.H, self.d, self.a, self.b, self.c, self.m[11] + self.SQRT3, 9)
        self.rstep(self.H, self.a, self.b, self.c, self.d, self.m[3] + self.SQRT3, 3)
        self.rstep(self.H, self.b, self.c, self.d, self.a, self.m[13] + self.SQRT3, 15)
        self.rstep(self.H, self.c, self.d, self.a, self.b, self.m[5] + self.SQRT3, 11)
        self.rstep(self.H, self.d, self.a, self.b, self.c, self.m[9] + self.SQRT3, 9)
        self.rstep(self.H, self.a, self.b, self.c, self.d, self.m[1] + self.SQRT3, 3)
        self.rstep(self.H, self.b, self.c, self.d, self.a, self.m[14] + self.SQRT3, 15)
        self.rstep(self.H, self.c, self.d, self.a, self.b, self.m[6] + self.SQRT3, 11)
        self.rstep(self.H, self.d, self.a, self.b, self.c, self.m[10] + self.SQRT3, 9)
        self.rstep(self.H, self.a, self.b, self.c, self.d, self.m[2] + self.SQRT3, 3)
        self.rstep(self.H, self.b, self.c, self.d, self.a, self.m[12] + self.SQRT3, 15)
        self.rstep(self.H, self.c, self.d, self.a, self.b, self.m[4] + self.SQRT3, 11)
        self.rstep(self.H, self.d, self.a, self.b, self.c, self.m[8] + self.SQRT3, 9)
        self.rstep(self.H, self.a, self.b, self.c, self.d, self.m[0] + self.SQRT3, 3)
        
        # Reverse Round 2
        self.rstep(self.G, self.b, self.c, self.d, self.a, self.m[15] + self.SQRT2, 13)
        self.rstep(self.G, self.c, self.d, self.a, self.b, self.m[11] + self.SQRT2, 9)
        self.rstep(self.G, self.d, self.a, self.b, self.c, self.m[7] + self.SQRT2, 5)
        self.rstep(self.G, self.a, self.b, self.c, self.d, self.m[3] + self.SQRT2, 3)
        self.rstep(self.G, self.b, self.c, self.d, self.a, self.m[14] + self.SQRT2, 13)
        self.rstep(self.G, self.c, self.d, self.a, self.b, self.m[10] + self.SQRT2, 9)
        self.rstep(self.G, self.d, self.a, self.b, self.c, self.m[6] + self.SQRT2, 5)
        self.rstep(self.G, self.a, self.b, self.c, self.d, self.m[2] + self.SQRT2, 3)
        self.rstep(self.G, self.b, self.c, self.d, self.a, self.m[13] + self.SQRT2, 13)
        self.rstep(self.G, self.c, self.d, self.a, self.b, self.m[9] + self.SQRT2, 9)
        self.rstep(self.G, self.d, self.a, self.b, self.c, self.m[5] + self.SQRT2, 5)
        self.rstep(self.G, self.a, self.b, self.c, self.d, self.m[1] + self.SQRT2, 3)
        self.rstep(self.G, self.b, self.c, self.d, self.a, self.m[12] + self.SQRT2, 13)
        self.rstep(self.G, self.c, self.d, self.a, self.b, self.m[8] + self.SQRT2, 9)
        self.rstep(self.G, self.d, self.a, self.b, self.c, self.m[4] + self.SQRT2, 5)
        self.rstep(self.G, self.a, self.b, self.c, self.d, self.m[0] + self.SQRT2, 3)
        
        # Reverse Round 1
        self.rstep(self.F, self.b, self.c, self.d, self.a, self.m[15], 19)
        self.rstep(self.F, self.c, self.d, self.a, self.b, self.m[14], 11)
        self.rstep(self.F, self.d, self.a, self.b, self.c, self.m[13], 7)
        self.rstep(self.F, self.a, self.b, self.c, self.d, self.m[12], 3)
        self.rstep(self.F, self.b, self.c, self.d, self.a, self.m[11], 19)
        self.rstep(self.F, self.c, self.d, self.a, self.b, self.m[10], 11)
        self.rstep(self.F, self.d, self.a, self.b, self.c, self.m[9], 7)
        self.rstep(self.F, self.a, self.b, self.c, self.d, self.m[8], 3)
        self.rstep(self.F, self.b, self.c, self.d, self.a, self.m[7], 19)
        self.rstep(self.F, self.c, self.d, self.a, self.b, self.m[6], 11)
        self.rstep(self.F, self.d, self.a, self.b, self.c, self.m[5], 7)
        self.rstep(self.F, self.a, self.b, self.c, self.d, self.m[4], 3)
        self.rstep(self.F, self.b, self.c, self.d, self.a, self.m[3], 19)
        self.rstep(self.F, self.c, self.d, self.a, self.b, self.m[2], 11)
        self.rstep(self.F, self.d, self.a, self.b, self.c, self.m[1], 7)
        self.rstep(self.F, self.a, self.b, self.c, self.d, self.m[0], 3)
    
    def __str__(self):
        A = self.a.toHex()
        B = self.b.toHex()
        C = self.c.toHex()
        D = self.d.toHex()
        return A+B+C+D
    
    def HtoABCD(self, H=None):
        if H is None:
            A = byte([symbols(var) for i in range(32,0,-8) for j in range(8) for var in ["a_{}".format((i-8)+j)]])
            B = byte([symbols(var) for i in range(32,0,-8) for j in range(8) for var in ["b_{}".format((i-8)+j)]])
            C = byte([symbols(var) for i in range(32,0,-8) for j in range(8) for var in ["c_{}".format((i-8)+j)]])
            D = byte([symbols(var) for i in range(32,0,-8) for j in range(8) for var in ["d_{}".format((i-8)+j)]])
        else:
            ha = '{:032b}'.format(int(H[0:8],16))
            A = byte([true if ha[(i-8)+j] == '1' else false for i in range(32,0,-8) for j in range(8) ])
            hb = '{:032b}'.format(int(H[8:16],16))
            B = byte([true if hb[(i-8)+j] == '1' else false for i in range(32,0,-8) for j in range(8) ])
            hc = '{:032b}'.format(int(H[16:24],16))
            C = byte([true if hc[(i-8)+j] == '1' else false for i in range(32,0,-8) for j in range(8) ])
            hd = '{:032b}'.format(int(H[24:32],16))
            D = byte([true if hd[(i-8)+j] == '1' else false for i in range(32,0,-8) for j in range(8) ])
        return A,B,C,D
    
    def solve(self, H):
        A,B,C,D = self.HtoABCD(H)
        eq = And(*[~(A[i] ^ self.a[i]) & ~(B[i] ^ self.b[i]) & ~(C[i] ^ self.c[i]) & ~(D[i] ^ self.d[i]) for i in range(32)])
        print("Equation:",eq)
        return satisfiable(eq)
    
    def rsolve(self):
        A = self.INITA.copy()
        B = self.INITB.copy()
        C = self.INITC.copy()
        D = self.INITD.copy()
        eq = And(*[~(A[i] ^ self.a[i]) & ~(B[i] ^ self.b[i]) & ~(C[i] ^ self.c[i]) & ~(D[i] ^ self.d[i]) for i in range(32)])
        print("Equation:",eq)
        return satisfiable(eq)
    
    def subs(self, variables):
        self.a.subs(variables)
        self.b.subs(variables)
        self.c.subs(variables)
        self.d.subs(variables)
        for i in range(16):
            self.m[i].subs(variables)
    
    def getpass(self):
        length = self.m[14].toInt()
        length += self.m[15].toInt() << 32
        length //= 16
        res = ""
        for i in range(length):
            b = self.m[i//2]
            c = 0
            e = 1
            for j in range(8):
                if b[31-(j+16*(i%2))] == True:
                    c += e
                e *= 2
            res += "{:c}".format(c)
        return res

if __name__ == '__main__':
    parser = OptionParser()
    parser.add_option("-l", "--length", dest="length", default=-1, type="int",
                  help="Password length", metavar="LEN")
    parser.add_option("-H", "--hash", dest="H", default=None,
                  help="Hash", metavar="HASH")
    parser.add_option("-p", "--password", dest="password", default=None,
                  help="Password", metavar="PASS")
    parser.add_option("-r", "--reverse",
                  action="store_true", dest="reverse", default=False,
                  help="Start from the hash and compute backward")
    parser.add_option("-v", "--verbose",
                  action="store_true", dest="verbose", default=False,
                  help="Verbose mode")
    (options, args) = parser.parse_args()
    s = {}
    # Assuming UTF16 with only 8bits used
    for i in range(1,56,2):
        for j in range(8):
            s['m_{}'.format(i*8+j)] = false
    # msg size < 448, which is only 9bits out of 64. And it's a multiple of 16.
    for i in range(464,512):
        s['m_{}'.format(i)] = false
    for i in range(452,463):
        s['m_{}'.format(i)] = false
    if options.password is not None:
        options.length = len(options.password)
    if options.length >= 0:
        for i in range(0,options.length*2,2):
            s['m_{}'.format(i*8)] = false # only 7bits
        s['m_{}'.format(options.length*2*8)] = true
        for i in range(options.length*2*8+1,448,1):
            s['m_{}'.format(i)] = false
        for i in range(4):
            s['m_{}'.format(451-i)] = true if (options.length>>i)&1 else false
        s['m_463'] = true if (options.length > 15) else false
    else:
        #TODO: Find a boolean formula to link the size and the other bits
        pass
    if options.password is not None:
        i=0
        for c in options.password:
            bits = '{:08b}'.format(ord(c))
            for j in range(8):
                s['m_{}'.format(i*2*8+j)] = true if bits[j] == '1' else false
            i+=1 
    print("Guessed {} bits out of 512".format(len(s)))
    M = [byte([symbols(var) if var not in s else s[var] for j1 in range(32,0,-8) for j2 in range(8) for j in [(j1-8)+j2] for var in ["m_{}".format(i*32+j)]]) for i in range(16)]
    MD4 = md4(M, verbose=options.verbose)
    if options.reverse:
        MD4.rcompute(options.H)
        sol = MD4.rsolve()
    else:
        MD4.compute()
        sol = MD4.solve(options.H)
    MD4.subs(sol)
    print("Hash:",MD4)
    print("Pass:",MD4.getpass())
    print("Message:",*[MD4.m[i].toHex() for i in range(16)])
    
