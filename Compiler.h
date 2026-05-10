#pragma once

#include <memory>
#include <string>
#include "Object.h"

std::shared_ptr<ObjFunction> compile(const std::string& source);