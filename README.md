# Lagarith
Reification refactor and port of Lagarith codec as obtained from https://lags.leetcode.net/codec.html

This project was undertaken specifically to suit our needs, but may be useful for someone else.

Lagarith codec class isolated from system-specific codec driver implementation.
Refactored for portability.
Non-RGB colorspace/transcoding functionality eliminated.
Limited Lagarith-encoded AVI i/o class added.
Testing framework and unit tests added.
Now builds for Windows x64 and Android ARM64.

ARM64 port utilizes JRatcliff's SSE2NEON.h
https://github.com/jratcliff63367/sse2neon

To build for android this version of cmake is required: 
https://github.com/Reification/CMake/releases/tag/v3.11.1-reification.2

