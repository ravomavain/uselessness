# Uselessness

That's where I'll upload few code samples I made, most of them are useless though.

## Fibonacci irc bot (fibirc.vala)

This simple irc bot writen in vala connect to an irc channel and reply to '!fib n' commands with the nth Fibonacci number.

Compile with :

    valac fibirc.vala --pkg gee-1.0 --pkg gio-2.0

*It require valac compiler.*

Usage :

    ./ircbot --help

## Python sha1 implementation (sha1.py)

This is a python implementation of the sha1 algorithm, was originaly done for my brother who needed to do it as a programation homework (but since I finished it a bit too late -aka. after midnight- it was never used).
If you are looking for a fast and efficient implementation of sha1, you'd better to use the buildin hashlib's implementation, which is much faster.

Usage:

    ./sha1.py "string to hash"

## Huffman (huffman/*)

This is the ANSI C version of the Huffan decompression algorithm from [teeworlds' C++ version](https://github.com/oy/teeworlds/blob/master/src/engine/shared/huffman.cpp).
It's not a full implementation of the Huffman compression algorithm and was only created for [fisted's wireshark tw dissector](https://github.com/fisted/wireshark/tree/tw-dissect)

Can be compiled with:

    gcc -ansi -pedantic -Wall -fPIC *.c -o huffman

Also work with less strict rules:

    gcc *.c -o huffman

*main.c is only a test program*

## bin2blob & blob2bin

PHP scripts to convert sql blob data (0xAF49...) to binary data and vice versa.

Both scripts can be replaced by inline php commands:

* bin2blob:

    php -r 'echo "0x".bin2hex(file_get_contents("php://stdin"));'

* blob2bin:

    php -r 'echo pack("H*" , preg_replace(array("/^0x/i","/[^0-9A-F]/i"),"",file_get_contents("php://stdin")));'

