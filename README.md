## Introduction

The present codebase contains work in progress on a toy compiler for a language
that ultimately is to hold the middle somewhere between Pascal and Oberon. Its
primary purpose is to help me learn about X86-64 assembly and backend compiler
techniques.

## Current state

This project is in an early stage, at present supporting only an empty
program declaration; e.g.,
```
Program foo;
Begin
End.
```

## Requirements

* GCC or Clang (C99-compatible)
* GNU Make
* NASM

## Installation and Use

Clone or download the sources. E.g,
```
wget https://github.com/arnobastenhof/oberon/archive/master.zip
unzip master.zip
```
To compile, type the following command from the project root,
```
make all
```
This creates an executable `smc` in the directory `build`. Run it by passing in
the source file as a command-line argument; e.g.,
```
build/smc test.src
```
A file `out.s` will be created from which an executable may be obtained using
`nasm` and `ld`:
```
nasm -f elf64 out.s
ld out.o
```

## TODO's

Some of the very next tasks I intend to work on:

* Parse constant expressions.
* Have the user specify a destination file as a command-line argument, writing
  to stdout otherwise.
