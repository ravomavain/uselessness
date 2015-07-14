#!/usr/bin/env python
# -*- coding: utf-8 -*-

from distutils.util import strtobool

def ask(question, default=True):
    prompt = question
    if default == None:
        prompt += ' [y/n] '
    elif default:
        prompt += ' [Y/n] '
    else:
        prompt += ' [y/N] '
    while True:
        try:
            ans = input(prompt).strip()
            if ans == '' and default != None:
                return default
            return bool(strtobool(ans))
        except ValueError:
            print('Please respond with \'y\' or \'n\'.\n')

