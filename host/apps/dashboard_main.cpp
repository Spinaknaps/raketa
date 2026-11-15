#include "dashboard_session.h"

#include <QApplication>
#include <tcl.h>

int main(int argc, char *argv[])
{
    Tcl_FindExecutable(argv[0]);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("raketa"));
    int exitCode;
    {
        DashboardSession session;
        exitCode = session.run();
    }
    Tcl_Finalize();
    return exitCode;
}
