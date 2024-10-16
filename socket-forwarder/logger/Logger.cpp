#include "Logger.h"

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/patternlayout.h>

namespace forwarder
{
    void initialiseConsoleLogger()
    {
        log4cxx::PatternLayoutPtr patternLayout = std::make_shared<log4cxx::PatternLayout>();
        patternLayout->setConversionPattern("%d{yyyy-MM-dd HH:mm:ss} [%-5p] %c{1}:%L - %m%n");
        log4cxx::ConsoleAppenderPtr consoleAppender = std::make_shared<log4cxx::ConsoleAppender>(patternLayout);
        log4cxx::BasicConfigurator::configure(consoleAppender);
    }
}
