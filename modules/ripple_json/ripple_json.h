//------------------------------------------------------------------------------
/*
    Copyright (c) 2011-2013, OpenCoin, Inc.
*/
//==============================================================================

/** Include this to get the @ref ripple_json module.

    @file ripple_json.h
    @ingroup ripple_json
*/

/** JSON parsiing and output support.

    A simple set of JSON manipulation classes.

    @defgroup ripple_json
*/

#ifndef RIPPLE_JSON_RIPPLEHEADER
#define RIPPLE_JSON_RIPPLEHEADER

#include "../modules/ripple_basics/ripple_basics.h"

#include "json/json_config.h" // Needed before these cpptl includes

#ifndef JSON_USE_CPPTL_SMALLMAP
# include <map>
#else
# include <cpptl/smallmap.h>
#endif

#ifdef JSON_USE_CPPTL
# include <cpptl/forwards.h>
#endif

namespace ripple
{

#include "json/json_forwards.h"
#include "json/json_features.h"
#include "json/json_value.h"
#include "json/json_reader.h"
#include "json/json_writer.h"

}

#endif