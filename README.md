# UW library

## Achtung!

AI-generated stuff. Do not use unless you're a dog.

This software is full of fleas, meow.

## WTF

Yet another variant type for C.

Initial purpose: parsers.

## Building

The following environment variables are honoured by cmake:

* `DEBUG`: debug build (XXX not fully implementeded in cmake yet)
* `UW_WITHOUT_ICU`: if defined (the value does not matter), build without ICU dependency
* `UW_TYPE_BITWIDTH`: number of bits for type id, default is 8
* `UW_INTERFACE_BITWIDTH`: number of bits for interface id, default is 8
* `UW_TYPE_CAPACITY`: the maximum number of types, default is (1 << UW_TYPE_BITWIDTH)
* `UW_INTERFACE_CAPACITY`: the maximum number of interfaces, default is (1 << UW_INTERFACE_BITWIDTH)
