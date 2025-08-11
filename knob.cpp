#include "knob.h"
#include <QLabel>

Knob::Knob(QQueue<QPair<int, int>>* cc_queue, const LiquidSFZ::CCInfo& cc_info, QWidget *parent): QVBoxLayout(parent)
{
    this->queue = cc_queue;
    this->cc = cc_info.cc();

    setAlignment(Qt::AlignHCenter);

    QDial* dial = new QDial(parent);
    addWidget(dial);

    QLabel* label = new QLabel(parent);
    label->setAlignment(Qt::AlignHCenter);
    addWidget(label);

    if (cc_info.has_label()) {
        label->setText(QString::fromStdString(cc_info.label()));
    } else {
        label->setText(QString::number(cc_info.cc()));
    }

    dial->setMaximum(127);
    dial->setMinimum(0);
    dial->setValue(cc_info.default_value());
    connect(dial, &QDial::valueChanged, this, &Knob::onValueChanged);
}

void Knob::onValueChanged(int val) {
    queue->enqueue(QPair<int, int>(cc, val));
}
