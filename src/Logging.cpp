#include "Logging.h"

#include <QLoggingCategory>

namespace rocketplot
{

Q_LOGGING_CATEGORY(lcRender, "rocketplot.render", QtWarningMsg)
Q_LOGGING_CATEGORY(lcInput, "rocketplot.input", QtWarningMsg)
Q_LOGGING_CATEGORY(lcData, "rocketplot.data", QtWarningMsg)

}  // namespace rocketplot
