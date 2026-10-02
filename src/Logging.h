#pragma once

#include <QLoggingCategory>

namespace rocketplot
{

// Debug output, off by default. Turn on with QT_LOGGING_RULES="rocketplot.*.debug=true".
Q_DECLARE_LOGGING_CATEGORY(lcRender)  // rocketplot.render: layout and per-frame decimation
Q_DECLARE_LOGGING_CATEGORY(lcInput)   // rocketplot.input: pan, zoom, reset
Q_DECLARE_LOGGING_CATEGORY(lcData)    // rocketplot.data: data set or appended, rejected arguments

}  // namespace rocketplot
