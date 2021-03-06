/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * Contains built-in functions for Maths
 * ----------------------------------------------------------------------------
 */
#include "jsutils.h"
#include "jsvar.h"

#ifdef ARM
#include "mconf.h"
#include "protos.h"
#else
#include <math.h>
#endif

#define PI (3.141592653589793)


JsVarInt jswrap_integer_valueOf(JsVar *v);
JsVarFloat jswrap_math_abs(JsVarFloat x);
double jswrap_math_pow(double x, double y);
double jswrap_math_sqrt(double x);
JsVarFloat jswrap_math_clip(JsVarFloat x, JsVarFloat min, JsVarFloat max);
JsVarFloat jswrap_math_minmax(JsVar *args, bool isMax);
