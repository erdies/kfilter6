#include "driver.h"

#include <QTextStream>

int main()
{
    driver d;
    d.SetTitle(QStringLiteral("Qt6 driver smoke test"));
    d.setRdc(5.6);
    d.setF0(300.0);
    d.Berechneparameter();
    d.Schall();
    d.Impedanz();

    QTextStream out(stdout);
    out << d.GetTitle() << '\n';
    out << "Rdc=" << d.getRdc() << '\n';
    out << "Sound active=" << d.PressureisActive << '\n';
    out << "Impedance[0]=" << d.ResultImpedanz[0] << '\n';
    return 0;
}
