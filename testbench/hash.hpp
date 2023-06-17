
#pragma once
static inline 
unsigned int hashSample(int a, int b) {
    return a^b;
}

static inline 
unsigned int myhash(int a, int b) {
    return hashSample(a, b);
}