#ifndef KNOB_H
#define KNOB_H

#include <QQueue>
#include <QPair>
#include <QDial>
#include <QVBoxLayout>
#include "liquidsfz.hh"

class Knob : public QVBoxLayout
{
    Q_OBJECT
public:
    Knob(QQueue<QPair<int, int>>* cc_queue, const LiquidSFZ::CCInfo &cc_info, QWidget* parent = nullptr);

protected slots:
    void onValueChanged(int val);

private:
    QQueue<QPair<int, int>>* queue = nullptr;
    int cc = 0;
};

#endif // KNOB_H
