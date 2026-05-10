// Object.cpp
#include "Object.h"

std::string ObjFunction::toString() const {
    if (name.empty()) return "<script>";
    return "<fn " + name + ">";
}

std::string ObjClosure::toString() const {
    return function ? function->toString() : "<closure>";
}