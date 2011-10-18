#ifndef ENGINEBUFFERCONTROL_H
#define ENGINEBUFFERCONTROL_H

#include <QObject>
#include <QScopedPointer>
#include <QTime>

#include "util/fifo.h"
#include "controlobject.h"

class CallbackControlManager;

struct ControlUpdate {
    ControlUpdate()
            : time(0),
              value(0) {
    }
    ControlUpdate(double time, double value)
            : time(time),
              value(value) {
    }
    double time;
    double value;
};

struct CallbackControlMessage {
    ControlObject* control;
    ControlUpdate update;
};

class ControlWatcher : public QObject {
    Q_OBJECT
  public:
    ControlWatcher(ControlObject* pControl);
    virtual ~ControlWatcher();
  signals:
    void controlUpdated(ControlObject* pControl, double value);
  private slots:
    void slotValueChanged(double v);
  private:
    ControlObject* m_pControl;
};

class CallbackControl : public QObject {
    Q_OBJECT
  public:
    // Create a CallbackControl wrapper around pControl. Takes ownership of
    // pControl.
    CallbackControl(CallbackControlManager* pControlManager,
                    ControlObject* pControl, int bufferLength);
    virtual ~CallbackControl();

    // Get access to the internal control. WARNING: Touching this control is NOT
    // callback-safe.
    ControlObject* getControl() const {
        return m_control.data();
    }

    ConfigKey getKey() const {
        return m_control->getKey();
    }

    double get() const;
    void set(double value);
    void add(double value);
    void sub(double value);

    // Receive update from CallbackControlManager
    void receiveUpdate(ControlUpdate update);

  signals:
    // Callback-safe signal that a control value is changed. Always emitted from
    // the callback thread. WARNING: always connect to this signal with
    // Qt::DirectConnection or the slot WILL NOT be callback-safe.
    void valueChanged(double value);
    void valueChanged(double value, double time);
    void valueChangedFromEngine(double value);
    void valueChangedFromEngine(double value, double time);
    void controlUpdatedFromCallback(CallbackControl* pControl, double value);

  private:
    ControlUpdate m_value;
    FIFO<ControlUpdate> m_updates;
    QScopedPointer<ControlObject> m_control;
    ControlWatcher m_controlWatcher;
};

class CallbackControlManager : public QObject {
    Q_OBJECT
  public:
    explicit CallbackControlManager();
    virtual ~CallbackControlManager();

    // Add a control to be managed by CallbackControlManager. Returns a
    // CallbackControl. MUST ONLY BE CALLED DURING MIXXX INITIALIZATION.
    CallbackControl* addControl(ControlObject* pControl, int bufferLength);

    // Get a CallbackControl for the given ControlObject
    CallbackControl* getControl(ConfigKey key);

    void callbackProcessIncomingUpdates();
    void callbackProcessOutgoingUpdates();
    void processOutgoingUpdates();

  public slots:
    // Thread Safe but NOT callback-safe. Indicates that the control 'pControl'
    // was just updated to value 'value'
    void slotControlUpdated(ControlObject* pControl, double value);

    // Callback-safe.
    void slotControlUpdatedFromCallback(CallbackControl* pControl, double value);

  private:

    ////////////////////////////////////////////////////////////////////////////
    // The following may only be called/touched while m_incomingUpdateMutex is
    // held.
    ////////////////////////////////////////////////////////////////////////////

    double getTime() {
        // So stupid, but we can't trust QTime to give us accurate long-term
        // times since if somebody changes the system clock while running Mixxx,
        // the results is basically undefined. Restart it every time we get the
        // time and keep a tally.
        double elapsed = m_timer.restart();
        m_lastTimeValue += elapsed;
        return m_lastTimeValue;
    }
    QMutex m_incomingUpdateMutex;
    FIFO<CallbackControlMessage> m_incomingUpdateFifo;


    // TODO(XXX) Switch to QElapsedTimer once we require Qt >=4.7
    QTime m_timer;
    double m_lastTimeValue;

    ////////////////////////////////////////////////////////////////////////////
    // The following may only be touched from within the callback
    ////////////////////////////////////////////////////////////////////////////

    QHash<ConfigKey, CallbackControl*> m_callbackControls;
    QSet<ConfigKey> m_updatedControls;
    FIFO<CallbackControlMessage> m_outgoingUpdateFifo;
};

#endif /* ENGINEBUFFERCONTROL_H */
