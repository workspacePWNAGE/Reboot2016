#pragma once
#include <windows.h>
#include <functional>
#include "AccountTest.h"
#include "v8tree/Instance.h"
static const UINT OriginalKeyboardMessage=WM_APP+77;
void installOriginalLoginBridge(RBX::Instance* parent,LocalAccountSession* account,bool external);
